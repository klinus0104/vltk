# Runtime and fixture operations

## Default runtime

The default Gateway topology uses `s3relay_ref` with `S3RELAY_IMPL=cpp` and
`paysys` with `PAY_SYS_IMPL=cpp`. Both services expose protocol-level Docker
healthchecks. Start it together with the database services using:

```sh
docker compose -f docker-compose.yml -f docker-compose.gateway.yml up -d --build
```

The default topology does not start Wine, MDAC, Xvfb, or a proprietary binary.

## Reference comparison profile

The Windows Relay reference is opt-in through the `reference` profile. To run
it for a controlled differential capture, start the profile and override the
Gateway target explicitly:

```sh
S3RELAY_TARGET=10.211.55.3:7777 \
  docker compose -f docker-compose.yml \
  -f docker-compose.gateway-ref-windows.yml up -d s3relay_windows_ref
```

The original PaySys and Relay binaries are otherwise launched only by the
isolated differential runners under `scripts/run-*-differential.sh`.

## Fixture reset and rollback

Differential tests reset disposable databases before each target. Reapply the
PaySys fixture with `scripts/run-paysys-differential.sh`; reapply the Relay
fixture with `scripts/run-s3relay-differential.sh`. These commands create and
remove their own temporary containers, networks, and config directories.

To roll back the runtime selection without changing source files, set
`S3RELAY_TARGET` to the explicitly started reference endpoint, or stop the
`reference` profile and restart the default C++ services. Never point a
production database at the disposable `vltk_paysys_diff` or differential Relay
fixtures.

## Resilience checks

- `scripts/run-delayed-mssql-cold-boot.sh` validates startup after a delayed
  MSSQL endpoint.
- `scripts/service-soak.py` defaults to a two-hour concurrent heartbeat and
  reconnect run; use a shorter duration only for smoke validation.
- `scripts/run-relay-protocol-tests.sh` includes bounded malformed-frame fuzzing
  and route/account concurrency checks.
