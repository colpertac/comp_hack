#!/usr/bin/env bash
# Stop and start one COMP service: lobby | world | channel
# Used by Phase 16I ops sidecar for Lane A config apply.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

need_cmd ss

SERVICE="${1:-}"
case "${SERVICE}" in
  lobby)
    NAME=comp_lobby
    PORT=10666
    ;;
  world)
    NAME=comp_world
    PORT=18666
    ;;
  channel)
    NAME=comp_channel
    PORT=14666
    ;;
  *)
    die "usage: restart-service.sh <lobby|world|channel>"
    ;;
esac

stop_one() {
  local pidfile pid

  pidfile="$(pid_file "${NAME}")"
  if [[ ! -f "${pidfile}" ]]; then
    pkill -f "$(bin_path "${NAME}")" 2>/dev/null || true
    info "${NAME}: no pid file"
    return
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
}

start_one() {
  local bin pidfile logfile pid cfg

  bin="$(bin_path "${NAME}")"
  pidfile="$(pid_file "${NAME}")"
  logfile="${LOCAL_LOG_DIR}/${NAME}.out"
  cfg="$(config_file_for "${NAME}")"

  [[ -x "${bin}" ]] || die "missing binary: ${bin} (run scripts/build.sh first)"
  [[ -f "${cfg}" ]] || die "missing config: ${cfg}"

  if [[ -f "${pidfile}" ]]; then
    pid="$(cat "${pidfile}")"
    if is_running_pid "${pid}"; then
      info "${NAME} already running (pid ${pid})"
      return
    fi
    rm -f "${pidfile}"
  fi

  ensure_runtime_dirs
  info "starting ${NAME} (cwd=${RUNTIME_DIR}, config=${cfg})"
  if [[ "${SERVICE}" == "lobby" ]]; then
    load_comp_reset_secret || info "note: COMP_RESET_SECRET not set — forgot-password disabled on lobby API"
  fi
  (
    cd "${RUNTIME_DIR}"
    nohup "${bin}" "${cfg}" >"${logfile}" 2>&1 &
    echo $! >"${pidfile}"
  )
  pid="$(cat "${pidfile}")"

  sleep 1
  if ! is_running_pid "${pid}"; then
    rm -f "${pidfile}"
    die "${NAME} exited immediately; see ${logfile} and ${LOG_DIR}/"
  fi

  if ! wait_for_port "${PORT}" 25; then
    info "warning: ${NAME} started but port ${PORT} not open yet; check ${logfile}"
  else
    info "${NAME} listening on ${PORT} (pid ${pid})"
  fi
}

stop_one
start_one

info "${SERVICE} restarted"
