#include "account_store.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <cstring>
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

struct AccountRecord {
    std::string password;
    std::uint32_t ext_point = 0;
    std::uint32_t left_second = 360000;
    std::uint32_t end_time = 0;
};

struct OnlineSession {
    std::uint32_t client_id = 0;
    std::time_t login_time = 0;
    std::uint32_t initial_left = 0;
};

bool credential_matches(const std::string& client, const std::string& stored) {
    if (client == stored) return true;
    // The legacy client transmits the uppercase 29-character tail of its
    // 32-character MD5 credential; MSSQL stores the complete digest.
    return stored.size() == 32 && client.size() == 29 && stored.substr(3) == client;
}

std::string trim(std::string s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), is_space));
    s.erase(std::find_if_not(s.rbegin(), s.rend(), is_space).base(), s.end());
    return s;
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

#if 0  // File-backed account backend removed; production uses MSSQL only.
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

std::string config_path_join(const std::string& root, const std::string& path) {
    if (path.empty()) return root;
    if (path.front() == '/') return path;
    return root + "/" + path;
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

[[maybe_unused]] std::string sql_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        out.push_back(c);
        if (c == '\'') out.push_back('\'');
    }
    return out;
}

class CsvAccountStore final : public AccountStore {
public:
    CsvAccountStore(const AccountConfig& cfg, std::string csv_path)
        : accept_any_login_(cfg.accept_any_login),
          enforce_password_(cfg.enforce_password),
          realtime_charge_(cfg.realtime_charge),
          respect_enddate_(cfg.respect_enddate),
          charge_interval_(std::max(1, cfg.charge_interval)),
          csv_path_(std::move(csv_path)) {
        load();
    }

    LoginResult login(const std::string& account, const std::string& password, std::uint32_t client_id) override {
        LoginResult result;
        if (account.empty()) {
            result.n_return = 3;
            return result;
        }

        auto now = static_cast<std::uint32_t>(std::time(nullptr));
        auto it = accounts_.find(account);
        bool changed = false;
        if (it == accounts_.end()) {
            if (!accept_any_login_) {
                result.n_return = 3;
                return result;
            }
            AccountRecord rec;
            rec.password = password;
            rec.left_second = 360000;
            it = accounts_.emplace(account, rec).first;
            changed = true;
        }

        AccountRecord& rec = it->second;
        if (enforce_password_ && !rec.password.empty() && rec.password != password) {
            result.n_return = 4;
            result.ext_point = rec.ext_point;
            return result;
        }

        std::uint32_t seconds_until_end = rec.end_time > now ? rec.end_time - now : 0;
        std::uint32_t left = rec.left_second + seconds_until_end;
        if (left <= 0x708) left = 0x57e40;

        result.n_return = 1;
        result.ext_point = rec.ext_point;
        result.left_second = left;
        result.end_time = now + left;
        online_[account] = OnlineSession{client_id, std::time(nullptr), left};
        if (changed) save();
        return result;
    }

    void logout(const std::string& account) override {
        auto session_it = online_.find(account);
        if (session_it == online_.end()) return;

        std::time_t now = std::time(nullptr);
        std::uint32_t used = 0;
        if (now > session_it->second.login_time) {
            used = static_cast<std::uint32_t>(now - session_it->second.login_time);
        }

        if (apply_usage(account, used, now)) save();
        online_.erase(session_it);
    }

    void tick() override {
        if (!realtime_charge_) return;

        std::time_t now = std::time(nullptr);
        if (now - last_charge_time_ < charge_interval_) return;
        last_charge_time_ = now;

        bool changed = false;
        for (auto& [account, session] : online_) {
            if (now <= session.login_time) continue;
            std::uint32_t used = static_cast<std::uint32_t>(now - session.login_time);
            if (apply_usage(account, used, now)) changed = true;
            session.login_time = now;
        }
        if (changed) save();
    }

    const char* backend_name() const override {
        return "csv";
    }

private:
    bool apply_usage(const std::string& account, std::uint32_t used, std::time_t now) {
        if (used == 0) return false;
        auto account_it = accounts_.find(account);
        if (account_it == accounts_.end()) return false;

        AccountRecord& rec = account_it->second;
        if (respect_enddate_ && rec.end_time > static_cast<std::uint32_t>(now)) return false;

        rec.left_second = used >= rec.left_second ? 0 : rec.left_second - used;
        rec.end_time = 0;
        std::cerr << "[gio] '" << account << "' dung them " << used
                  << "s -> con " << rec.left_second << " giay ("
                  << (static_cast<double>(rec.left_second) / 3600.0) << " gio)\n";
        return true;
    }

    void load() {
        std::ifstream in(csv_path_);
        if (!in) {
            std::cerr << "account file not found: " << csv_path_
                      << " (accept_any_login=" << (accept_any_login_ ? "true" : "false") << ")\n";
            return;
        }

        std::string line;
        while (std::getline(in, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            auto cols = split_csv_line(line);
            if (cols.empty() || cols[0] == "account") continue;

            AccountRecord rec;
            if (cols.size() > 1) rec.password = cols[1];
            if (cols.size() > 2 && !cols[2].empty()) rec.ext_point = static_cast<std::uint32_t>(std::stoul(cols[2]));
            if (cols.size() > 3 && !cols[3].empty()) rec.left_second = static_cast<std::uint32_t>(std::stoul(cols[3]));
            if (cols.size() > 4 && !cols[4].empty()) rec.end_time = static_cast<std::uint32_t>(std::stoul(cols[4]));
            accounts_[cols[0]] = rec;
        }
        std::cerr << "loaded " << accounts_.size() << " account records from " << csv_path_ << "\n";
    }

    bool save() const {
        if (csv_path_.empty()) return false;
        std::string tmp_path = csv_path_ + ".tmp";
        {
            std::ofstream out(tmp_path, std::ios::trunc);
            if (!out) {
                std::cerr << "failed to write account csv temp file: " << tmp_path << "\n";
                return false;
            }

            out << "account,password,ext_point,left_second,end_time\n";
            std::vector<std::string> keys;
            keys.reserve(accounts_.size());
            for (const auto& [account, _] : accounts_) keys.push_back(account);
            std::sort(keys.begin(), keys.end());

            for (const std::string& account : keys) {
                const AccountRecord& rec = accounts_.at(account);
                out << csv_escape(account) << ','
                    << csv_escape(rec.password) << ','
                    << rec.ext_point << ','
                    << rec.left_second << ','
                    << rec.end_time << '\n';
            }
        }

        if (std::rename(tmp_path.c_str(), csv_path_.c_str()) != 0) {
            std::cerr << "failed to replace account csv file: " << csv_path_ << "\n";
            std::remove(tmp_path.c_str());
            return false;
        }
        return true;
    }

    bool accept_any_login_ = true;
    bool enforce_password_ = true;
    bool realtime_charge_ = false;
    bool respect_enddate_ = false;
    int charge_interval_ = 5;
    std::time_t last_charge_time_ = 0;
    std::string csv_path_;
    std::unordered_map<std::string, AccountRecord> accounts_;
    std::unordered_map<std::string, OnlineSession> online_;
};

#endif

#ifdef HAVE_FREETDS
int db_error_handler(DBPROCESS*, int severity, int dberr, int oserr,
                     char* dberrstr, char* oserrstr) {
    std::cerr << "MSSQL DB-Lib error severity=" << severity << " dberr=" << dberr
              << " oserr=" << oserr;
    if (dberrstr && *dberrstr) std::cerr << " message='" << dberrstr << "'";
    if (oserrstr && *oserrstr) std::cerr << " os_message='" << oserrstr << "'";
    std::cerr << "\n";
    // FreeTDS' default handler returns INT_EXIT for a dead connection.  A
    // database restart must fail the current request, not terminate PaySys.
    return INT_CANCEL;
}

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

bool db_select_int(DBPROCESS* db, const std::string& sql, DBINT& value) {
    if (!db) return false;
    value = 0;
    dbcmd(db, sql.c_str());
    if (dbsqlexec(db) == FAIL) return false;

    bool found = false;
    while (true) {
        RETCODE result = dbresults(db);
        if (result == NO_MORE_RESULTS) return found;
        if (result == FAIL) return false;

        dbbind(db, 1, INTBIND, sizeof(value), reinterpret_cast<BYTE*>(&value));
        while (true) {
            RETCODE row = dbnextrow(db);
            if (row == NO_MORE_ROWS) break;
            if (row == FAIL) return false;
            found = true;
        }
    }
}

bool db_has_rows(DBPROCESS* db, const std::string& sql, bool& has_rows) {
    if (!db) return false;
    has_rows = false;
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
            has_rows = true;
        }
    }
}

bool db_select_three_ints(DBPROCESS* db, const std::string& sql, DBINT& a, DBINT& b, DBINT& c) {
    if (!db) return false;
    a = 0;
    b = 0;
    c = 0;
    dbcmd(db, sql.c_str());
    if (dbsqlexec(db) == FAIL) return false;

    bool found = false;
    while (true) {
        RETCODE result = dbresults(db);
        if (result == NO_MORE_RESULTS) return found;
        if (result == FAIL) return false;

        dbbind(db, 1, INTBIND, sizeof(a), reinterpret_cast<BYTE*>(&a));
        dbbind(db, 2, INTBIND, sizeof(b), reinterpret_cast<BYTE*>(&b));
        dbbind(db, 3, INTBIND, sizeof(c), reinterpret_cast<BYTE*>(&c));
        while (true) {
            RETCODE row = dbnextrow(db);
            if (row == NO_MORE_ROWS) break;
            if (row == FAIL) return false;
            found = true;
        }
    }
}

class MssqlAccountStore final : public AccountStore {
public:
    explicit MssqlAccountStore(const AccountConfig& cfg)
        : realtime_charge_(cfg.realtime_charge),
          respect_enddate_(cfg.respect_enddate),
          enforce_password_(cfg.enforce_password),
          windows_schema_(cfg.account_schema == "windows"),
          charge_interval_(std::max(1, cfg.charge_interval)),
          db_server_(cfg.db_server),
          db_name_(cfg.db_name) {
        if (dbinit() == FAIL) throw std::runtime_error("MSSQL: dbinit failed");
        dberrhandle(db_error_handler);

        login_ = dblogin();
        if (!login_) throw std::runtime_error("MSSQL: dblogin failed");

        DBSETLUSER(login_, const_cast<char*>(cfg.db_user.c_str()));
        DBSETLPWD(login_, const_cast<char*>(cfg.db_password.c_str()));
        DBSETLAPP(login_, const_cast<char*>("sword3paysys_cpp"));
        DBSETLCHARSET(login_, const_cast<char*>(cfg.db_charset.c_str()));

        if (!reconnect()) {
            throw std::runtime_error("MSSQL: dbuse failed for " + cfg.db_name);
        }

        std::cerr << "MSSQL connected " << cfg.db_user << "@" << cfg.db_server << "/" << cfg.db_name
                  << " account_schema=" << (windows_schema_ ? "windows" : "linux")
                  << " enforce_password=" << (enforce_password_ ? "true" : "false") << "\n";
        reset_online_state();
    }

    ~MssqlAccountStore() override {
        if (db_) dbclose(db_);
        if (login_) dbloginfree(login_);
    }

    LoginResult login(const std::string& account, const std::string& password, std::uint32_t client_id) override {
        if (!ensure_connection()) {
            LoginResult failed;
            failed.n_return = 2;
            return failed;
        }

        LoginResult result = windows_schema_
            ? login_windows_schema(account, password, client_id)
            : login_linux_schema(account, password, client_id);
        if (result.n_return == 2 && reconnect()) {
            std::cerr << "MSSQL: retrying account login after reconnect\n";
            result = windows_schema_
                ? login_windows_schema(account, password, client_id)
                : login_linux_schema(account, password, client_id);
        }
        return result;
    }

    LoginResult login_linux_schema(const std::string& account, const std::string& password,
                                   std::uint32_t client_id) {

        LoginResult result;
        if (!db_) return result;
        if (account.empty()) {
            result.n_return = 3;
            return result;
        }

        std::string escaped = sql_escape(account);
        std::string sql =
            "SELECT a.cPassWord, a.nExtPoint, a.iClientID, "
            "ISNULL(h.iLeftSecond,0), "
            "ISNULL(DATEDIFF(second, GETDATE(), h.dEndDate),0) "
            "FROM Account_Info a "
            "LEFT JOIN Account_Habitus h ON a.cAccName=h.cAccName "
            "WHERE a.cAccName='" + escaped + "'";

        char db_password[65]{};
        DBINT ext_point = 0;
        DBINT db_client_id = 0;
        DBINT left_second = 0;
        DBINT end_remaining = 0;

        dbcmd(db_, sql.c_str());
        if (dbsqlexec(db_) == FAIL) {
            result.n_return = 2;
            return result;
        }

        bool row_found = false;
        while (true) {
            RETCODE res = dbresults(db_);
            if (res == NO_MORE_RESULTS) break;
            if (res == FAIL) {
                result.n_return = 2;
                return result;
            }

            dbbind(db_, 1, STRINGBIND, sizeof(db_password), reinterpret_cast<BYTE*>(db_password));
            dbbind(db_, 2, INTBIND, sizeof(ext_point), reinterpret_cast<BYTE*>(&ext_point));
            dbbind(db_, 3, INTBIND, sizeof(db_client_id), reinterpret_cast<BYTE*>(&db_client_id));
            dbbind(db_, 4, INTBIND, sizeof(left_second), reinterpret_cast<BYTE*>(&left_second));
            dbbind(db_, 5, INTBIND, sizeof(end_remaining), reinterpret_cast<BYTE*>(&end_remaining));

            RETCODE row = dbnextrow(db_);
            if (row == FAIL) {
                result.n_return = 2;
                return result;
            }
            if (row != NO_MORE_ROWS) {
                row_found = true;
                while (dbnextrow(db_) != NO_MORE_ROWS) {
                }
            }
        }

        if (!row_found) {
            result.n_return = 3;
            return result;
        }

        std::string stored_password(db_password, strnlen(db_password, sizeof(db_password)));
        while (!stored_password.empty() && stored_password.back() == ' ') stored_password.pop_back();
        if (enforce_password_ && !stored_password.empty() && !credential_matches(password, stored_password)) {
            result.n_return = 4;
            result.ext_point = ext_point > 0 ? static_cast<std::uint32_t>(ext_point) : 0;
            return result;
        }

        std::uint32_t left = 0;
        if (end_remaining > 0) left += static_cast<std::uint32_t>(end_remaining);
        if (left_second > 0) left += static_cast<std::uint32_t>(left_second);
        if (left <= 0x708) left = 0x57e40;

        std::string update =
            "UPDATE Account_Info SET iClientID=" + std::to_string(client_id) +
            ", dLoginDate=GETDATE() WHERE cAccName='" + escaped + "'";
        if (!db_exec_drain(db_, update)) {
            result.n_return = 2;
            return result;
        }

        auto now = static_cast<std::uint32_t>(std::time(nullptr));
        result.n_return = 1;
        result.ext_point = ext_point > 0 ? static_cast<std::uint32_t>(ext_point) : 0;
        result.left_second = left;
        result.end_time = now + left;
        online_[account] = OnlineSession{client_id, std::time(nullptr), left};
        return result;
    }

    void logout(const std::string& account) override {
        auto session_it = online_.find(account);
        std::uint32_t used = 0;
        std::uint32_t client_id = 0;
        if (session_it != online_.end()) {
            client_id = session_it->second.client_id;
            std::time_t now = std::time(nullptr);
            if (now > session_it->second.login_time) {
                used = static_cast<std::uint32_t>(now - session_it->second.login_time);
            }
        }

        if (!commit_logout(account, client_id, used)) {
            std::cerr << "MSSQL: atomic logout failed for " << account
                      << "; preserving in-memory session for retry\n";
            return;
        }
        if (session_it != online_.end()) online_.erase(session_it);
    }

    void tick() override {
        if (!realtime_charge_) return;

        std::time_t now = std::time(nullptr);
        if (now - last_charge_time_ < charge_interval_) return;
        last_charge_time_ = now;

        for (auto& [account, session] : online_) {
            if (now <= session.login_time) continue;
            std::uint32_t used = static_cast<std::uint32_t>(now - session.login_time);
            if (update_habitus_usage(account, used)) {
                session.login_time = now;
            }
        }
    }

    const char* backend_name() const override {
        return "mssql";
    }

private:
    bool commit_logout(const std::string& account,
                       std::uint32_t client_id,
                       std::uint32_t used_seconds) {
        if (account.empty() || !ensure_connection()) return false;
        const std::string escaped = sql_escape(account);
        const std::string used = std::to_string(used_seconds);

        std::string mutations;
        if (used_seconds > 0) {
            if (windows_schema_) {
                mutations =
                    "UPDATE View_AccountMoney SET iLeftSecond = iLeftSecond - " + used +
                    " WHERE (cAccName = '" + escaped + "') "
                    "AND (DATEDIFF(second, GETDATE(), dEndDate) <= 0) "
                    "AND (iClientID = " + std::to_string(client_id) + "); ";
            } else {
                mutations =
                    "UPDATE Account_Habitus SET "
                    "iUseSecond = ISNULL(iUseSecond,0) + CASE WHEN iLeftSecond >= " + used +
                    " THEN " + used + " ELSE iLeftSecond END, "
                    "iLeftSecond = CASE WHEN iLeftSecond >= " + used +
                    " THEN iLeftSecond - " + used + " ELSE 0 END "
                    "WHERE cAccName='" + escaped + "' AND ISNULL(iLeftSecond,0) > 0";
                if (respect_enddate_) {
                    mutations += " AND DATEDIFF(second, GETDATE(), dEndDate) <= 0";
                }
                mutations += "; ";
            }
        }
        mutations +=
            "UPDATE Account_Info SET iClientID=0, dLogoutDate=GETDATE() "
            "WHERE cAccName='" + escaped + "'; ";

        const std::string transaction =
            "SET XACT_ABORT ON; "
            "BEGIN TRY "
            "BEGIN TRANSACTION; " + mutations +
            "COMMIT TRANSACTION; "
            "END TRY "
            "BEGIN CATCH "
            "IF XACT_STATE() <> 0 ROLLBACK TRANSACTION; "
            "DECLARE @error_message nvarchar(4000); "
            "SET @error_message = ERROR_MESSAGE(); "
            "RAISERROR(N'%s', 16, 1, @error_message); "
            "END CATCH";
        if (!db_exec_drain(db_, transaction)) return false;

        if (used_seconds > 0) {
            if (windows_schema_) {
                std::cerr << "[gio] '" << account << "' dung them " << used_seconds
                          << "s theo View_AccountMoney (atomic logout)\n";
            } else {
                DBINT after_left = 0;
                if (select_left_second(escaped, after_left)) {
                    std::cerr << "[gio] '" << account << "' dung them " << used_seconds
                              << "s -> con " << after_left << " giay ("
                              << (static_cast<double>(after_left) / 3600.0)
                              << " gio, atomic logout)\n";
                }
            }
        }
        return true;
    }

    bool reconnect() {
        if (db_) {
            dbclose(db_);
            db_ = nullptr;
        }
        db_ = dbopen(login_, const_cast<char*>(db_server_.c_str()));
        if (!db_) {
            std::cerr << "MSSQL: reconnect dbopen failed for " << db_server_ << "\n";
            return false;
        }
        if (dbuse(db_, const_cast<char*>(db_name_.c_str())) == FAIL) {
            std::cerr << "MSSQL: reconnect dbuse failed for " << db_name_ << "\n";
            dbclose(db_);
            db_ = nullptr;
            return false;
        }
        return true;
    }

    bool ensure_connection() {
        if (db_ && !DBDEAD(db_)) return true;
        std::cerr << "MSSQL: connection is dead; reconnecting\n";
        return reconnect();
    }

    void reset_online_state() {
        if (!db_) return;
        std::string sql = "UPDATE Account_Info SET iClientID=0, dLogoutDate=GETDATE() WHERE iClientID<>0";
        if (!db_exec_drain(db_, sql)) {
            std::cerr << "MSSQL: reset iClientID=0 failed\n";
        } else {
            std::cerr << "MSSQL: reset iClientID=0 for online accounts\n";
        }
    }

    void update_account_logout(const std::string& account) {
        if (!db_ || account.empty()) return;
        std::string escaped = sql_escape(account);

        std::string logout =
            "UPDATE Account_Info SET iClientID=0, dLogoutDate=GETDATE() "
            "WHERE cAccName='" + escaped + "'";
        if (!db_exec_drain(db_, logout)) {
            std::cerr << "MSSQL: logout Account_Info update failed for " << account << "\n";
        }
    }

    bool update_habitus_usage(const std::string& account, std::uint32_t used_seconds) {
        if (windows_schema_) return update_windows_usage(account, used_seconds);
        if (!db_ || account.empty() || used_seconds == 0) return false;
        std::string escaped = sql_escape(account);
        std::string used = std::to_string(used_seconds);
        DBINT before_left = 0;
        bool have_before_left = select_left_second(escaped, before_left);
        std::string habitus =
            "UPDATE Account_Habitus SET "
            "iUseSecond = ISNULL(iUseSecond,0) + CASE WHEN iLeftSecond >= " + used +
            " THEN " + used + " ELSE iLeftSecond END, "
            "iLeftSecond = CASE WHEN iLeftSecond >= " + used +
            " THEN iLeftSecond - " + used + " ELSE 0 END "
            "WHERE cAccName='" + escaped + "' AND ISNULL(iLeftSecond,0) > 0";
        if (respect_enddate_) {
            habitus += " AND DATEDIFF(second, GETDATE(), dEndDate) <= 0";
        }
        if (!db_exec_drain(db_, habitus)) {
            std::cerr << "MSSQL: Account_Habitus update failed for " << account << "\n";
            return false;
        }
        DBINT after_left = 0;
        bool have_after_left = select_left_second(escaped, after_left);
        if (have_after_left) {
            std::cerr << "[gio] '" << account << "' dung them " << used_seconds
                      << "s -> con " << after_left << " giay ("
                      << (static_cast<double>(after_left) / 3600.0) << " gio)\n";
        } else if (have_before_left) {
            DBINT estimated = before_left > static_cast<DBINT>(used_seconds)
                                  ? before_left - static_cast<DBINT>(used_seconds)
                                  : 0;
            std::cerr << "[gio] '" << account << "' dung them " << used_seconds
                      << "s -> con " << estimated << " giay (estimated)\n";
        }
        return true;
    }

    bool select_left_second(const std::string& escaped_account, DBINT& left_second) {
        std::string sql =
            "SELECT ISNULL(iLeftSecond,0) FROM Account_Habitus WHERE cAccName='" + escaped_account + "'";
        return db_select_int(db_, sql, left_second);
    }

    LoginResult login_windows_schema(const std::string& account, const std::string& password, std::uint32_t client_id) {
        LoginResult result;
        if (!db_) return result;
        if (account.empty()) {
            result.n_return = 3;
            return result;
        }

        std::string escaped_account = sql_escape(account);
        std::string escaped_password = sql_escape(password);

        DBINT current_client = 0;
        if (db_select_int(db_,
                "select iClientID from Account_info where (cAccName = '" + escaped_account + "')",
                current_client) &&
            current_client > 0 && current_client != static_cast<DBINT>(client_id)) {
            result.n_return = 6;
            return result;
        }

        if (enforce_password_) {
            bool password_ok = false;
            std::string password_sql =
                "select cAccName from Account_info where (cAccName = '" + escaped_account +
                "') and (cSecPassword COLLATE Chinese_PRC_CS_AS = '" + escaped_password + "')";
            if (!db_has_rows(db_, password_sql, password_ok)) {
                result.n_return = 2;
                return result;
            }
            if (!password_ok) {
                result.n_return = 3;
                return result;
            }
        }

        DBINT end_remaining = 0;
        DBINT left_second = 0;
        DBINT ext_point = 0;
        std::string money_sql =
            "select datediff(second, getdate(), dEndDate), iLeftSecond, nExtPoint "
            "from View_AccountMoney where (cAccname = '" + escaped_account + "')";
        if (!db_select_three_ints(db_, money_sql, end_remaining, left_second, ext_point)) {
            result.n_return = 5;
            return result;
        }

        std::uint32_t left = 0;
        if (end_remaining > 0) left += static_cast<std::uint32_t>(end_remaining);
        if (left_second > 0) left += static_cast<std::uint32_t>(left_second);
        if (left <= 0x708) left = 0x57e40;

        std::string update =
            "UPDATE Account_Info SET iClientID=" + std::to_string(client_id) +
            ", dLoginDate=GETDATE() WHERE cAccName='" + escaped_account + "'";
        if (!db_exec_drain(db_, update)) {
            result.n_return = 2;
            return result;
        }

        auto now = static_cast<std::uint32_t>(std::time(nullptr));
        result.n_return = 1;
        result.ext_point = ext_point > 0 ? static_cast<std::uint32_t>(ext_point) : 0;
        result.left_second = left;
        result.end_time = now + left;
        online_[account] = OnlineSession{client_id, std::time(nullptr), left};
        return result;
    }

    bool update_windows_usage(const std::string& account, std::uint32_t used_seconds) {
        if (!db_ || account.empty() || used_seconds == 0) return false;
        auto it = online_.find(account);
        std::uint32_t client_id = it == online_.end() ? 0 : it->second.client_id;
        std::string escaped = sql_escape(account);
        std::string sql =
            "update View_AccountMoney set iLeftSecond = iLeftSecond - " + std::to_string(used_seconds) +
            " where (cAccName = '" + escaped + "') "
            "and (datediff(second, getdate(), dEndDate) <= 0) "
            "and (iClientID = " + std::to_string(client_id) + ")";
        if (!db_exec_drain(db_, sql)) {
            std::cerr << "MSSQL: Windows View_AccountMoney update failed for " << account << "\n";
            return false;
        }
        std::cerr << "[gio] '" << account << "' dung them " << used_seconds
                  << "s theo View_AccountMoney\n";
        return true;
    }

    LOGINREC* login_ = nullptr;
    DBPROCESS* db_ = nullptr;
    bool realtime_charge_ = false;
    bool respect_enddate_ = false;
    bool enforce_password_ = true;
    bool windows_schema_ = false;
    int charge_interval_ = 5;
    std::time_t last_charge_time_ = 0;
    std::string db_server_;
    std::string db_name_;
    std::unordered_map<std::string, OnlineSession> online_;
};
#endif

}  // namespace

std::unique_ptr<AccountStore> create_account_store(const AccountConfig& cfg, const std::string& root) {
#ifdef HAVE_FREETDS
    try {
        return std::make_unique<MssqlAccountStore>(cfg);
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
