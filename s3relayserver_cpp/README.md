# s3relayserver_cpp

C++ S3Relay replacement using the original MSSQL Relay schema and Heaven
encrypted protocol.

## Runtime

- Requires FreeTDS/DB-Lib at build time.
- Requires MSSQL database `account_tong` and the configured Relay server tables.
- Implements strict server verification, account/session state, route lookup,
  forwarding, heartbeat and logout handling.
- Uses the native Windows Relay wire opcodes without a file-backed runtime.

The service intentionally fails fast when MSSQL/FreeTDS is unavailable. It
listens on port `5003` and exposes a protocol-level healthcheck.

Build through the repository Compose configuration:

```bash
docker compose -f ../docker-compose.yml \
  -f ../docker-compose.gateway.yml build s3relay_ref
```
