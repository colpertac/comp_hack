#!/usr/bin/env bash
# Shared paths and helpers for COMP_hack local/systemd tooling.

set -euo pipefail

SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-current}"
BIN_DIR="${BIN_DIR:-${BUILD_DIR}/bin}"
RUN_DIR="${RUN_DIR:-${ROOT_DIR}/.run}"
LOCAL_LOG_DIR="${LOCAL_LOG_DIR:-${RUN_DIR}/logs}"

CONFIG_DIR="${CONFIG_DIR:-/etc/comp_hack}"
DATA_ROOT="${DATA_ROOT:-/var/lib/comp_hack}"
LOG_DIR="${LOG_DIR:-/var/log/comp_hack}"
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

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    die "run this script with sudo"
  fi
}

ensure_dirs_owned_by_user() {
  local user="${SUDO_USER:-${USER}}"
  local group
  group="$(id -gn "${user}")"

  mkdir -p \
    "${CONFIG_DIR}" \
    "${DATABASE_DIR}" \
    "${DATASTORE_DIR}" \
    "${WEBROOT_DIR}" \
    "${LOG_DIR}" \
    "${RUN_DIR}" \
    "${LOCAL_LOG_DIR}"

  chown -R "${user}:${group}" \
    "${CONFIG_DIR}" \
    "${DATA_ROOT}" \
    "${LOG_DIR}" \
    "${RUN_DIR}"
}

bin_path() {
  local name="$1"
  echo "${BIN_DIR}/${name}"
}

pid_file() {
  local name="$1"
  echo "${RUN_DIR}/${name}.pid"
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
