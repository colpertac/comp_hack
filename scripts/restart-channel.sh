#!/usr/bin/env bash
# Stop and start comp_channel only (lobby/world keep running).
# Used by Phase 16I ops sidecar for lane-A datastore reload.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

need_cmd ss

NAME=comp_channel
PORT=14666

# World keeps RegisteredChannel rows until disconnect is processed. A fast
# restart (or kill -9) leaves a stale row → next start fails with
# "failed to register with the world's database". Wait for world to clear,
# then delete any leftover before we start again.
clear_stale_channel_registration() {
  local db="${DATABASE_DIR}/world.sqlite3"
  local count=0

  [[ -f "${db}" ]] || return 0
  command -v python3 >/dev/null 2>&1 || {
    info "python3 missing; cannot clear RegisteredChannel (restart may fail)"
    return 0
  }

  for _ in $(seq 1 40); do
    count="$(
      python3 - "${db}" <<'PY'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
print(db.execute("SELECT COUNT(*) FROM RegisteredChannel").fetchone()[0])
db.close()
PY
    )"
    if [[ "${count}" == "0" ]]; then
      return 0
    fi
    sleep 0.25
  done

  info "clearing ${count} stale RegisteredChannel row(s) in ${db}"
  python3 - "${db}" <<'PY'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
db.execute("DELETE FROM RegisteredChannel")
db.commit()
db.close()
PY
}

stop_one() {
  local pidfile pid

  pidfile="$(pid_file "${NAME}")"
  if [[ ! -f "${pidfile}" ]]; then
    pkill -f "$(bin_path "${NAME}")" 2>/dev/null || true
    info "${NAME}: no pid file"
    clear_stale_channel_registration
    return
  fi

  pid="$(cat "${pidfile}")"
  if is_running_pid "${pid}"; then
    info "stopping ${NAME} (pid ${pid})"
    kill "${pid}" 2>/dev/null || true
    # Give world time to unregister before we escalate (was ~5s).
    for _ in $(seq 1 40); do
      is_running_pid "${pid}" || break
      sleep 0.25
    done
    if is_running_pid "${pid}"; then
      info "force killing ${NAME}"
      kill -9 "${pid}" 2>/dev/null || true
      sleep 0.5
    fi
  else
    info "${NAME}: not running"
  fi
  rm -f "${pidfile}"
  clear_stale_channel_registration
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
}

stop_one
start_one

info "channel restarted"
