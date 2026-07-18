#!/usr/bin/env bash
# Stop locally started COMP_hack servers.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

stop_one() {
  local name="$1"
  local pidfile pid

  pidfile="$(pid_file "${name}")"
  if [[ ! -f "${pidfile}" ]]; then
    # Fall back to matching the build binary path.
    pkill -f "$(bin_path "${name}")" 2>/dev/null || true
    info "${name}: no pid file"
    return
  fi

  pid="$(cat "${pidfile}")"
  if is_running_pid "${pid}"; then
    info "stopping ${name} (pid ${pid})"
    kill "${pid}" 2>/dev/null || true
    for _ in $(seq 1 20); do
      is_running_pid "${pid}" || break
      sleep 0.25
    done
    if is_running_pid "${pid}"; then
      info "force killing ${name}"
      kill -9 "${pid}" 2>/dev/null || true
    fi
  else
    info "${name}: not running"
  fi
  rm -f "${pidfile}"
}

# Reverse dependency order.
stop_one comp_channel
stop_one comp_world
stop_one comp_lobby

info "stopped"
