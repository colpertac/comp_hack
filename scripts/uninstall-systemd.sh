#!/usr/bin/env bash
# Remove systemd units and optionally installed binaries.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

require_root
need_cmd systemctl

PREFIX="${PREFIX:-/usr/local}"
UNIT_DIR="${UNIT_DIR:-/etc/systemd/system}"
REMOVE_BINARIES="${REMOVE_BINARIES:-1}"
REMOVE_USER="${REMOVE_USER:-0}"

info "Stopping/disabling services"
systemctl stop comp_channel.service comp_world.service comp_lobby.service 2>/dev/null || true
systemctl disable comp_channel.service comp_world.service comp_lobby.service 2>/dev/null || true

info "Removing unit files"
rm -f \
  "${UNIT_DIR}/comp_lobby.service" \
  "${UNIT_DIR}/comp_world.service" \
  "${UNIT_DIR}/comp_channel.service"
systemctl daemon-reload

if [[ "${REMOVE_BINARIES}" == "1" ]]; then
  info "Removing binaries from ${PREFIX}/bin"
  rm -f \
    "${PREFIX}/bin/comp_lobby" \
    "${PREFIX}/bin/comp_world" \
    "${PREFIX}/bin/comp_channel"
fi

if [[ "${REMOVE_USER}" == "1" ]] && id comphack >/dev/null 2>&1; then
  info "Removing system user comphack"
  userdel comphack 2>/dev/null || true
fi

info "Uninstalled systemd services"
echo "Config/data under ${CONFIG_DIR} and ${DATA_ROOT} were left in place."
