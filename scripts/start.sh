#!/usr/bin/env bash
# Start lobby -> world -> channel using built binaries.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

need_cmd ss

mkdir -p "${RUN_DIR}" "${LOCAL_LOG_DIR}"

start_one() {
  local name="$1"
  local port="$2"
  local bin pidfile logfile pid

  bin="$(bin_path "${name}")"
  pidfile="$(pid_file "${name}")"
  logfile="${LOCAL_LOG_DIR}/${name}.out"

  [[ -x "${bin}" ]] || die "missing binary: ${bin} (run scripts/build.sh first)"

  if [[ -f "${pidfile}" ]]; then
    pid="$(cat "${pidfile}")"
    if is_running_pid "${pid}"; then
      info "${name} already running (pid ${pid})"
      return
    fi
    rm -f "${pidfile}"
  fi

  info "starting ${name}"
  # Default config path on Linux is /etc/comp_hack/
  nohup "${bin}" >"${logfile}" 2>&1 &
  pid=$!
  echo "${pid}" >"${pidfile}"

  sleep 1
  if ! is_running_pid "${pid}"; then
    rm -f "${pidfile}"
    die "${name} exited immediately; see ${logfile} and ${LOG_DIR}/${name/comp_/}.log"
  fi

  if ! wait_for_port "${port}" 25; then
    info "warning: ${name} started but port ${port} not open yet; check ${logfile}"
  else
    info "${name} listening on ${port} (pid ${pid})"
  fi
}

start_one comp_lobby 10666
start_one comp_world 18666
start_one comp_channel 14666

info "all servers started"
echo "  logs: ${LOCAL_LOG_DIR}/*.out and ${LOG_DIR}/*.log"
echo "  stop: ${SCRIPT_DIR}/stop.sh"
