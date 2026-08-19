# sword3paysys_cpp

C++ PaySys replacement using the original MSSQL account schema and Heaven
encrypted protocol.

## Runtime

- Requires FreeTDS/DB-Lib at build time.
- Requires MSSQL database `account_tong` at runtime.
- Reads account credentials, online state, billing time and logout state from
  `Account_Info`, `Account_Habitus` and the configured Windows view/schema.
- Password enforcement is controlled by `enforce_password` and supports the
  legacy client credential representation.

The service intentionally fails fast when MSSQL/FreeTDS is unavailable. There is
no file-backed account runtime.

Build through the repository Compose configuration:

```bash
docker compose -f ../docker-compose.yml \
  -f ../docker-compose.gateway.yml build paysys
```

Configuration is read from the mounted `config/` directory. The service listens
on port `5002` and exposes a protocol-level healthcheck.
