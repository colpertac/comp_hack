#!/usr/bin/env bash
# Show status of local COMP_hack servers.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

status_one() {
  local name="$1"
  local port="$2"
  local pidfile pid state port_state

  pidfile="$(pid_file "${name}")"
  state="stopped"
  pid="-"

  if [[ -f "${pidfile}" ]]; then
    pid="$(cat "${pidfile}")"
    if is_running_pid "${pid}"; then
      state="running"
    else
      state="stale-pid"
    fi
  elif pgrep -f "$(bin_path "${name}")" >/dev/null 2>&1; then
    state="running(no-pidfile)"
    pid="$(pgrep -f "$(bin_path "${name}")" | head -n1)"
  fi

  if ss -ltn "( sport = :${port} )" 2>/dev/null | grep -q ":${port}"; then
    port_state="open"
  else
    port_state="closed"
  fi

  printf '%-14s %-18s pid=%-8s port=%-5s (%s)\n' \
    "${name}" "${state}" "${pid}" "${port}" "${port_state}"
}

echo "BIN_DIR=${BIN_DIR}"
echo "RUNTIME_DIR=${RUNTIME_DIR}"
echo "CONFIG_DIR=${CONFIG_DIR}"
echo
status_one comp_lobby 10666
status_one comp_world 18666
status_one comp_channel 14666

if systemctl list-unit-files 'comp_*.service' >/dev/null 2>&1; then
  echo
  echo "systemd units:"
  systemctl is-enabled comp_lobby.service comp_world.service comp_channel.service 2>/dev/null || true
  systemctl is-active comp_lobby.service comp_world.service comp_channel.service 2>/dev/null || true
fi
