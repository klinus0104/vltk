# VLTK Pay Runtime Guide

This guide covers local build, configuration, startup, verification, testing,
and troubleshooting for the reconstructed PaySys and S3Relay runtime.

## Requirements

- Docker Desktop or Docker Engine with Compose v2.
- Permission to run Docker commands.
- Repository configuration, protocol fixtures, and database setup files.
- For native builds: CMake, a C++17 compiler, and FreeTDS/DB-Lib.

The default topology does not require proprietary PaySys or Relay binaries.

## Build and start the default stack

```bash
docker compose -f docker-compose.yml \
  -f docker-compose.gateway.yml up -d --build
```

Inspect service state:

```bash
docker compose -f docker-compose.yml \
  -f docker-compose.gateway.yml ps
```

The default ports are MySQL `3306`, MSSQL `1433`, PaySys `5002`, Relay `5003`,
and Gateway `5001`, `5622`, `5623`, `5632`, and `6666`.

Gateway normally reaches Relay through `10.211.55.4:5003`, allowing native Relay
peer validation to observe the expected Gateway identity. Override it for a
special topology with `S3RELAY_TARGET`.

## Healthchecks

```bash
python3 scripts/service-healthcheck.py paysys --port 5002
python3 scripts/service-healthcheck.py relay --port 5003
docker compose -f docker-compose.yml -f docker-compose.gateway.yml ps
```

Healthchecks validate business responses, not merely open TCP sockets.

## Database configuration

PaySys and S3Relay use MSSQL through FreeTDS. Gateway/GameServer uses MySQL.
Runtime settings are kept in `config/Acc_Setup.ini`, `config/Relay_Setup.ini`,
and `config/mssql.ini`. Do not place plaintext secrets in `README.md`,
`GUIDE.md`, or `WORKLOG.md`.

## Test suites

```bash
# Differential fixtures
./scripts/run-differential-tests.sh

# Relay MSSQL smoke, negative-path, golden-vector, and fuzz checks
./scripts/run-relay-protocol-tests.sh

# PaySys transaction and rollback checks
./scripts/run-paysys-atomic-tests.sh

# Delayed MSSQL cold boot
./scripts/run-delayed-mssql-cold-boot.sh

# Delayed MySQL Gateway cold boot
./scripts/run-delayed-mysql-gateway-cold-boot.sh
```

The Relay MSSQL harness provisions its disposable schema from
`tests/relay_harness/relay_mssql_fixture.sql`. Legacy lifecycle/routing cases
that assert CSV-only envelopes are not part of the native MSSQL suite.

## Reference comparison mode

Start the Windows/Wine reference Relay only when differential comparison is
required:

```bash
S3RELAY_TARGET=10.211.55.3:7777 \
  docker compose \
  -f docker-compose.yml -f docker-compose.gateway-ref-windows.yml \
  up -d s3relay_windows_ref
```

Differential manifests are stored under `tests/differential/`.

## Soak test

The two-hour PaySys/Relay soak remains an explicit deferred TODO and is not run
by normal builds:

```bash
python3 scripts/service-soak.py --duration 7200
```

## Troubleshooting

View recent logs:

```bash
docker logs --tail=200 jx_paysys_cpp
docker logs --tail=200 jx_s3relay_cpp
docker logs --tail=200 jx_gateway
```

If a client reports that the server is full or under maintenance, first verify
Relay health and Gateway-to-Relay routing, then inspect the Relay logs for peer
identity or database verification failures. If login fails, confirm the account
exists in MSSQL and that `enforce_password=1` matches the stored credential
format expected by the client.

For rollback procedures, fixture reset instructions, and operational notes see
[`docs/OPERATIONS.md`](docs/OPERATIONS.md). Record substantive findings in
[`WORKLOG.md`](WORKLOG.md).
