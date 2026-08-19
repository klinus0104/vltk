#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="${SOURCE_DIR:-/work/windows/paysyswin}"
CAPTURE_DIR="${CAPTURE_DIR:-/captures}"
MSSQL_TARGET="${MSSQL_TARGET:-mssql:1433}"
MSSQL_WAIT_TIMEOUT="${MSSQL_WAIT_TIMEOUT:-180}"
CAPTURE_DAY="$(date -u +%Y%m%d)"
export WINEARCH=win32
export WINEPREFIX="${WINEPREFIX:-/wineprefix}"
export WINEDEBUG="${WINEDEBUG:--all}"
export DISPLAY="${DISPLAY:-:99}"
export GNUTLS_SYSTEM_PRIORITY_FILE="/runtime/priorityGNU"
WINE_LOADER="${WINE_LOADER:-/usr/lib/wine/wine}"

mkdir -p /runtime "${WINEPREFIX}" \
  "${CAPTURE_DIR}/logs/windows-relay" "${CAPTURE_DIR}/pcap"
# A restarted container can retain the log symlink in its writable layer. Remove
# it before copying the Windows distribution so cp never sees a directory being
# copied onto a symlink.
rm -rf /runtime/relayserver_log
cp -a "${SOURCE_DIR}/." /runtime/
rm -rf /runtime/relayserver_log
mkdir -p "${CAPTURE_DIR}/logs/windows-relay/native"
ln -s "${CAPTURE_DIR}/logs/windows-relay/native" /runtime/relayserver_log

exec > >(while IFS= read -r line; do \
    printf '%s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "${line}"; \
  done | tee -a "${CAPTURE_DIR}/logs/windows-relay/wine_${CAPTURE_DAY}.log") 2>&1

wait_for_tcp() {
  local target="$1" timeout="$2" label="$3"
  local host="${target%:*}" port="${target##*:}" elapsed=0
  echo "[wine-relay] waiting for ${label} at ${host}:${port} (timeout ${timeout}s)"
  until timeout 1 bash -c "</dev/tcp/${host}/${port}" 2>/dev/null; do
    if (( elapsed >= timeout )); then
      echo "[wine-relay] ERROR: ${label} did not become ready within ${timeout}s"
      return 1
    fi
    sleep 1
    ((elapsed += 1))
  done
  echo "[wine-relay] ${label} TCP endpoint is ready"
}

wait_for_tcp "${MSSQL_TARGET}" "${MSSQL_WAIT_TIMEOUT}" MSSQL

echo "[wine-relay] forwarding 127.0.0.1:1433 -> ${MSSQL_TARGET}"
socat TCP-LISTEN:1433,bind=127.0.0.1,reuseaddr,fork TCP:"${MSSQL_TARGET}" &
wait_for_tcp "127.0.0.1:1433" 10 "local MSSQL forward"

echo "[wine-relay] initializing Xvfb on ${DISPLAY}"
Xvfb "${DISPLAY}" -screen 0 1024x768x16 -nolisten tcp &

if [[ ! -f "${WINEPREFIX}/.initialized" ]]; then
  echo "[wine-relay] initializing 32-bit Wine prefix"
  "${WINE_LOADER}" wineboot.exe --init
  touch "${WINEPREFIX}/.initialized"
fi

if [[ ! -f "${WINEPREFIX}/.mdac28-installed" ]]; then
  echo "[wine-relay] installing MDAC 2.8 for the relay's ADODB connection"
  winetricks -q mdac28
  touch "${WINEPREFIX}/.mdac28-installed"
fi

if [[ ! -f "${WINEPREFIX}/.sqloledb-registered" ]]; then
  echo "[wine-relay] registering the legacy SQL OLE DB provider"
  wine regsvr32 'C:\Program Files\Common Files\System\OLE DB\sqloledb.dll'
  touch "${WINEPREFIX}/.sqloledb-registered"
fi

echo "[wine-relay] capturing TCP port 7777"
tcpdump -i any -U -n -s 0 -G 3600 \
  -w "${CAPTURE_DIR}/pcap/windows_relay_%Y%m%dT%H%M%SZ.pcap" \
  '(tcp port 7777 or tcp port 1433)' &

echo "[wine-relay] starting S3RelayServer.exe"
exec "${WINE_LOADER}" /runtime/S3RelayServer.exe
