#!/usr/bin/env bash
# Start lobby -> world -> channel using built binaries and runtime/ configs.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

need_cmd ss

ensure_runtime_dirs

start_one() {
  local name="$1"
  local port="$2"
  local bin pidfile logfile pid cfg

  bin="$(bin_path "${name}")"
  pidfile="$(pid_file "${name}")"
  logfile="${LOCAL_LOG_DIR}/${name}.out"
  cfg="$(config_file_for "${name}")"

  [[ -x "${bin}" ]] || die "missing binary: ${bin} (run scripts/build.sh first)"
  [[ -f "${cfg}" ]] || die "missing config: ${cfg} (run scripts/setup.sh or migrate-runtime.sh)"

  if [[ -f "${pidfile}" ]]; then
    pid="$(cat "${pidfile}")"
    if is_running_pid "${pid}"; then
      info "${name} already running (pid ${pid})"
      return
    fi
    rm -f "${pidfile}"
  fi

  info "starting ${name} (cwd=${RUNTIME_DIR}, config=${cfg})"
  if [[ "${name}" == "comp_lobby" ]]; then
    load_comp_reset_secret || info "note: COMP_RESET_SECRET not set — forgot-password disabled on lobby API"
  fi
  # cwd = runtime/ so relative paths in XML (database/, datastore, logs/) resolve.
  (
    cd "${RUNTIME_DIR}"
    nohup "${bin}" "${cfg}" >"${logfile}" 2>&1 &
    echo $! >"${pidfile}"
  )
  pid="$(cat "${pidfile}")"

  sleep 1
  if ! is_running_pid "${pid}"; then
    rm -f "${pidfile}"
    die "${name} exited immediately; see ${logfile} and ${LOG_DIR}/"
  fi

  require_service_up "${name}" "${port}" "${pid}" "${logfile}" 90
}

start_one comp_lobby 10666
start_one comp_world 18666
start_one comp_channel 14666

info "all servers started (runtime=${RUNTIME_DIR})"
echo "  logs: ${LOCAL_LOG_DIR}/*.out and ${LOG_DIR}/*.log"
echo "  stop: ${SCRIPT_DIR}/stop.sh"
