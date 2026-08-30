#!/usr/bin/env bash
# Start one COMP service: lobby | world | channel
# Used by Phase 16I ops sidecar (Admin Power per-service controls).

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
    die "usage: start-service.sh <lobby|world|channel>"
    ;;
esac

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
    exit 0
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

require_service_up "${NAME}" "${PORT}" "${pid}" "${logfile}" 90

info "${SERVICE} started"
