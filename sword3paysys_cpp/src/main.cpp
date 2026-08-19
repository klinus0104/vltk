#include "account_store.h"

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
#include <ctime>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kTableBytes = 0x58bc;
constexpr std::size_t kTableWords = 0x162f;
constexpr std::uint32_t kCipherAdd = 0x2e6d23c1;
constexpr std::size_t kMaxFrame = 0x2000;
constexpr std::size_t kMaxInputBuffer = 0x800000;
// PaySys account-login opcode; numeric value is part of the wire protocol.
constexpr std::uint8_t kOpcodeAccountLogin = 0x21;

struct Config {
    std::string bind = "0.0.0.0";
    int port = 5002;
    bool allow_wan = false;
    bool accept_any_login = true;
    bool enforce_password = true;
    bool realtime_charge = false;
    bool respect_enddate = false;
    int charge_interval = 5;
    std::string db_server = "127.0.0.1:1433";
    std::string db_user = "sa";
    std::string db_password;
    std::string db_name = "account_tong";
    std::string db_charset = "UTF-8";
    std::string account_schema = "linux";
};

struct KeyState {
    std::uint32_t k1 = 0;
    std::uint32_t k2 = 0;
};

struct Client {
    int fd = -1;
    std::uint32_t id = 0;
    std::string account;
    std::time_t login_time = 0;
    std::uint32_t heartbeat_counter = 0;
    bool logged_in = false;
    KeyState keys;
    std::vector<std::uint8_t> in;
    std::vector<std::uint8_t> out;
};

struct ServerState {
    std::unique_ptr<AccountStore> account_store;
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
    auto acc = read_ini(root + "/Acc_Setup.ini");
    if (acc.count("Local")) {
        auto& local = acc["Local"];
        if (local.count("bind")) cfg.bind = local["bind"];
        if (local.count("port")) cfg.port = std::stoi(local["port"]);
        if (local.count("allow_wan")) cfg.allow_wan = parse_bool(local["allow_wan"]);
        if (local.count("accept_any_login")) cfg.accept_any_login = parse_bool(local["accept_any_login"]);
        if (local.count("enforce_password")) cfg.enforce_password = parse_bool(local["enforce_password"]);
    }
    if (acc.count("Billing")) {
        auto& billing = acc["Billing"];
        if (billing.count("realtime_charge")) cfg.realtime_charge = parse_bool(billing["realtime_charge"]);
        if (billing.count("respect_enddate")) cfg.respect_enddate = parse_bool(billing["respect_enddate"]);
        if (billing.count("charge_interval")) cfg.charge_interval = std::max(1, std::stoi(billing["charge_interval"]));
    }

    auto db = read_ini(root + "/mssql.ini");
    auto read_db = [&](const std::string& key, std::string& target) {
        if (db.count("") && db[""].count(key)) target = db[""][key];
        if (db.count("mssql") && db["mssql"].count(key)) target = db["mssql"][key];
        if (db.count("MSSQL") && db["MSSQL"].count(key)) target = db["MSSQL"][key];
    };
    read_db("server", cfg.db_server);
    read_db("user", cfg.db_user);
    read_db("password", cfg.db_password);
    read_db("database", cfg.db_name);
    read_db("charset", cfg.db_charset);
    read_db("account_schema", cfg.account_schema);
    return cfg;
}

AccountConfig account_config_from(const Config& cfg) {
    AccountConfig out;
    out.accept_any_login = cfg.accept_any_login;
    out.enforce_password = cfg.enforce_password;
    out.realtime_charge = cfg.realtime_charge;
    out.respect_enddate = cfg.respect_enddate;
    out.charge_interval = cfg.charge_interval;
    out.db_server = cfg.db_server;
    out.db_user = cfg.db_user;
    out.db_password = cfg.db_password;
    out.db_name = cfg.db_name;
    out.db_charset = cfg.db_charset;
    out.account_schema = cfg.account_schema;
    return out;
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

std::uint32_t read_u32_unaligned(const std::vector<std::uint8_t>& data, std::size_t off) {
    return static_cast<std::uint32_t>(data[off]) |
           (static_cast<std::uint32_t>(data[off + 1]) << 8) |
           (static_cast<std::uint32_t>(data[off + 2]) << 16) |
           (static_cast<std::uint32_t>(data[off + 3]) << 24);
}

void write_u16_at(std::vector<std::uint8_t>& out, std::size_t off, std::uint16_t v) {
    out[off] = static_cast<std::uint8_t>(v & 0xff);
    out[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
}

void write_u32_at(std::vector<std::uint8_t>& out, std::size_t off, std::uint32_t v) {
    out[off] = static_cast<std::uint8_t>(v & 0xff);
    out[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    out[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xff);
    out[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xff);
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

[[maybe_unused]] void queue_encrypted_frame(Client& client, const std::vector<std::uint8_t>& body, const std::vector<std::uint32_t>& table) {
    std::vector<std::uint8_t> encrypted = body;
    heaven_cipher(table, encrypted.data(), encrypted.size(), client.keys.k2);
    queue_plain_frame(client, encrypted);
}

void queue_handshake(Client& client) {
    // libheaven sends a 44-byte frame: a two-byte length header followed by a
    // 42-byte handshake body.  Bishop reads the encoded inbound/outbound keys
    // at body offsets 8 and 17 respectively.
    std::vector<std::uint8_t> body(0x2a, 0);
    body[0] = 0x20;
    write_u32_at(body, 8, handshake_word(client.keys.k1));
    write_u32_at(body, 17, handshake_word(client.keys.k2));
    queue_plain_frame(client, body);
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

void queue_heartbeat(Client& client, const std::vector<std::uint32_t>& table) {
    std::vector<std::uint8_t> ping(5, 0);
    ping[0] = 0x8d;
    write_u32_at(ping, 1, client.heartbeat_counter++);
    queue_encrypted_frame(client, ping, table);
}

void handle_frame(Client& client, std::vector<std::uint8_t> body, const std::vector<std::uint32_t>& table, ServerState& state) {
    heaven_cipher(table, body.data(), body.size(), client.keys.k1);
    if (body.size() >= 2) {
        std::cerr << "#" << client.id << " PaySys decrypted opcode bytes: 0x"
                  << std::hex << static_cast<int>(body[0]) << " 0x"
                  << static_cast<int>(body[1]) << std::dec << " len=" << body.size() << "\n";
    }

    if (body.size() == 5) {
        std::vector<std::uint8_t> resp(5, 0);
        resp[0] = 0x8d;
        std::copy(body.begin() + 1, body.end(), resp.begin() + 1);
        queue_encrypted_frame(client, resp, table);
        return;
    }

    if (body.empty()) return;
    std::uint8_t opcode = body[0];

    if ((opcode == 0x24 || opcode == 0x25) && body.size() > 0x2a) {
        std::uint32_t request_id = read_u32_unaligned(body, 0x07);
        std::vector<std::uint8_t> resp(0x33, 0);
        resp[0] = 0x24;
        write_u32_at(resp, 0x01, 0x00010032);
        write_u16_at(resp, 0x05, 0x0004);
        write_u32_at(resp, 0x07, request_id);
        std::copy_n(body.begin() + 0x0b, 32, resp.begin() + 0x0b);
        write_u32_at(resp, 0x2b, 1);
        queue_encrypted_frame(client, resp, table);

        queue_heartbeat(client, table);
        std::cerr << "#" << client.id << " -> s2c_gatewayverify nReturn=SUCCESS acc='"
                  << fixed_ascii(body, 0x0b, 32) << "'\n";
        return;
    }

    if (opcode == kOpcodeAccountLogin && body.size() > 0x4a) {
        std::uint32_t request_id = read_u32_unaligned(body, 0x07);
        std::string account = fixed_ascii(body, 0x0b, 32);
        // The request embeds a three-byte password header at 0x2b; the
        // 32-byte credential/hash itself begins at 0x2e.
        std::string password = fixed_ascii(body, 0x2e, 32);
        LoginResult login = state.account_store->login(account, password, client.id);
        // The original response builder always publishes time(NULL) + left,
        // including failure replies where left is zero.
        if (login.end_time == 0) {
            login.end_time = static_cast<std::uint32_t>(std::time(nullptr));
        }
        if (login.n_return == 1) {
            if (client.logged_in && client.account != account) state.account_store->logout(client.account);
            client.account = account;
            client.login_time = std::time(nullptr);
            client.logged_in = true;
        }

        std::vector<std::uint8_t> resp(0x63, 0);
        resp[0] = kOpcodeAccountLogin;
        write_u32_at(resp, 0x01, 0x00010062);
        write_u16_at(resp, 0x05, 0x0009);
        write_u32_at(resp, 0x07, request_id);
        std::copy_n(body.begin() + 0x0b, 32, resp.begin() + 0x0b);
        write_u32_at(resp, 0x2b, login.n_return);
        write_u32_at(resp, 0x2f, login.ext_point);
        write_u32_at(resp, 0x4f, login.end_time);
        write_u32_at(resp, 0x57, 0x000000ff);
        queue_encrypted_frame(client, resp, table);
        std::cerr << "#" << client.id << " -> s2c_accountlogin acc='" << account
                  << "' nReturn=" << login.n_return << " ext=" << login.ext_point
                  << " left=" << login.left_second << "s endTime=" << login.end_time << "\n";
        return;
    }

    if (opcode == 0x22 && body.size() >= 0x2b) {
        // The reference treats this 43-byte packet as a one-way notification:
        // Bishop has committed the selected character to a GameServer. There
        // is no PaySys business response; retaining the session is sufficient.
        std::string account = fixed_ascii(body, 0x0b, 32);
        if (!account.empty()) {
            client.account = account;
            client.logged_in = true;
        }
        std::cerr << "#" << client.id << " <- c2s_gamelogin acc='" << account
                  << "' (one-way, no response)\n";
        return;
    }

    if (opcode == 0x23 && body.size() > 0x2a) {
        std::uint32_t request_id = read_u32_unaligned(body, 0x07);
        std::string account = fixed_ascii(body, 0x0b, 32);
        state.account_store->logout(account);
        if (client.account == account) {
            client.account.clear();
            client.logged_in = false;
            client.login_time = 0;
        }

        std::vector<std::uint8_t> resp(0x33, 0);
        resp[0] = 0x23;
        write_u32_at(resp, 0x01, 0x00010032);
        write_u16_at(resp, 0x05, 0x0004);
        write_u32_at(resp, 0x07, request_id);
        std::copy_n(body.begin() + 0x0b, 32, resp.begin() + 0x0b);
        write_u32_at(resp, 0x2b, 1);
        queue_encrypted_frame(client, resp, table);
        std::cerr << "#" << client.id << " -> s2c_accountlogout acc='" << account << "'\n";
        return;
    }

    std::cerr << "#" << client.id << " unhandled PaySys opcode=0x"
              << std::hex << static_cast<int>(opcode) << std::dec
              << " len=" << body.size() << "\n";
}

void parse_client_frames(Client& client, const std::vector<std::uint32_t>& table, ServerState& state) {
    while (client.in.size() >= 2) {
        std::uint16_t frame_len = static_cast<std::uint16_t>(client.in[0] | (client.in[1] << 8));
        if (frame_len < 2 || frame_len > kMaxFrame) {
            throw std::runtime_error("invalid frame length");
        }
        if (client.in.size() < frame_len) return;

        std::vector<std::uint8_t> body(client.in.begin() + 2, client.in.begin() + frame_len);
        client.in.erase(client.in.begin(), client.in.begin() + frame_len);
        handle_frame(client, std::move(body), table, state);
    }
}

void event_loop(int listener, const std::vector<std::uint32_t>& table, ServerState& state) {
    std::map<int, Client> clients;
    std::mt19937 rng(std::random_device{}());
    std::uint32_t next_id = 1;
    std::time_t last_tick = std::time(nullptr);
    std::uint32_t tick_counter = 0;

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

        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        if (select(maxfd + 1, &readfds, &writefds, nullptr, &timeout) < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error("select failed");
        }

        std::time_t now = std::time(nullptr);
        if (now != last_tick) {
            last_tick = now;
            state.account_store->tick();
            ++tick_counter;
            if ((tick_counter & 1u) == 0) {
                for (auto& [_, client] : clients) {
                    if (client.logged_in) queue_heartbeat(client, table);
                }
            }
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
                std::cerr << "Bishop #" << client.id << " connected. Sent handshake K1=0x"
                          << std::hex << client.keys.k1 << " K2=0x" << client.keys.k2 << std::dec << "\n";
                char ip[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
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
                        parse_client_frames(client, table, state);
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
        }

        for (int fd : closed) {
            auto it = clients.find(fd);
            if (it != clients.end()) {
                // The original PaySys keeps the account session when its
                // Bishop transport disappears. Account cleanup is driven by
                // the explicit 0x23 lifecycle packet (including the
                // GameServer/Bishop path used after a forced client close),
                // not by this service socket's EOF.
                std::cerr << "Client #" << it->second.id << " disconnected\n";
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
        state.account_store = create_account_store(account_config_from(cfg), root);
        int listener = create_listener(cfg);
        std::cerr << "Sword3PaySys C++ listening on " << cfg.bind << ":" << cfg.port
                  << " account_backend=" << state.account_store->backend_name()
                  << " realtime_charge=" << (cfg.realtime_charge ? "on" : "off")
                  << " charge_interval=" << cfg.charge_interval
                  << " respect_enddate=" << (cfg.respect_enddate ? "on" : "off")
                  << " db=" << cfg.db_server << "/" << cfg.db_name << "\n";
        event_loop(listener, table, state);
        close(listener);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
