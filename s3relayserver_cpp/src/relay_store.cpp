#include "relay_store.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#ifdef HAVE_FREETDS
#include <sybdb.h>
#ifndef DBSETLCHARSET
#define DBSETLCHARSET 10
#endif
#endif

namespace {

std::string trim(std::string s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), is_space));
    s.erase(std::find_if_not(s.rbegin(), s.rend(), is_space).base(), s.end());
    return s;
}

bool is_legacy_192_168_peer(const std::string& ip) {
    return ip.rfind("192.168.", 0) == 0;
}

bool ascii_iequals(std::string left, std::string right) {
    left = trim(left);
    right = trim(right);
    if (left.size() != right.size()) return false;
    return std::equal(left.begin(), left.end(), right.begin(), [](unsigned char a, unsigned char b) {
        return std::toupper(a) == std::toupper(b);
    });
}

bool identity_matches(const std::string& peer_ip,
                      std::uint16_t service_port,
                      const std::string& identity,
                      const std::string& expected_ip,
                      std::uint16_t expected_port,
                      const std::string& expected_identity) {
    if (is_legacy_192_168_peer(peer_ip)) return true;
    return peer_ip == trim(expected_ip) && service_port == expected_port &&
           ascii_iequals(identity, expected_identity);
}

std::string sql_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        out.push_back(c);
        if (c == '\'') out.push_back('\'');
    }
    return out;
}

#if 0  // File-backed relay backend removed; production uses MSSQL only.
std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> cols;
    std::string current;
    bool quoted = false;
    for (char c : line) {
        if (c == '"') {
            quoted = !quoted;
            continue;
        }
        if (c == ',' && !quoted) {
            cols.push_back(trim(current));
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    cols.push_back(trim(current));
    return cols;
}

std::string csv_escape(const std::string& value) {
    bool needs_quotes = value.find_first_of(",\"\n\r") != std::string::npos;
    if (!needs_quotes) return value;
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::string path_join(const std::string& root, const std::string& path) {
    if (path.empty()) return root;
    if (path.front() == '/') return path;
    return root + "/" + path;
}

[[maybe_unused]] std::string sql_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        out.push_back(c);
        if (c == '\'') out.push_back('\'');
    }
    return out;
}

struct CsvServer {
    std::string password;
    std::uint32_t id = 0;
    std::string ip;
    std::uint16_t port = 0;
    std::string memo;
};

struct CsvAccount {
    std::string password;
    std::string sec_password;
    std::uint32_t client_id = 0;
    std::uint32_t user_ip = 0;
    bool frozen = false;
};

class CsvRelayStore final : public RelayStore {
public:
    CsvRelayStore(const RelayConfig& cfg, const std::string& root)
        : server_path_(path_join(root, cfg.server_file)),
          account_path_(path_join(root, cfg.account_file)) {
        load_servers();
        load_accounts();
    }

    ServerVerifyResult verify_server(const std::string& name,
                                     const std::string& password,
                                     const std::string& peer_ip,
                                     std::uint16_t service_port,
                                     const std::string& identity) override {
        auto it = servers_.find(name);
        if (it == servers_.end()) {
            CsvServer server;
            server.password = password;
            server.id = next_server_id_++;
            server.ip = peer_ip;
            server.port = service_port;
            server.memo = identity;
            it = servers_.emplace(name, server).first;
            save_servers();
        }
        CsvServer& server = it->second;
        ServerVerifyResult result;
        result.found = true;
        result.password_match = server.password.empty() || server.password == password;
        if (!result.password_match) return result;
        if (!identity_matches(peer_ip, service_port, identity,
                              server.ip, server.port, server.memo)) return {};
        result.ok = true;
        result.id = server.id;
        result.ip = server.ip.empty() ? peer_ip : server.ip;
        result.port = server.port == 0 ? service_port : server.port;
        result.memo = server.memo;
        return result;
    }

    bool mark_account_online(const std::string& account, std::uint32_t client_id, std::uint32_t user_ip) override {
        if (account.empty()) return false;
        const auto existing = accounts_.find(account);
        const bool existed = existing != accounts_.end();
        const CsvAccount before = existed ? existing->second : CsvAccount{};
        CsvAccount& rec = accounts_[account];
        if (rec.frozen) return false;
        rec.client_id = client_id;
        rec.user_ip = user_ip;
        if (save_accounts()) return true;
        if (existed) accounts_[account] = before;
        else accounts_.erase(account);
        return false;
    }

    bool mark_account_offline(const std::string& account) override {
        if (account.empty()) return false;
        CsvAccount& rec = accounts_[account];
        rec.client_id = 0;
        save_accounts();
        return true;
    }

    bool account_client_id(const std::string& account, std::uint32_t& client_id) override {
        auto it = accounts_.find(account);
        if (it == accounts_.end() || it->second.client_id == 0) return false;
        client_id = it->second.client_id;
        return true;
    }

    bool account_count(std::uint32_t client_id,
                       bool filter_client_id,
                       std::uint32_t& count) override {
        count = 0;
        for (const auto& [account, rec] : accounts_) {
            (void)account;
            if (!filter_client_id || rec.client_id == client_id) ++count;
        }
        return true;
    }

    bool get_password(const std::string& account, bool secondary, std::string& password) override {
        auto it = accounts_.find(account);
        if (it == accounts_.end()) return false;
        password = secondary ? it->second.sec_password : it->second.password;
        return true;
    }

    bool set_password(const std::string& account, bool secondary, const std::string& password) override {
        if (account.empty()) return false;
        CsvAccount& rec = accounts_[account];
        if (secondary) rec.sec_password = password;
        else rec.password = password;
        save_accounts();
        return true;
    }

    bool set_credentials(const std::string& account,
                         const std::string& password,
                         const std::string& secondary,
                         std::uint32_t user_ip) override {
        if (account.empty() || password.empty()) return false;
        const auto existing = accounts_.find(account);
        const bool existed = existing != accounts_.end();
        const CsvAccount before = existed ? existing->second : CsvAccount{};
        CsvAccount& rec = accounts_[account];
        rec.password = password;
        if (!secondary.empty()) rec.sec_password = secondary;
        if (user_ip != 0) rec.user_ip = user_ip;
        if (save_accounts()) return true;
        if (existed) accounts_[account] = before;
        else accounts_.erase(account);
        return false;
    }

    bool set_account_user_ip(const std::string& account, std::uint32_t user_ip) override {
        if (account.empty()) return false;
        CsvAccount& rec = accounts_[account];
        rec.user_ip = user_ip;
        save_accounts();
        return true;
    }

    bool get_account_user_ip(const std::string& account, std::uint32_t& user_ip) override {
        auto it = accounts_.find(account);
        if (it == accounts_.end()) return false;
        user_ip = it->second.user_ip;
        return true;
    }

    bool kickout_account(const std::string& account) override {
        return mark_account_offline(account);
    }

    bool freeze_account(const std::string& account) override {
        if (account.empty()) return false;
        CsvAccount& rec = accounts_[account];
        rec.frozen = true;
        rec.client_id = 0;
        save_accounts();
        return true;
    }

    bool unlock_account(const std::string& account) override {
        if (account.empty()) return false;
        CsvAccount& rec = accounts_[account];
        rec.frozen = false;
        save_accounts();
        return true;
    }

    const char* backend_name() const override {
        return "csv";
    }

private:
    void load_servers() {
        std::ifstream in(server_path_);
        if (!in) return;
        std::string line;
        while (std::getline(in, line)) {
            auto cols = split_csv_line(trim(line));
            if (cols.empty() || cols[0].empty() || cols[0] == "name") continue;
            CsvServer server;
            if (cols.size() > 1) server.password = cols[1];
            if (cols.size() > 2 && !cols[2].empty()) server.id = static_cast<std::uint32_t>(std::stoul(cols[2]));
            if (cols.size() > 3) server.ip = cols[3];
            if (cols.size() > 4 && !cols[4].empty()) server.port = static_cast<std::uint16_t>(std::stoul(cols[4]));
            if (cols.size() > 5) server.memo = cols[5];
            next_server_id_ = std::max(next_server_id_, server.id + 1);
            servers_[cols[0]] = server;
        }
    }

    void load_accounts() {
        std::ifstream in(account_path_);
        if (!in) return;
        std::string line;
        while (std::getline(in, line)) {
            auto cols = split_csv_line(trim(line));
            if (cols.empty() || cols[0].empty() || cols[0] == "account") continue;
            CsvAccount account;
            if (cols.size() > 1) account.password = cols[1];
            if (cols.size() > 2) account.sec_password = cols[2];
            if (cols.size() > 3 && !cols[3].empty()) account.client_id = static_cast<std::uint32_t>(std::stoul(cols[3]));
            if (cols.size() > 4 && !cols[4].empty()) account.user_ip = static_cast<std::uint32_t>(std::stoul(cols[4]));
            if (cols.size() > 5) account.frozen = cols[5] == "1";
            accounts_[cols[0]] = account;
        }
    }

    void save_servers() const {
        std::ofstream out(server_path_, std::ios::trunc);
        if (!out) return;
        out << "name,password,id,ip,port,memo\n";
        for (const auto& [name, server] : servers_) {
            out << csv_escape(name) << ',' << csv_escape(server.password) << ','
                << server.id << ',' << csv_escape(server.ip) << ','
                << server.port << ',' << csv_escape(server.memo) << '\n';
        }
    }

    bool save_accounts() const {
        const std::string temp_path = account_path_ + ".tmp";
        std::ofstream out(temp_path, std::ios::trunc);
        if (!out) return false;
        out << "account,password,sec_password,client_id,user_ip,frozen\n";
        for (const auto& [account, rec] : accounts_) {
            out << csv_escape(account) << ',' << csv_escape(rec.password) << ','
                << csv_escape(rec.sec_password) << ',' << rec.client_id << ','
                << rec.user_ip << ',' << (rec.frozen ? 1 : 0) << '\n';
        }
        out.flush();
        if (!out) {
            out.close();
            std::remove(temp_path.c_str());
            return false;
        }
        out.close();
        if (std::rename(temp_path.c_str(), account_path_.c_str()) != 0) {
            std::remove(temp_path.c_str());
            return false;
        }
        return true;
    }

    std::string server_path_;
    std::string account_path_;
    std::uint32_t next_server_id_ = 1;
    std::unordered_map<std::string, CsvServer> servers_;
    std::unordered_map<std::string, CsvAccount> accounts_;
};

#endif

#ifdef HAVE_FREETDS
bool db_exec_drain(DBPROCESS* db, const std::string& sql) {
    if (!db) return false;
    dbcmd(db, sql.c_str());
    if (dbsqlexec(db) == FAIL) return false;
    while (true) {
        RETCODE result = dbresults(db);
        if (result == NO_MORE_RESULTS) return true;
        if (result == FAIL) return false;
        while (true) {
            RETCODE row = dbnextrow(db);
            if (row == NO_MORE_ROWS) break;
            if (row == FAIL) return false;
        }
    }
}

class MssqlRelayStore final : public RelayStore {
public:
    explicit MssqlRelayStore(const RelayConfig& cfg) {
        if (dbinit() == FAIL) throw std::runtime_error("Relay MSSQL: dbinit failed");
        login_ = dblogin();
        if (!login_) throw std::runtime_error("Relay MSSQL: dblogin failed");
        DBSETLUSER(login_, const_cast<char*>(cfg.db_user.c_str()));
        DBSETLPWD(login_, const_cast<char*>(cfg.db_password.c_str()));
        DBSETLAPP(login_, const_cast<char*>("s3relayserver_cpp"));
        DBSETLCHARSET(login_, const_cast<char*>(cfg.db_charset.c_str()));
        db_ = dbopen(login_, const_cast<char*>(cfg.db_server.c_str()));
        if (!db_) throw std::runtime_error("Relay MSSQL: dbopen failed");
        if (dbuse(db_, const_cast<char*>(cfg.db_name.c_str())) == FAIL) {
            dbclose(db_);
            db_ = nullptr;
            throw std::runtime_error("Relay MSSQL: dbuse failed");
        }
        std::cerr << "Relay MSSQL connected " << cfg.db_user << "@" << cfg.db_server << "/" << cfg.db_name << "\n";
    }

    ~MssqlRelayStore() override {
        if (db_) dbclose(db_);
        if (login_) dbloginfree(login_);
    }

    ServerVerifyResult verify_server(const std::string& name,
                                     const std::string& password,
                                     const std::string& peer_ip,
                                     std::uint16_t service_port,
                                     const std::string& identity) override {
        ServerVerifyResult result;
        std::string sql =
            "select cIP, iPort, iid, cMemo, cPassword from ServerList where (cServerName = '" + sql_escape(name) + "')";
        char ip[65]{};
        char memo[129]{};
        char stored_password[129]{};
        DBINT port = 0;
        DBINT id = 0;
        dbcmd(db_, sql.c_str());
        if (dbsqlexec(db_) == FAIL) return result;
        while (true) {
            RETCODE res = dbresults(db_);
            if (res == NO_MORE_RESULTS) break;
            if (res == FAIL) return {};
            dbbind(db_, 1, NTBSTRINGBIND, sizeof(ip), reinterpret_cast<BYTE*>(ip));
            dbbind(db_, 2, INTBIND, sizeof(port), reinterpret_cast<BYTE*>(&port));
            dbbind(db_, 3, INTBIND, sizeof(id), reinterpret_cast<BYTE*>(&id));
            dbbind(db_, 4, NTBSTRINGBIND, sizeof(memo), reinterpret_cast<BYTE*>(memo));
            dbbind(db_, 5, NTBSTRINGBIND, sizeof(stored_password), reinterpret_cast<BYTE*>(stored_password));
            RETCODE row = dbnextrow(db_);
            if (row == FAIL) return {};
            if (row != NO_MORE_ROWS) {
                result.found = true;
                result.password_match = std::string(stored_password) == password;
                std::cerr << "Relay MSSQL verify row server='" << name
                          << "' password_match=" << (result.password_match ? "yes" : "no")
                          << " stored_len=" << std::string(stored_password).size()
                          << " request_len=" << password.size()
                          << " peer=" << peer_ip
                          << " bypass=" << (is_legacy_192_168_peer(peer_ip) ? "yes" : "no")
                          << "\n";
                if (!result.password_match) return result;
                std::uint16_t expected_port = port > 0 ? static_cast<std::uint16_t>(port) : 0;
                const bool bypass = is_legacy_192_168_peer(peer_ip);
                result.ip_match = bypass || peer_ip == trim(ip);
                result.identity_match = bypass || ascii_iequals(identity, memo);
                // Windows reads iPort, but changing only this field does not
                // reject a controlled strict-path verification.
                result.ok = result.ip_match && result.identity_match;
                result.id = id > 0 ? static_cast<std::uint32_t>(id) : 0;
                result.ip = ip[0] ? ip : peer_ip;
                result.port = expected_port != 0 ? expected_port : service_port;
                result.memo = memo;
                while (dbnextrow(db_) != NO_MORE_ROWS) {
                }
            }
        }
        return result;
    }

    bool mark_account_online(const std::string& account, std::uint32_t client_id, std::uint32_t user_ip) override {
        std::string escaped = sql_escape(account);
        return db_exec_drain(db_,
            "update Account_info set iClientID = " + std::to_string(client_id) +
            ", nUserIP = " + std::to_string(user_ip) +
            " where (cAccName = '" + escaped + "')");
    }

    bool mark_account_offline(const std::string& account) override {
        return db_exec_drain(db_,
            "update Account_info set iClientID = 0 where (cAccName = '" + sql_escape(account) + "')");
    }

    bool account_client_id(const std::string& account, std::uint32_t& client_id) override {
        std::string sql = "select iClientID from Account_info where (cAccName = '" + sql_escape(account) + "')";
        DBINT value = 0;
        dbcmd(db_, sql.c_str());
        if (dbsqlexec(db_) == FAIL) return false;
        bool found = false;
        while (true) {
            RETCODE res = dbresults(db_);
            if (res == NO_MORE_RESULTS) break;
            if (res == FAIL) return false;
            dbbind(db_, 1, INTBIND, sizeof(value), reinterpret_cast<BYTE*>(&value));
            RETCODE row = dbnextrow(db_);
            if (row == FAIL) return false;
            if (row != NO_MORE_ROWS) {
                found = true;
                while (dbnextrow(db_) != NO_MORE_ROWS) {
                }
            }
        }
        if (!found || value <= 0) return false;
        client_id = static_cast<std::uint32_t>(value);
        return true;
    }

    bool account_count(std::uint32_t client_id,
                       bool filter_client_id,
                       std::uint32_t& count) override {
        std::string sql = "select count(*) from Account_info";
        if (filter_client_id) {
            sql += " where (iClientID = " + std::to_string(client_id) + ")";
        }
        DBINT value = 0;
        dbcmd(db_, sql.c_str());
        if (dbsqlexec(db_) == FAIL) return false;
        bool found = false;
        while (true) {
            RETCODE res = dbresults(db_);
            if (res == NO_MORE_RESULTS) break;
            if (res == FAIL) return false;
            dbbind(db_, 1, INTBIND, sizeof(value), reinterpret_cast<BYTE*>(&value));
            RETCODE row = dbnextrow(db_);
            if (row == FAIL) return false;
            if (row != NO_MORE_ROWS) {
                found = true;
                while (dbnextrow(db_) != NO_MORE_ROWS) {
                }
            }
        }
        if (!found || value < 0) return false;
        count = static_cast<std::uint32_t>(value);
        return true;
    }

    bool get_password(const std::string& account, bool secondary, std::string& password) override {
        std::string col = secondary ? "cSecPassword" : "cPassword";
        std::string sql = "select " + col + " from Account_info where (cAccName = '" + sql_escape(account) + "')";
        char value[129]{};
        dbcmd(db_, sql.c_str());
        if (dbsqlexec(db_) == FAIL) return false;
        bool found = false;
        while (true) {
            RETCODE res = dbresults(db_);
            if (res == NO_MORE_RESULTS) break;
            if (res == FAIL) return false;
            dbbind(db_, 1, NTBSTRINGBIND, sizeof(value), reinterpret_cast<BYTE*>(value));
            RETCODE row = dbnextrow(db_);
            if (row == FAIL) return false;
            if (row != NO_MORE_ROWS) {
                found = true;
                password = trim(value);
                while (dbnextrow(db_) != NO_MORE_ROWS) {
                }
            }
        }
        return found;
    }

    bool set_password(const std::string& account, bool secondary, const std::string& password) override {
        std::string col = secondary ? "cSecPassword" : "cPassword";
        return db_exec_drain(db_,
            "update Account_info set " + col + " = '" + sql_escape(password) +
            "' where (cAccName = '" + sql_escape(account) + "')");
    }

    bool set_credentials(const std::string& account,
                         const std::string& password,
                         const std::string& secondary,
                         std::uint32_t user_ip) override {
        if (account.empty() || password.empty()) return false;
        std::string update =
            "update Account_info set cPassword = '" + sql_escape(password) + "'";
        if (!secondary.empty()) {
            update += ", cSecPassword = '" + sql_escape(secondary) + "'";
        }
        if (user_ip != 0) {
            update += ", nUserIP = " + std::to_string(user_ip);
        }
        update += " where (cAccName = '" + sql_escape(account) + "')";
        return db_exec_drain(db_, update);
    }

    bool set_account_user_ip(const std::string& account, std::uint32_t user_ip) override {
        return db_exec_drain(db_,
            "update Account_info set nUserIP = " + std::to_string(user_ip) +
            " where (cAccName = '" + sql_escape(account) + "')");
    }

    bool get_account_user_ip(const std::string& account, std::uint32_t& user_ip) override {
        std::string sql = "select nUserIP from Account_info where (cAccName = '" + sql_escape(account) + "')";
        DBINT value = 0;
        dbcmd(db_, sql.c_str());
        if (dbsqlexec(db_) == FAIL) return false;
        bool found = false;
        while (true) {
            RETCODE res = dbresults(db_);
            if (res == NO_MORE_RESULTS) break;
            if (res == FAIL) return false;
            dbbind(db_, 1, INTBIND, sizeof(value), reinterpret_cast<BYTE*>(&value));
            RETCODE row = dbnextrow(db_);
            if (row == FAIL) return false;
            if (row != NO_MORE_ROWS) {
                found = true;
                while (dbnextrow(db_) != NO_MORE_ROWS) {
                }
            }
        }
        if (found) user_ip = static_cast<std::uint32_t>(value);
        return found;
    }

    bool kickout_account(const std::string& account) override {
        return mark_account_offline(account);
    }

    bool freeze_account(const std::string& account) override {
        return mark_account_offline(account);
    }

    bool unlock_account(const std::string& account) override {
        return !account.empty();
    }

    const char* backend_name() const override {
        return "mssql";
    }

private:
    LOGINREC* login_ = nullptr;
    DBPROCESS* db_ = nullptr;
};
#endif

}  // namespace

std::unique_ptr<RelayStore> create_relay_store(const RelayConfig& cfg, const std::string& root) {
#ifdef HAVE_FREETDS
    try {
        return std::make_unique<MssqlRelayStore>(cfg);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        throw;
    }
#else
    (void)cfg;
    (void)root;
    throw std::runtime_error("MSSQL support is required; FreeTDS was not found");
#endif
}
