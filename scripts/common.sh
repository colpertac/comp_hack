#!/usr/bin/env bash
# Shared paths and helpers for COMP_hack local tooling.
#
# Portable layout (Minecraft-style): everything under $ROOT_DIR/runtime/
#   runtime/config/     lobby.xml, world.xml, channel.xml, constants.xml, …
#   runtime/database/   SQLite files
#   runtime/datastore/  server content + packages + BinaryData
#   runtime/webroot/
#   runtime/logs/
#
# Override with CONFIG_DIR / DATA_ROOT / LOG_DIR / RUNTIME_DIR if needed.

set -euo pipefail

SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-current}"
BIN_DIR="${BIN_DIR:-${BUILD_DIR}/bin}"
RUN_DIR="${RUN_DIR:-${ROOT_DIR}/.run}"
LOCAL_LOG_DIR="${LOCAL_LOG_DIR:-${RUN_DIR}/logs}"

RUNTIME_DIR="${RUNTIME_DIR:-${ROOT_DIR}/runtime}"
CONFIG_DIR="${CONFIG_DIR:-${RUNTIME_DIR}/config}"
DATA_ROOT="${DATA_ROOT:-${RUNTIME_DIR}}"
LOG_DIR="${LOG_DIR:-${RUNTIME_DIR}/logs}"
DATABASE_DIR="${DATABASE_DIR:-${DATA_ROOT}/database}"
DATASTORE_DIR="${DATASTORE_DIR:-${DATA_ROOT}/datastore}"
WEBROOT_DIR="${WEBROOT_DIR:-${DATA_ROOT}/webroot}"

CLIENT_DIR="${CLIENT_DIR:-/home/cat/software/smt/game/reimagine}"
EXTERNAL_IP="${EXTERNAL_IP:-127.0.0.1}"

SERVICES=(comp_lobby comp_world comp_channel)

die() {
  echo "error: $*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"
}

info() {
  echo "==> $*"
}

ensure_runtime_dirs() {
  mkdir -p \
    "${CONFIG_DIR}" \
    "${DATABASE_DIR}" \
    "${DATASTORE_DIR}" \
    "${WEBROOT_DIR}" \
    "${LOG_DIR}" \
    "${RUN_DIR}" \
    "${LOCAL_LOG_DIR}"
}

# Kept for older docs that mentioned sudo /etc installs; no longer required.
ensure_dirs_owned_by_user() {
  ensure_runtime_dirs
}

bin_path() {
  local name="$1"
  echo "${BIN_DIR}/${name}"
}

pid_file() {
  local name="$1"
  echo "${RUN_DIR}/${name}.pid"
}

config_file_for() {
  local name="$1"
  case "${name}" in
    comp_lobby) echo "${CONFIG_DIR}/lobby.xml" ;;
    comp_world) echo "${CONFIG_DIR}/world.xml" ;;
    comp_channel) echo "${CONFIG_DIR}/channel.xml" ;;
    *) die "unknown service: ${name}" ;;
  esac
}

is_running_pid() {
  local pid="$1"
  [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null
}

wait_for_port() {
  local port="$1"
  local seconds="${2:-20}"
  local i
  for ((i = 0; i < seconds; i++)); do
    if ss -ltn "( sport = :${port} )" 2>/dev/null | grep -q ":${port}"; then
      return 0
    fi
    sleep 1
  done
  return 1
}

# Website Admin → Email writes this; Docker entrypoint reads it too.
load_comp_reset_secret() {
  if [[ -n "${COMP_RESET_SECRET:-}" ]]; then
    export COMP_RESET_SECRET
    return 0
  fi
  local candidates=()
  if [[ -n "${COMP_RESET_SECRET_FILE:-}" ]]; then
    candidates+=("${COMP_RESET_SECRET_FILE}")
  fi
  candidates+=(
    "${ROOT_DIR}/../ai_custom_smt_server/website/data/comp-reset-secret"
    "${ROOT_DIR}/../deploy/website-data/comp-reset-secret"
  )
  local f
  for f in "${candidates[@]}"; do
    [[ -n "${f}" && -f "${f}" ]] || continue
    COMP_RESET_SECRET="$(tr -d '\n\r' <"${f}")"
    export COMP_RESET_SECRET
    info "loaded COMP_RESET_SECRET from ${f}"
    return 0
  done
  return 1
}
