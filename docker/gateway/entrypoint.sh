#!/usr/bin/env bash
set -euo pipefail

cd /home/jxser/gateway

CAPTURE_DIR="${CAPTURE_DIR:-/captures}"
CAPTURE_DAY="$(date -u +%Y%m%d)"
mkdir -p "${CAPTURE_DIR}/pcap" "${CAPTURE_DIR}/logs"

# Keep a persistent, UTC-timestamped copy in addition to Docker's own logs.
exec > >(while IFS= read -r line; do \
    printf '%s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "${line}"; \
  done | tee -a "${CAPTURE_DIR}/logs/gateway_${CAPTURE_DAY}.log") 2>&1

PCAP_PREFIX="${CAPTURE_DIR}/pcap/gateway_%Y%m%dT%H%M%SZ.pcap"
PCAP_FILTER='tcp port 5001 or tcp port 5002 or tcp port 5003 or tcp port 5004 or tcp port 5005 or tcp port 5622 or tcp port 5623 or tcp port 5632 or tcp port 6666 or tcp port 7777'
echo "[capture] starting tcpdump on any: ${PCAP_FILTER}"
tcpdump -i any -U -n -s 0 -G 3600 -w "${PCAP_PREFIX}" ${PCAP_FILTER} &
TCPDUMP_PID=$!
echo "[capture] tcpdump pid=${TCPDUMP_PID}; rotating PCAP every hour"

chmod +x ./goddess_y ./bishop_y 2>/dev/null || true
chmod +x ./s3relay/s3relay_y 2>/dev/null || true
chmod +x /home/jxser/server1/jx_linux_y 2>/dev/null || true
mkdir -p /tmp/jx-gateway-bin
cp -f ./goddess_y /tmp/jx-gateway-bin/goddess_y
cp -f ./bishop_y /tmp/jx-gateway-bin/bishop_y
chmod +x /tmp/jx-gateway-bin/goddess_y /tmp/jx-gateway-bin/bishop_y

PATCH_ELF_BSS="${PATCH_ELF_BSS:-0}"
if [[ "${PATCH_ELF_BSS}" == "1" ]]; then
  patch-elf-bss /tmp/jx-gateway-bin/goddess_y /tmp/jx-gateway-bin/bishop_y
fi

export LD_LIBRARY_PATH="/home/jxser/gateway:/home/jxser/gateway/s3relay:/home/jxser/server1:${LD_LIBRARY_PATH:-}"

PAY_SYS_TARGET="${PAY_SYS_TARGET:-host.docker.internal:5002}"
S3RELAY_TARGET="${S3RELAY_TARGET:-host.docker.internal:5003}"
MYSQL_TARGET="${MYSQL_TARGET:-host.docker.internal:3306}"
RUN_GODDESS="${RUN_GODDESS:-1}"
RUN_BISHOP="${RUN_BISHOP:-1}"
RUN_S3RELAY="${RUN_S3RELAY:-1}"
RUN_GAME="${RUN_GAME:-1}"
GAME_PUBLIC_IP="${GAME_PUBLIC_IP:-10.211.55.2}"
GAME_PORT="${GAME_PORT:-6666}"
DEPENDENCY_WAIT_TIMEOUT="${DEPENDENCY_WAIT_TIMEOUT:-180}"

wait_for_tcp() {
  local target="$1" timeout="$2" label="$3"
  local host="${target%:*}" port="${target##*:}" elapsed=0
  echo "[gateway] waiting for ${label} at ${host}:${port} (timeout ${timeout}s)"
  until timeout 1 bash -c "</dev/tcp/${host}/${port}" 2>/dev/null; do
    if (( elapsed >= timeout )); then
      echo "[gateway] ERROR: ${label} did not become ready within ${timeout}s"
      return 1
    fi
    sleep 1
    ((elapsed += 1))
  done
  echo "[gateway] ${label} TCP endpoint is ready"
}

echo "[gateway] cwd=$(pwd)"
echo "[gateway] LD_LIBRARY_PATH=${LD_LIBRARY_PATH}"
echo "[gateway] PAY_SYS_TARGET=${PAY_SYS_TARGET}"
echo "[gateway] S3RELAY_TARGET=${S3RELAY_TARGET}"
echo "[gateway] MYSQL_TARGET=${MYSQL_TARGET}"
echo "[gateway] GAME_PUBLIC_IP=${GAME_PUBLIC_IP}"
echo "[gateway] GAME_PORT=${GAME_PORT}"
echo "[gateway] PATCH_ELF_BSS=${PATCH_ELF_BSS}"

wait_for_tcp "${MYSQL_TARGET}" "${DEPENDENCY_WAIT_TIMEOUT}" MySQL
wait_for_tcp "${PAY_SYS_TARGET}" "${DEPENDENCY_WAIT_TIMEOUT}" PaySys
wait_for_tcp "${S3RELAY_TARGET}" "${DEPENDENCY_WAIT_TIMEOUT}" RootRelay

echo "[gateway] forwarding container 127.0.0.1:3306 -> ${MYSQL_TARGET}"
socat TCP-LISTEN:3306,bind=127.0.0.1,reuseaddr,fork TCP:"${MYSQL_TARGET}" &
MYSQL_SOCAT_PID=$!
echo "[gateway] mysql forward pid=${MYSQL_SOCAT_PID}"
sleep 1

echo "[gateway] forwarding container 127.0.0.1:7777 -> ${S3RELAY_TARGET}"
socat -d -d TCP-LISTEN:7777,bind=127.0.0.1,reuseaddr,fork TCP:"${S3RELAY_TARGET}" &
S3RELAY_SOCAT_PID=$!
echo "[gateway] s3relay forward pid=${S3RELAY_SOCAT_PID}"
sleep 1

if [[ "${RUN_GODDESS}" == "1" ]]; then
  echo "[gateway] starting goddess_y on configured RoleSvrPort"
  if [[ "${TRACE_GODDESS_KICK:-0}" == "1" ]]; then
    echo "[gateway] Goddess kick trace enabled -> ${CAPTURE_DIR}/logs/goddess_kick_trace.log"
    gdb -q -batch -x /usr/local/share/jx/trace-goddess.gdb --args /tmp/jx-gateway-bin/goddess_y &
  else
    /tmp/jx-gateway-bin/goddess_y &
  fi
  GODDESS_PID=$!
  echo "[gateway] goddess_y pid=${GODDESS_PID}"
  sleep 2
fi

echo "[gateway] forwarding container 127.0.0.1:5002 -> ${PAY_SYS_TARGET}"
socat -d -d TCP-LISTEN:5002,bind=127.0.0.1,reuseaddr,fork TCP:"${PAY_SYS_TARGET}" &
SOCAT_PID=$!
echo "[gateway] paysys forward pid=${SOCAT_PID}"
sleep 1

if [[ "${RUN_BISHOP}" != "1" ]]; then
  echo "[gateway] RUN_BISHOP=0, keeping container alive"
  tail -f /dev/null
fi

echo "[gateway] starting bishop_y on configured ClientOpenPort/DenialPort"
if [[ "${TRACE_BISHOP_KICK:-0}" == "1" ]]; then
  echo "[gateway] Bishop kick trace enabled -> ${CAPTURE_DIR}/logs/bishop_kick_trace.log"
  gdb -q -batch -x /usr/local/share/jx/trace-bishop.gdb --args /tmp/jx-gateway-bin/bishop_y &
else
  /tmp/jx-gateway-bin/bishop_y &
fi
BISHOP_PID=$!
echo "[gateway] bishop_y pid=${BISHOP_PID}"
sleep 3

if [[ "${RUN_S3RELAY}" == "1" ]]; then
  echo "[gateway] starting s3relay/s3relay_y"
  (
    cd /home/jxser/gateway/s3relay
    exec ./s3relay_y
  ) &
  S3RELAY_PID=$!
  echo "[gateway] s3relay_y pid=${S3RELAY_PID}"
  sleep 3
fi

if [[ "${RUN_GAME}" == "1" ]]; then
  echo "[gateway] starting server1/jx_linux_y"
  (
    cd /home/jxser/server1
    if [[ "${TRACE_GAME_KICK:-0}" == "1" ]]; then
      echo "[gateway] GameServer kick trace enabled -> ${CAPTURE_DIR}/logs/gameserver_kick_trace.log"
      exec gdb -q -batch -x /usr/local/share/jx/trace-gameserver.gdb --args ./jx_linux_y
    else
      exec ./jx_linux_y
    fi
  ) &
  GAME_PID=$!
  echo "[gateway] jx_linux_y pid=${GAME_PID}"

  # Docker publishes the host port to the address on the default Compose
  # network.  jx_linux_y binds only to GAME_PUBLIC_IP on gateway_lan, so bridge
  # the published-port destination to the address where the game actually
  # listens.  Binding to just the default-network IP avoids colliding with
  # jx_linux_y on GAME_PUBLIC_IP:GAME_PORT.
  DEFAULT_ROUTE_IF="$(ip -4 route show default | awk 'NR == 1 { print $5 }')"
  DEFAULT_NETWORK_IP="$(ip -4 -o addr show dev "${DEFAULT_ROUTE_IF}" | awk 'NR == 1 { split($4, addr, "/"); print addr[1] }')"
  if [[ -z "${DEFAULT_NETWORK_IP}" ]]; then
    echo "[gateway] ERROR: could not determine the default Docker network IP"
    exit 1
  fi
  echo "[gateway] forwarding published ${DEFAULT_NETWORK_IP}:${GAME_PORT} -> ${GAME_PUBLIC_IP}:${GAME_PORT}"
  socat -d -d TCP-LISTEN:"${GAME_PORT}",bind="${DEFAULT_NETWORK_IP}",reuseaddr,fork TCP:"${GAME_PUBLIC_IP}":"${GAME_PORT}" &
  GAME_SOCAT_PID=$!
  echo "[gateway] game published-port forward pid=${GAME_SOCAT_PID}"
fi

# Treat every enabled legacy service as critical. If one exits, terminate the
# entrypoint so Docker's restart policy reruns the complete ordered startup.
CRITICAL_PROCESSES=("Bishop:${BISHOP_PID}")
if [[ "${RUN_GODDESS}" == "1" ]]; then CRITICAL_PROCESSES+=("Goddess:${GODDESS_PID}"); fi
if [[ "${RUN_S3RELAY}" == "1" ]]; then CRITICAL_PROCESSES+=("S3Relay:${S3RELAY_PID}"); fi
if [[ "${RUN_GAME}" == "1" ]]; then CRITICAL_PROCESSES+=("GameServer:${GAME_PID}"); fi
while true; do
  for spec in "${CRITICAL_PROCESSES[@]}"; do
    name="${spec%%:*}"
    pid="${spec#*:}"
    if [[ -n "${pid}" ]] && ! kill -0 "${pid}" 2>/dev/null; then
      echo "[gateway] ERROR: critical process ${name} pid=${pid} exited; restarting container"
      exit 1
    fi
  done
  sleep 2
done
