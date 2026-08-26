#!/usr/bin/env bash
# Stop one COMP service: lobby | world | channel
# Used by Phase 16I ops sidecar (Admin Power per-service controls).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

SERVICE="${1:-}"
case "${SERVICE}" in
  lobby)
    NAME=comp_lobby
    ;;
  world)
    NAME=comp_world
    ;;
  channel)
    NAME=comp_channel
    ;;
  *)
    die "usage: stop-service.sh <lobby|world|channel>"
    ;;
esac

pidfile="$(pid_file "${NAME}")"
if [[ ! -f "${pidfile}" ]]; then
  pkill -f "$(bin_path "${NAME}")" 2>/dev/null || true
  info "${NAME}: no pid file"
  info "${SERVICE} stopped"
  exit 0
fi

pid="$(cat "${pidfile}")"
if is_running_pid "${pid}"; then
  info "stopping ${NAME} (pid ${pid})"
  kill "${pid}" 2>/dev/null || true
  for _ in $(seq 1 20); do
    is_running_pid "${pid}" || break
    sleep 0.25
  done
  if is_running_pid "${pid}"; then
    info "force killing ${NAME}"
    kill -9 "${pid}" 2>/dev/null || true
  fi
else
  info "${NAME}: not running"
fi
rm -f "${pidfile}"

info "${SERVICE} stopped"
