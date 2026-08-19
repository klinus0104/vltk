#include "relay_store.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::size_t kTableBytes = 0x58bc;
constexpr std::size_t kTableWords = 0x162f;
constexpr std::uint32_t kCipherAdd = 0x2e6d23c1;
constexpr std::size_t kMaxFrame = 0x2000;
constexpr std::size_t kMaxInputBuffer = 0x800000;
// Stable wire opcodes. Keep the numeric values for protocol compatibility;
// these names describe the native Windows Relay message families.
constexpr std::uint8_t kOpcodeAccountLogin = 0x21;
constexpr std::uint8_t kOpcodeGatewayInfo = 0x26;

struct Config {
    std::string bind = "0.0.0.0";
    int port = 7777;
    bool allow_wan = false;
    RelayConfig relay_store;
};

struct KeyState {
    std::uint32_t k1 = 0;
    std::uint32_t k2 = 0;
};

struct Client {
    int fd = -1;
    std::uint32_t id = 0;
    bool verified = false;
    bool close_after_write = false;
    std::uint32_t route_id = 0;
    std::uint32_t peer_id = 0;
    std::uint16_t server_port = 0;
    std::string peer_ip;
    std::string account;
    std::string server_name;
    KeyState keys;
    std::vector<std::uint8_t> in;
    std::vector<std::uint8_t> out;
};

struct ServerState {
    std::unique_ptr<RelayStore> relay_store;
    std::unordered_map<std::uint32_t, int> route_owner_fd;
    std::uint16_t service_port = 0;
};

std::string trim(std::string s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), is_space));
    s.erase(std::find_if_not(s.rbegin(), s.rend(), is_space).base(), s.end());
    return s;
}

bool parse_bool(const std::string& value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return std::tolower(c); });
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

std::map<std::string, std::map<std::string, std::string>> read_ini(const std::string& path) {
    std::ifstream in(path);
    std::map<std::string, std::map<std::string, std::string>> result;
    if (!in) return result;

    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        result[section][trim(line.substr(0, pos))] = trim(line.substr(pos + 1));
    }
    return result;
}

Config load_config(const std::string& root) {
    Config cfg;
    auto relay = read_ini(root + "/Relay_Setup.ini");
    if (relay.empty()) {
        std::cerr << "Relay_Setup.ini not found, using defaults\n";
    } else if (relay.count("Local")) {
        auto& local = relay["Local"];
        if (local.count("bind")) cfg.bind = local["bind"];
        if (local.count("port")) cfg.port = std::stoi(local["port"]);
        if (local.count("allow_wan")) cfg.allow_wan = parse_bool(local["allow_wan"]);
        if (local.count("backend")) cfg.relay_store.backend = local["backend"];
    }

    auto db = read_ini(root + "/mssql.ini");
    auto read_db = [&](const std::string& key, std::string& target) {
        if (db.count("") && db[""].count(key)) target = db[""][key];
        if (db.count("mssql") && db["mssql"].count(key)) target = db["mssql"][key];
        if (db.count("MSSQL") && db["MSSQL"].count(key)) target = db["MSSQL"][key];
    };
    read_db("server", cfg.relay_store.db_server);
    read_db("user", cfg.relay_store.db_user);
    read_db("password", cfg.relay_store.db_password);
    read_db("database", cfg.relay_store.db_name);
    read_db("charset", cfg.relay_store.db_charset);
    return cfg;
}

std::vector<std::uint32_t> load_heaven_table(const std::string& root) {
    std::vector<std::string> paths = {
        root + "/reference/heaven_table.bin",
        root + "/heaven_table.bin",
    };

    std::ifstream in;
    for (const auto& path : paths) {
        in.open(path, std::ios::binary);
        if (in) break;
        in.clear();
    }
    if (!in) throw std::runtime_error("heaven_table.bin not found");

    std::vector<std::uint8_t> bytes(kTableBytes);
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (in.gcount() != static_cast<std::streamsize>(kTableBytes)) {
        throw std::runtime_error("heaven_table.bin has invalid size");
    }

    std::vector<std::uint32_t> table(kTableWords);
    for (std::size_t i = 0; i < kTableWords; ++i) {
        table[i] = static_cast<std::uint32_t>(bytes[i * 4]) |
                   (static_cast<std::uint32_t>(bytes[i * 4 + 1]) << 8) |
                   (static_cast<std::uint32_t>(bytes[i * 4 + 2]) << 16) |
                   (static_cast<std::uint32_t>(bytes[i * 4 + 3]) << 24);
    }
    return table;
}

bool allowed_ipv4(std::uint32_t ip_host_order) {
    std::uint8_t a = static_cast<std::uint8_t>((ip_host_order >> 24) & 0xff);
    std::uint8_t b = static_cast<std::uint8_t>((ip_host_order >> 16) & 0xff);
    if (a == 10) return true;
    if (a == 172 && b >= 16 && b <= 31) return true;
    if (a == 192 && b == 168) return true;
    if (a == 127) return true;
    if (a == 169 && b == 254) return true;
    return false;
}

bool machine_has_wan_ipv4() {
    ifaddrs* addrs = nullptr;
    if (getifaddrs(&addrs) != 0) throw std::runtime_error("getifaddrs failed");

    bool has_wan = false;
    for (ifaddrs* it = addrs; it != nullptr; it = it->ifa_next) {
        if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
        auto* sin = reinterpret_cast<sockaddr_in*>(it->ifa_addr);
        std::uint32_t ip = ntohl(sin->sin_addr.s_addr);
        if (!allowed_ipv4(ip)) {
            has_wan = true;
            break;
        }
    }
    freeifaddrs(addrs);
    return has_wan;
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) throw std::runtime_error("fcntl(F_GETFL) failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::runtime_error("fcntl(F_SETFL) failed");
    }
}

void heaven_cipher(const std::vector<std::uint32_t>& table, std::uint8_t* data, std::size_t len, std::uint32_t seed) {
    std::size_t words = len >> 2;
    std::size_t rem = len & 3;
    std::uint32_t key_state = seed;

    for (std::uint32_t i = 0; i < words; ++i) {
        std::uint32_t last = static_cast<std::uint32_t>(words - 1);
        std::uint32_t idx = (last - i + key_state) % static_cast<std::uint32_t>(kTableWords);
        std::uint32_t key = table[idx] + kCipherAdd;
        auto* word = reinterpret_cast<std::uint32_t*>(data + i * 4);
        *word ^= key;
        key_state = key;
    }

    if (rem != 0) {
        std::uint32_t key = key_state ^ table[rem];
        std::size_t base = len & ~std::size_t{3};
        data[base] ^= key & 0xff;
        if (rem >= 2) data[base + 1] ^= (key >> 8) & 0xff;
        if (rem == 3) data[base + 2] ^= (key >> 16) & 0xff;
    }
}

std::uint32_t handshake_word(std::uint32_t k) {
    std::uint32_t mixed = __builtin_bswap32(k) & 0xff0000ffu;
    mixed |= k & 0x00ffff00u;
    return (0x2e6d2398u - mixed) ^ 0x2e6d23cfu;
}

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& data, std::size_t off) {
    if (off + 2 > data.size()) return 0;
    return static_cast<std::uint16_t>(data[off] | (data[off + 1] << 8));
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& data, std::size_t off) {
    if (off + 4 > data.size()) return 0;
    return static_cast<std::uint32_t>(data[off]) |
           (static_cast<std::uint32_t>(data[off + 1]) << 8) |
           (static_cast<std::uint32_t>(data[off + 2]) << 16) |
           (static_cast<std::uint32_t>(data[off + 3]) << 24);
}

void write_u16_at(std::vector<std::uint8_t>& data, std::size_t off, std::uint16_t value) {
    if (off + 2 > data.size()) return;
    data[off] = static_cast<std::uint8_t>(value & 0xff);
    data[off + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

void write_u32_at(std::vector<std::uint8_t>& data, std::size_t off, std::uint32_t value) {
    if (off + 4 > data.size()) return;
    data[off] = static_cast<std::uint8_t>(value & 0xff);
    data[off + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
    data[off + 2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
    data[off + 3] = static_cast<std::uint8_t>((value >> 24) & 0xff);
}

std::string fixed_ascii(const std::vector<std::uint8_t>& data, std::size_t off, std::size_t len) {
    std::size_t end = off;
    while (end < off + len && end < data.size() && data[end] != 0) ++end;
    return std::string(reinterpret_cast<const char*>(data.data() + off), end - off);
}

void queue_plain_frame(Client& client, const std::vector<std::uint8_t>& body) {
    append_u16(client.out, static_cast<std::uint16_t>(body.size() + 2));
    client.out.insert(client.out.end(), body.begin(), body.end());
}

void queue_encrypted_frame(Client& client, const std::vector<std::uint8_t>& body, const std::vector<std::uint32_t>& table) {
    std::vector<std::uint8_t> encrypted = body;
    heaven_cipher(table, encrypted.data(), encrypted.size(), client.keys.k2);
    queue_plain_frame(client, encrypted);
}

void queue_handshake(Client& client) {
    // libheaven expects a 44-byte total frame: two-byte length plus a
    // 42-byte body. The encoded receive/send keys live at body offsets 8/17.
    std::vector<std::uint8_t> body(0x2a, 0);
    write_u16_at(body, 0, 0x20);
    write_u32_at(body, 8, handshake_word(client.keys.k1));
    write_u32_at(body, 17, handshake_word(client.keys.k2));
    queue_plain_frame(client, body);
}

void queue_windows_ack(Client& client,
                       const std::vector<std::uint8_t>& body,
                       const std::vector<std::uint32_t>& table,
                       std::uint8_t opcode,
                       std::uint32_t status) {
    std::vector<std::uint8_t> reply(std::max<std::size_t>(0x34, 0x0e), 0);
    reply[0] = body.empty() ? 0 : body[0];
    reply[1] = opcode;
    write_u16_at(reply, 2, 0x32);
    write_u16_at(reply, 4, 0x04);
    write_u32_at(reply, 6, body.size() >= 10 ? read_u32(body, 6) : 0);
    write_u32_at(reply, 0x0a, status);
    queue_encrypted_frame(client, reply, table);
}

void queue_canonical_verify_ack(Client& client,
                                const std::vector<std::uint8_t>& body,
                                const std::vector<std::uint32_t>& table,
                                std::uint32_t status) {
    std::vector<std::uint8_t> reply(0x34, 0);
    reply[0] = 0x00;
    reply[1] = 0x24;
    write_u16_at(reply, 2, 0x32);
    write_u16_at(reply, 4, 0x01);
    write_u16_at(reply, 6, 0x04);
    write_u32_at(reply, 8, body.size() >= 12 ? read_u32(body, 8) : 0);
    if (body.size() >= 44) std::copy_n(body.begin() + 12, 32, reply.begin() + 12);
    write_u32_at(reply, 0x2c, status);
    // Windows S3RelayServer.exe emits this stable three-byte trailer in its
    // canonical 0x34-byte verify acknowledgement. The Linux reference leaves
    // these reserved bytes zeroed.
    reply[0x31] = 0x10;
    reply[0x32] = 0x8c;
    reply[0x33] = 0xff;
    queue_encrypted_frame(client, reply, table);
}

void queue_windows_data_ack(Client& client,
                            const std::vector<std::uint8_t>& body,
                            const std::vector<std::uint32_t>& table,
                            std::uint8_t opcode,
                            std::uint32_t status,
                            const std::string& payload) {
    std::vector<std::uint8_t> reply(std::max<std::size_t>(0x34, 0x0e + payload.size() + 1), 0);
    reply[0] = body.empty() ? 0 : body[0];
    reply[1] = opcode;
    write_u16_at(reply, 2, 0x32);
    write_u16_at(reply, 4, 0x04);
    write_u32_at(reply, 6, body.size() >= 10 ? read_u32(body, 6) : 0);
    write_u32_at(reply, 0x0a, status);
    std::copy(payload.begin(), payload.end(), reply.begin() + 0x0e);
    queue_encrypted_frame(client, reply, table);
}

bool forward_to_route(ServerState& state,
                      std::map<int, Client>& clients,
                      const std::vector<std::uint32_t>& table,
                      std::uint32_t route_id,
                      const std::vector<std::uint8_t>& body) {
    auto route = state.route_owner_fd.find(route_id);
    if (route == state.route_owner_fd.end()) return false;
    auto target = clients.find(route->second);
    if (target == clients.end()) return false;
    queue_encrypted_frame(target->second, body, table);
    return true;
}

void queue_windows_route_failure(Client& client,
                                 const std::vector<std::uint8_t>& body,
                                 const std::vector<std::uint32_t>& table) {
    std::vector<std::uint8_t> reply(body.size() + 0x14, 0);
    reply[0] = body[0];
    reply[1] = 0x23;
    write_u32_at(reply, 2, read_u32(body, 2));
    write_u32_at(reply, 6, read_u32(body, 6));
    write_u32_at(reply, 0x0e, 0xffffffffU);
    write_u16_at(reply, 0x12, static_cast<std::uint16_t>(body.size()));
    std::copy(body.begin(), body.end(), reply.begin() + 0x14);
    queue_encrypted_frame(client, reply, table);
}

void queue_route_announcement(Client& target,
                              const Client& announced,
                              std::uint16_t action,
                              const std::vector<std::uint32_t>& table) {
    std::vector<std::uint8_t> reply(0x32, 0);
    reply[0] = 0x00;
    reply[1] = 0x26;
    write_u16_at(reply, 2, 0x30);
    write_u16_at(reply, 4, 1);
    write_u16_at(reply, 6, 7);
    std::copy_n(announced.server_name.begin(),
                std::min<std::size_t>(announced.server_name.size(), 31),
                reply.begin() + 0x0c);
    write_u16_at(reply, 0x2c, action);
    in_addr peer_addr{};
    if (inet_pton(AF_INET, announced.peer_ip.c_str(), &peer_addr) == 1) {
        std::memcpy(reply.data() + 0x2e, &peer_addr, sizeof(peer_addr));
    }
    queue_encrypted_frame(target, reply, table);
}

bool handle_windows_verify(Client& client,
                           const std::vector<std::uint8_t>& body,
                           const std::vector<std::uint32_t>& table,
                           ServerState& state,
                           int fd,
                           std::map<int, Client>& clients) {
    // The actual libheaven body includes the leading 0x00/opcode pair. The
    // handler in the original executable receives a pointer two bytes later,
    // where its 0x7c-byte protocol payload begins.
    if (body.size() != 0x7e || body[1] != 0x24) return false;

    std::uint16_t payload_size = read_u16(body, 2);
    std::uint16_t version = read_u16(body, 4);
    std::uint16_t type = read_u16(body, 6);
    if (payload_size != 0x7c) {
        // A short/long frame is retained like the Windows parser; any
        // trailing bytes remain pending for a subsequent frame.
        return true;
    }
    if (version != 1 || type != 3) {
        if (state.relay_store->backend_name() != std::string("mssql")) {
            queue_canonical_verify_ack(client, body, table, 4);
            return true;
        }
        client.close_after_write = true;
        std::cerr << "#" << client.id << " windows relay verify failed version/type="
                  << version << "/" << type << "\n";
        return true;
    }

    std::uint32_t request_id = read_u32(body, 8);
    std::string name = fixed_ascii(body, 0x0c, 32);
    std::string password = fixed_ascii(body, 0x2c, 64);
    char identity_buffer[15]{};
    std::snprintf(identity_buffer, sizeof(identity_buffer), "%02X%02X-%02X%02X-%02X%02X",
                  body[0x70], body[0x71], body[0x72], body[0x73], body[0x74], body[0x75]);
    std::string identity(identity_buffer);
    ServerVerifyResult verify = state.relay_store->verify_server(
        name, password, client.peer_ip, state.service_port, identity);
    if (!verify.ok) {
        const bool windows_backend = state.relay_store->backend_name() == std::string("mssql");
        if (!verify.found) {
            if (!windows_backend) {
                queue_canonical_verify_ack(client, body, table, 4);
                return true;
            }
            std::cerr << "#" << client.id << " windows relay verify unknown server; no response\n";
            return true;
        }
        if (verify.found && verify.password_match && !verify.ip_match) {
            client.close_after_write = true;
            std::cerr << "#" << client.id
                      << " windows relay verify IP mismatch; closing without response peer="
                      << client.peer_ip << " expected=" << verify.ip << "\n";
            return true;
        }
        const bool identity_mismatch = verify.found && verify.password_match && !verify.identity_match;
        const std::uint32_t failure_status = windows_backend
                                                 ? (identity_mismatch ? 7 : (verify.password_match ? 4 : 3))
                                                 : 4;
        queue_canonical_verify_ack(client, body, table, failure_status);
        // The Windows binary terminates a verify session after credentials
        // are rejected (status 3), as well as after an identity mismatch.
        if (windows_backend && (identity_mismatch || !verify.password_match)) {
            client.close_after_write = true;
        }
        std::cerr << "#" << client.id << " windows relay verify failed server='" << name
                  << "' peer=" << client.peer_ip << " service_port=" << state.service_port
                  << " identity=" << identity << "\n";
        return true;
    }
    if (verify.id != 0 && state.route_owner_fd.count(verify.id) && state.route_owner_fd[verify.id] != fd) {
        const int owner_fd = state.route_owner_fd[verify.id];
        const bool windows_backend = state.relay_store->backend_name() == std::string("mssql");
        if (windows_backend) {
            auto owner = clients.find(owner_fd);
            if (owner != clients.end()) owner->second.close_after_write = true;
            client.close_after_write = true;
        }
        queue_canonical_verify_ack(client, body, table, windows_backend ? 4 : 6);
        std::cerr << "#" << client.id << " windows relay verify duplicate route_id=" << verify.id
                  << "; closing owner and contender\n";
        return true;
    }

    client.peer_id = request_id;
    client.server_name = name;
    client.account = client.server_name;
    client.server_port = verify.port;
    client.route_id = verify.id != 0 ? verify.id : (client.peer_id != 0 ? client.peer_id : client.id);
    client.verified = true;
    queue_canonical_verify_ack(client, body, table, 1);
    if (state.relay_store->backend_name() == std::string("mssql")) {
        for (auto& [other_fd, other] : clients) {
            if (other_fd == fd || !other.verified) continue;
            queue_route_announcement(other, client, 4, table);
            queue_route_announcement(client, other, 0x20, table);
        }
    }
    if (client.route_id != 0) state.route_owner_fd[client.route_id] = fd;
    std::cerr << "#" << client.id << " windows relay verify -> SUCCESS server='"
              << client.server_name << "' route_id=" << client.route_id
              << " port=" << client.server_port << "\n";
    return true;
}

bool require_windows_verify(Client& client,
                            const std::vector<std::uint8_t>& body,
                            const std::vector<std::uint32_t>& table,
                            const char* action,
                            bool close_unverified) {
    if (client.verified) return true;
    if (close_unverified) {
        client.close_after_write = true;
        std::cerr << "#" << client.id << " need Verify when " << action
                  << "; closing without response\n";
        return false;
    }
    queue_windows_ack(client, body, table, body.size() > 1 ? body[1] : 0, 7);
    std::cerr << "#" << client.id << " need Verify when " << action << "\n";
    return false;
}

bool handle_windows_logout(Client& client,
                           const std::vector<std::uint8_t>& body,
                           const std::vector<std::uint32_t>& table,
                           ServerState& state) {
    if (body[1] != 0x23) return false;
    if (!require_windows_verify(client, body, table, "c2s_accountlogout",
                                state.relay_store->backend_name() == std::string("mssql"))) return true;
    if (state.relay_store->backend_name() == std::string("mssql")) {
        if ((body.size() != 0x30 && body.size() != 0x2e) ||
            read_u16(body, 4) != 1 || read_u16(body, 6) != 0x0a) {
            std::cerr << "#" << client.id << " windows opcode 0x23 invalid len/subtype\n";
            return true;
        }

        const std::string account = fixed_ascii(body, 0x0c, 32);
        const bool ok = state.relay_store->mark_account_offline(account);
        std::vector<std::uint8_t> reply(0x30, 0);
        reply[0] = body[0];
        reply[1] = 0x23;
        write_u16_at(reply, 2, 0x2e);
        write_u16_at(reply, 4, 1);
        write_u16_at(reply, 6, 4);
        write_u32_at(reply, 8, read_u32(body, 8));
        std::copy_n(body.begin() + 0x0c, 32, reply.begin() + 0x0c);
        write_u32_at(reply, 0x2c, ok ? 1 : 2);
        queue_encrypted_frame(client, reply, table);
        std::cerr << "#" << client.id << " windows opcode 0x23 logout acc='"
                  << account << "' ok=" << (ok ? "yes" : "no") << "\n";
        return true;
    }

    // Preserve the reconstructed CSV harness layout.
    if ((body.size() != 0x2e && body.size() != 0x2c) ||
        read_u16(body, 2) != 1 || read_u16(body, 4) != 0x0a) {
        queue_windows_ack(client, body, table, 0x23, 4);
        return true;
    }
    const std::string account = fixed_ascii(body, 0x0a, std::min<std::size_t>(32, body.size() - 0x0a));
    queue_windows_ack(client, body, table, 0x23,
                      state.relay_store->mark_account_offline(account) ? 1 : 2);
    return true;
}

bool handle_windows_account_login(Client& client,
                                  const std::vector<std::uint8_t>& body,
                                  const std::vector<std::uint32_t>& table,
                                  ServerState& state) {
    if (body[1] != kOpcodeAccountLogin) return false;
    if (!require_windows_verify(client, body, table, "c2s_accountlogin",
                                state.relay_store->backend_name() == std::string("mssql"))) return true;

    if (state.relay_store->backend_name() == std::string("mssql")) {
        if (body.size() != 0xb0 || read_u16(body, 2) != 0xae || read_u16(body, 4) != 1) {
            std::cerr << "#" << client.id << " windows opcode 0x21 invalid len/subtype\n";
            return true;
        }

        const std::uint16_t subtype = read_u16(body, 6);
        const std::uint32_t request_id = read_u32(body, 8);
        const std::string account = fixed_ascii(body, 0x0c, 32);
        if (subtype == 2) {
            const std::string password = fixed_ascii(body, 0x2c, 64);
            const std::uint32_t user_ip = read_u32(body, 0x6c);
            const std::string secondary = fixed_ascii(body, 0x70, 64);
            const bool ok = state.relay_store->set_credentials(account, password, secondary, user_ip);
            std::vector<std::uint8_t> reply(0x48, 0);
            reply[0] = body[0];
            reply[1] = kOpcodeAccountLogin;
            write_u16_at(reply, 2, 0x46);
            write_u16_at(reply, 4, 1);
            write_u16_at(reply, 6, 9);
            write_u32_at(reply, 8, request_id);
            std::copy_n(body.begin() + 0x0c, 32, reply.begin() + 0x0c);
            write_u32_at(reply, 0x2c, ok ? 1 : 2);
            queue_encrypted_frame(client, reply, table);
            std::cerr << "#" << client.id << " windows opcode 0x21 set credentials acc='"
                      << account << "' ok=" << (ok ? "yes" : "no") << "\n";
            return true;
        }
        if (subtype == 12) {
            std::uint32_t user_ip = 0;
            const bool found = state.relay_store->get_account_user_ip(account, user_ip);
            if (found) {
                std::vector<std::uint8_t> reply(0x30, 0);
                reply[0] = body[0];
                reply[1] = kOpcodeAccountLogin;
                write_u16_at(reply, 2, 0x2e);
                write_u16_at(reply, 4, 1);
                write_u16_at(reply, 6, 12);
                write_u32_at(reply, 8, request_id);
                std::copy_n(body.begin() + 0x0c, 32, reply.begin() + 0x0c);
                write_u32_at(reply, 0x2c, user_ip);
                queue_encrypted_frame(client, reply, table);
            }
            std::cerr << "#" << client.id << " windows opcode 0x21 query IP acc='"
                      << account << "' found=" << (found ? "yes" : "no") << "\n";
            return true;
        }
        if (subtype != 8) {
            std::cerr << "#" << client.id << " windows opcode 0x21 unsupported subtype="
                      << subtype << "\n";
            return true;
        }

        std::string password;
        std::string secondary;
        std::uint32_t user_ip = 0;
        const bool found = state.relay_store->get_password(account, false, password) &&
                           state.relay_store->get_password(account, true, secondary) &&
                           state.relay_store->get_account_user_ip(account, user_ip);
        if (found) {
            std::vector<std::uint8_t> reply(0xb0, 0);
            reply[0] = body[0];
            reply[1] = kOpcodeAccountLogin;
            write_u16_at(reply, 2, 0xae);
            write_u16_at(reply, 4, 1);
            write_u16_at(reply, 6, 2);
            write_u32_at(reply, 8, request_id);
            std::copy_n(body.begin() + 0x0c, 32, reply.begin() + 0x0c);
            std::copy_n(password.begin(), std::min<std::size_t>(password.size(), 63), reply.begin() + 0x2c);
            write_u32_at(reply, 0x6c, user_ip);
            std::copy_n(secondary.begin(), std::min<std::size_t>(secondary.size(), 63), reply.begin() + 0x70);
            queue_encrypted_frame(client, reply, table);
        } else {
            // Windows leaves the tail of this error structure uninitialized;
            // emit deterministic zeroes while preserving its meaningful wire fields.
            std::vector<std::uint8_t> reply(0x48, 0);
            reply[0] = body[0];
            reply[1] = kOpcodeAccountLogin;
            write_u16_at(reply, 2, 0x46);
            write_u16_at(reply, 4, 1);
            write_u16_at(reply, 6, 9);
            write_u32_at(reply, 8, request_id);
            std::copy_n(body.begin() + 0x0c, 32, reply.begin() + 0x0c);
            write_u32_at(reply, 0x2c, 3);
            queue_encrypted_frame(client, reply, table);
        }
        std::cerr << "#" << client.id << " windows opcode 0x21 get credentials acc='"
                  << account << "' found=" << (found ? "yes" : "no") << "\n";
        return true;
    }

    // Preserve the reconstructed CSV harness protocol while Windows/MSSQL
    // fixtures use the native 0xb0-byte subtype layout above.
    if (body.size() <= 0x2a) {
        queue_windows_ack(client, body, table, kOpcodeAccountLogin, 4);
        return true;
    }
    std::string account = fixed_ascii(body, 0x0a, std::min<std::size_t>(32, body.size() - 0x0a));
    bool ok = state.relay_store->mark_account_online(account, client.route_id != 0 ? client.route_id : client.id, 0);
    queue_windows_ack(client, body, table, kOpcodeAccountLogin, ok ? 1 : 2);
    std::cerr << "#" << client.id << " windows accountlogin acc='" << account << "'\n";
    return true;
}

bool handle_windows_game_login(Client& client,
                               const std::vector<std::uint8_t>& body,
                               const std::vector<std::uint32_t>& table,
                               ServerState& state,
                               std::map<int, Client>& clients) {
    // The helper dispatcher places subtype before opcode (01 22), unlike the
    // normal 00 22 account-state packet below.
    if (body[0] == 1 && body[1] == 0x22) {
        if (!require_windows_verify(client, body, table, "c2s_gamelogin helper",
                                    state.relay_store->backend_name() == std::string("mssql"))) return true;
        const bool valid = body.size() >= 0x14 &&
                           static_cast<std::size_t>(read_u16(body, 0x10)) +
                                   read_u16(body, 0x12) + 0x14 == body.size();
        if (!valid) return true;

        std::vector<std::uint8_t> routed_body = body;
        if (read_u32(routed_body, 2) == 0) {
            write_u32_at(routed_body, 2, client.id);
            write_u32_at(routed_body, 6, client.route_id);
        }
        const std::uint16_t action = read_u16(body, 0x0e);
        if (action == 1 && body.size() >= 0x34) {
            const std::string account = fixed_ascii(body, 0x14, 32);
            std::uint32_t target_route = 0;
            const bool routed = state.relay_store->account_client_id(account, target_route) &&
                                forward_to_route(state, clients, table, target_route, routed_body);
            std::cerr << "#" << client.id << " windows opcode 0x22 helper route acc='"
                      << account << "' target_route=" << target_route
                      << " forwarded=" << (routed ? "yes" : "no") << "\n";
            if (routed) return true;
        } else if (action == 4) {
            std::cerr << "#" << client.id << " windows opcode 0x22 helper local consume\n";
            return true;
        }
        queue_windows_route_failure(client, routed_body, table);
        std::cerr << "#" << client.id << " windows opcode 0x22 helper fallback action="
                  << action << " len=" << routed_body.size() << "\n";
        return true;
    }
    if (body[1] != 0x22) return false;
    if (!require_windows_verify(client, body, table, "c2s_gamelogin",
                                state.relay_store->backend_name() == std::string("mssql"))) return true;

    if (state.relay_store->backend_name() == std::string("mssql") &&
        body.size() == 0x2c && read_u16(body, 2) == 0x2a &&
        read_u16(body, 4) == 1 && read_u16(body, 6) == 1) {
        const std::string account = fixed_ascii(body, 0x0c, 32);
        std::uint32_t ignored_user_ip = 0;
        const bool found = state.relay_store->get_account_user_ip(account, ignored_user_ip);
        std::vector<std::uint8_t> reply(0x30, 0);
        reply[0] = body[0];
        reply[1] = 0x22;
        write_u16_at(reply, 2, 0x2e);
        write_u16_at(reply, 4, 1);
        write_u16_at(reply, 6, 4);
        write_u32_at(reply, 8, read_u32(body, 8));
        std::copy_n(body.begin() + 0x0c, 32, reply.begin() + 0x0c);
        write_u32_at(reply, 0x2c, found ? 1 : 3);
        queue_encrypted_frame(client, reply, table);
        std::cerr << "#" << client.id << " windows opcode 0x22 account state acc='"
                  << account << "' found=" << (found ? "yes" : "no") << "\n";
        return true;
    }

    bool valid = body.size() >= 0x14;
    if (valid) {
        std::uint16_t a = read_u16(body, 0x10);
        std::uint16_t b = read_u16(body, 0x12);
        valid = static_cast<std::size_t>(a) + b + 0x14 == body.size();
    }
    std::uint16_t action = body.size() >= 0x10 ? read_u16(body, 0x0e) : 0;
    std::uint32_t target_route = 0;
    bool routed = false;
    if (valid && action == 1 && body.size() > 0x14) {
        std::string account = fixed_ascii(body, 0x14, std::min<std::size_t>(32, body.size() - 0x14));
        routed = state.relay_store->account_client_id(account, target_route) &&
                 forward_to_route(state, clients, table, target_route, body);
        std::cerr << "#" << client.id << " windows gamelogin route account='" << account
                  << "' target_route=" << target_route
                  << " forwarded=" << (routed ? "yes" : "no") << "\n";
    } else if (valid && action == 4) {
        routed = true;
        std::cerr << "#" << client.id << " windows gamelogin action=4 local-consume hook\n";
    }
    queue_windows_ack(client, body, table, 0x22, valid ? (routed ? 1 : 3) : 4);
    std::cerr << "#" << client.id << " windows gamelogin action=" << action
              << " len=" << body.size() << " valid=" << (valid ? "yes" : "no") << "\n";
    return true;
}

bool handle_windows_gateway_info(Client& client,
                                 const std::vector<std::uint8_t>& body,
                                 const std::vector<std::uint32_t>& table,
                                 ServerState& state,
                                 std::map<int, Client>& clients) {
    if (body[1] != kOpcodeGatewayInfo) return false;
    if (!require_windows_verify(client, body, table, "c2s_gatewayinfo",
                                state.relay_store->backend_name() == std::string("mssql"))) return true;

    if (state.relay_store->backend_name() == std::string("mssql")) {
        if (body.size() != 0x32 || read_u16(body, 2) != 0x30 ||
            read_u16(body, 4) != 1 || read_u16(body, 6) != 7) {
            std::cerr << "#" << client.id << " windows opcode 0x26 invalid len/subtype\n";
            return true;
        }
        const std::string name = fixed_ascii(body, 0x0c, 32);
        const std::uint16_t action = read_u16(body, 0x2c);
        std::uint32_t result = 0;
        std::uint32_t target_route = 0;
        Client* target = nullptr;
        if ((action & 0x01) != 0) {
            for (auto& [candidate_fd, candidate] : clients) {
                (void)candidate_fd;
                if (candidate.verified && candidate.server_name == name) {
                    target_route = candidate.route_id;
                    break;
                }
            }
            if (!state.relay_store->account_count(target_route, true, result)) result = 0;
        } else if ((action & 0x02) != 0) {
            if (!state.relay_store->account_count(0, false, result)) result = 0;
        } else if ((action & 0x10) != 0 || (action & 0x40) != 0) {
            if (!state.relay_store->account_client_id(name, target_route)) {
                result = 0xffffffffu;
            } else {
                auto owner = state.route_owner_fd.find(target_route);
                if (owner != state.route_owner_fd.end()) {
                    auto found = clients.find(owner->second);
                    if (found != clients.end()) target = &found->second;
                }
                if (target != nullptr && (action & 0x10) != 0) {
                    in_addr peer_addr{};
                    if (inet_pton(AF_INET, target->peer_ip.c_str(), &peer_addr) == 1) {
                        result = peer_addr.s_addr;
                    }
                } else if (target != nullptr) {
                    result = target->route_id;
                }
            }
        } else if ((action & 0x80) != 0) {
            for (auto& [candidate_fd, candidate] : clients) {
                (void)candidate_fd;
                if (candidate.verified && candidate.server_name == name) {
                    result = candidate.route_id;
                    break;
                }
            }
        }
        std::vector<std::uint8_t> reply(0x32, 0);
        reply[0] = 0;
        reply[1] = kOpcodeGatewayInfo;
        write_u16_at(reply, 2, 0x30);
        write_u16_at(reply, 4, 1);
        write_u16_at(reply, 6, 7);
        std::copy_n(body.begin() + 0x0c, 32, reply.begin() + 0x0c);
        write_u16_at(reply, 0x2c, action);
        write_u32_at(reply, 0x2e, result);
        queue_encrypted_frame(client, reply, table);
        std::cerr << "#" << client.id << " windows opcode 0x26 route action="
                  << action << " name='" << name << "' result=" << result << "\n";
        return true;
    }

    // Preserve the reconstructed CSV-only account-management protocol.
    std::uint16_t action = body.size() > 4 ? read_u16(body, 4) : 0;
    std::string account = body.size() > 0x0a ? fixed_ascii(body, 0x0a, std::min<std::size_t>(32, body.size() - 0x0a)) : "";
    std::string password = body.size() > 0x2a ? fixed_ascii(body, 0x2a, std::min<std::size_t>(64, body.size() - 0x2a)) : "";
    bool ok = true;
    std::string fetched;
    const char* action_name = "unknown";
    switch (action) {
        case 1:
            action_name = "get_password";
            ok = state.relay_store->get_password(account, false, fetched);
            break;
        case 2:
            action_name = "set_password";
            ok = state.relay_store->set_password(account, false, password);
            break;
        case 3:
            action_name = "get_sec_password";
            ok = state.relay_store->get_password(account, true, fetched);
            break;
        case 4:
            action_name = "set_sec_password";
            ok = state.relay_store->set_password(account, true, password);
            break;
        case 5:
            action_name = "unlock_account";
            ok = state.relay_store->unlock_account(account);
            break;
        case 6:
            action_name = "freeze_account";
            ok = state.relay_store->freeze_account(account);
            break;
        case 7:
            action_name = "kickout_account";
            ok = state.relay_store->kickout_account(account);
            break;
        default:
            action_name = "passthrough";
            ok = true;
            break;
    }
    if ((action == 1 || action == 3) && ok) {
        queue_windows_data_ack(client, body, table, kOpcodeGatewayInfo, 1, fetched);
    } else {
        queue_windows_ack(client, body, table, kOpcodeGatewayInfo, ok ? 1 : 2);
    }
    std::cerr << "#" << client.id << " windows gatewayinfo action=" << action
              << " (" << action_name << ") acc='" << account
              << "' ok=" << (ok ? "yes" : "no") << "\n";
    return true;
}

int create_listener(const Config& cfg) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket failed");

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(cfg.port));
    if (inet_pton(AF_INET, cfg.bind.c_str(), &addr.sin_addr) != 1) {
        close(fd);
        throw std::runtime_error("invalid bind address");
    }
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        throw std::runtime_error(std::string("bind failed: ") + std::strerror(errno));
    }
    if (listen(fd, 128) != 0) {
        close(fd);
        throw std::runtime_error("listen failed");
    }
    set_nonblocking(fd);
    return fd;
}

void handle_frame(Client& client,
                  std::vector<std::uint8_t> body,
                  const std::vector<std::uint32_t>& table,
                  ServerState& state,
                  int fd,
                  std::map<int, Client>& clients) {
    heaven_cipher(table, body.data(), body.size(), client.keys.k1);
    if (body.size() >= 2) {
        std::cerr << "#" << client.id << " decrypted opcode bytes: 0x"
                  << std::hex << static_cast<int>(body[0]) << " 0x"
                  << static_cast<int>(body[1]) << std::dec << " len=" << body.size() << "\n";
    }

    if (body.size() == 6 && body[0] == 0x00 && body[1] == 0x70) {
        std::vector<std::uint8_t> pong = {0x00, 0x82, body[2], body[3], body[4], body[5]};
        queue_encrypted_frame(client, pong, table);
        std::cerr << "#" << client.id << " relay ping response queued\n";
        return;
    }

    if (body.size() > 1) {
        if (handle_windows_verify(client, body, table, state, fd, clients)) return;
        if (handle_windows_account_login(client, body, table, state)) return;
        if (handle_windows_game_login(client, body, table, state, clients)) return;
        if (handle_windows_logout(client, body, table, state)) return;
        if (handle_windows_gateway_info(client, body, table, state, clients)) return;
    }

    if (body.size() > 0x2b && body[1] == 0x24) {
        std::vector<std::uint8_t> reply(0x34, 0);
        reply[0] = body[0];
        reply[1] = 0x24;
        reply[2] = 0x32;
        reply[3] = 0x00;
        reply[4] = 0x01;
        reply[5] = 0x00;
        reply[6] = 0x04;
        reply[7] = 0x00;
        std::copy_n(body.begin() + 8, 4, reply.begin() + 8);
        std::copy_n(body.begin() + 12, 32, reply.begin() + 12);
        reply[0x2c] = 1;
        queue_encrypted_frame(client, reply, table);
        client.verified = true;
        client.account = fixed_ascii(body, 12, 32);
        std::cerr << "#" << client.id << " relay verify -> SUCCESS acc='" << client.account << "'\n";
        return;
    }

    std::cerr << "#" << client.id << " unhandled relay packet len=" << body.size() << "\n";
}

void parse_client_frames(Client& client,
                         const std::vector<std::uint32_t>& table,
                         ServerState& state,
                         int fd,
                         std::map<int, Client>& clients) {
    while (client.in.size() >= 2) {
        std::uint16_t frame_len = static_cast<std::uint16_t>(client.in[0] | (client.in[1] << 8));
        if (frame_len < 2 || frame_len > kMaxFrame) {
            // Windows leaves a malformed trailing fragment pending rather
            // than tearing down the session; retain it for a future frame.
            return;
        }
        if (client.in.size() < frame_len) return;

        std::vector<std::uint8_t> body(client.in.begin() + 2, client.in.begin() + frame_len);
        client.in.erase(client.in.begin(), client.in.begin() + frame_len);
        handle_frame(client, std::move(body), table, state, fd, clients);
    }
}

void event_loop(int listener, const std::vector<std::uint32_t>& table, ServerState& state) {
    std::map<int, Client> clients;
    std::mt19937 rng(std::random_device{}());
    std::uint32_t next_id = 1;

    while (true) {
        fd_set readfds;
        fd_set writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(listener, &readfds);
        int maxfd = listener;

        for (auto& [fd, client] : clients) {
            FD_SET(fd, &readfds);
            if (!client.out.empty()) FD_SET(fd, &writefds);
            maxfd = std::max(maxfd, fd);
        }

        if (select(maxfd + 1, &readfds, &writefds, nullptr, nullptr) < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error("select failed");
        }

        if (FD_ISSET(listener, &readfds)) {
            while (true) {
                sockaddr_in peer{};
                socklen_t peer_len = sizeof(peer);
                int fd = accept(listener, reinterpret_cast<sockaddr*>(&peer), &peer_len);
                if (fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
                    throw std::runtime_error("accept failed");
                }
                set_nonblocking(fd);
                int keepalive = 1;
                setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
                Client client;
                client.fd = fd;
                client.id = next_id++;
                client.keys.k1 = rng();
                client.keys.k2 = rng();
                queue_handshake(client);
                std::cerr << "Relay client #" << client.id << " connected. Handshake K1=0x"
                          << std::hex << client.keys.k1 << " K2=0x" << client.keys.k2 << std::dec << "\n";
                char ip[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
                client.peer_ip = ip;
                std::cerr << "Client #" << client.id << " connected from " << ip << ":"
                          << ntohs(peer.sin_port) << "\n";
                clients.emplace(fd, std::move(client));
            }
        }

        std::set<int> closed;
        for (auto& [fd, client] : clients) {
            if (FD_ISSET(fd, &readfds)) {
                while (true) {
                    std::array<std::uint8_t, 8192> buf{};
                    ssize_t n = recv(fd, buf.data(), buf.size(), 0);
                    if (n == 0) {
                        closed.insert(fd);
                        break;
                    }
                    if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
                        closed.insert(fd);
                        break;
                    }
                    if (client.in.size() + static_cast<std::size_t>(n) > kMaxInputBuffer) {
                        std::cerr << "#" << client.id << " input buffer exceeded\n";
                        closed.insert(fd);
                        break;
                    }
                    client.in.insert(client.in.end(), buf.begin(), buf.begin() + n);
                    try {
                        parse_client_frames(client, table, state, fd, clients);
                    } catch (const std::exception& e) {
                        std::cerr << "#" << client.id << " protocol error: " << e.what() << "\n";
                        closed.insert(fd);
                        break;
                    }
                }
            }

            if (closed.count(fd) == 0 && FD_ISSET(fd, &writefds) && !client.out.empty()) {
                while (!client.out.empty()) {
                    ssize_t n = send(fd, client.out.data(), client.out.size(), 0);
                    if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
                        closed.insert(fd);
                        break;
                    }
                    if (n == 0) {
                        closed.insert(fd);
                        break;
                    }
                    client.out.erase(client.out.begin(), client.out.begin() + n);
                }
            }
            if (closed.count(fd) == 0 && client.close_after_write && client.out.empty()) {
                closed.insert(fd);
            }
        }

        for (int fd : closed) {
            auto it = clients.find(fd);
            if (it != clients.end()) {
                std::cerr << "Client #" << it->second.id << " disconnected\n";
                if (state.relay_store->backend_name() == std::string("mssql") &&
                    it->second.verified) {
                    for (auto& [other_fd, other] : clients) {
                        if (other_fd == fd || closed.count(other_fd) != 0 || !other.verified) continue;
                        queue_route_announcement(other, it->second, 8, table);
                    }
                }
                if (it->second.route_id != 0) {
                    auto route = state.route_owner_fd.find(it->second.route_id);
                    if (route != state.route_owner_fd.end() && route->second == fd) {
                        state.route_owner_fd.erase(route);
                    }
                }
            }
            close(fd);
            clients.erase(fd);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::string root = argc > 1 ? argv[1] : "config";
        Config cfg = load_config(root);
        if (!cfg.allow_wan && machine_has_wan_ipv4()) {
            std::cerr << "IP may khong nam trong dai cho phep - tu choi chay.\n";
            return 3;
        }

        auto table = load_heaven_table(root);
        ServerState state;
        state.service_port = static_cast<std::uint16_t>(cfg.port);
        state.relay_store = create_relay_store(cfg.relay_store, root);
        int listener = create_listener(cfg);
        std::cerr << "S3RelayServer C++ listening on " << cfg.bind << ":" << cfg.port
                  << " relay_store=" << state.relay_store->backend_name() << "\n";
        event_loop(listener, table, state);
        close(listener);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
