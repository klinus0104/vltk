#!/usr/bin/env bash
set -euo pipefail

MSSQL_TARGET="${MSSQL_TARGET:-mssql:1433}"
CAPTURE_DIR="${CAPTURE_DIR:-/captures}"
CAPTURE_DAY="$(date -u +%Y%m%d)"
PAY_SYS_IMPL="${PAY_SYS_IMPL:-reference}"
PAY_SYS_CONFIG_DIR="${PAY_SYS_CONFIG_DIR:-/work/config}"
MSSQL_WAIT_TIMEOUT="${MSSQL_WAIT_TIMEOUT:-180}"
mkdir -p "${CAPTURE_DIR}/logs"

# The reference binary prints decrypted packet bodies here. Preserve them with
# UTC timestamps so they can be correlated directly with the PCAP and timeline.
exec > >(while IFS= read -r line; do \
    printf '%s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "${line}"; \
  done | tee -a "${CAPTURE_DIR}/logs/paysys_${PAY_SYS_IMPL}_${CAPTURE_DAY}.log") 2>&1

wait_for_tcp() {
  local target="$1" timeout="$2" label="$3"
  local host="${target%:*}" port="${target##*:}" elapsed=0
  echo "[paysys] waiting for ${label} at ${host}:${port} (timeout ${timeout}s)"
  until timeout 1 bash -c "</dev/tcp/${host}/${port}" 2>/dev/null; do
    if (( elapsed >= timeout )); then
      echo "[paysys] ERROR: ${label} did not become ready within ${timeout}s"
      return 1
    fi
    sleep 1
    ((elapsed += 1))
  done
  echo "[paysys] ${label} TCP endpoint is ready"
}

wait_for_tcp "${MSSQL_TARGET}" "${MSSQL_WAIT_TIMEOUT}" MSSQL

echo "[paysys] forwarding 127.0.0.1:1433 -> ${MSSQL_TARGET}"
socat TCP-LISTEN:1433,bind=127.0.0.1,reuseaddr,fork TCP:"${MSSQL_TARGET}" &
SOCAT_PID=$!
trap 'kill "${SOCAT_PID}" 2>/dev/null || true' EXIT
wait_for_tcp "127.0.0.1:1433" 10 "local MSSQL forward"

if [[ "${PAY_SYS_IMPL}" == "cpp" ]]; then
  echo "[paysys] starting sword3paysys_cpp"
  exec /usr/local/bin/sword3paysys-cpp "${PAY_SYS_CONFIG_DIR}"
fi

cp /work/sword3paysys.unpacked /tmp/sword3paysys
chmod +x /tmp/sword3paysys
echo "[paysys] starting reference sword3paysys.unpacked"
exec /tmp/sword3paysys "${PAY_SYS_CONFIG_DIR}"
