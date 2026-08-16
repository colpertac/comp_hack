#!/usr/bin/env bash
# Create portable runtime/ under the repo (no /etc or /var required).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

FORCE_CONFIG="${FORCE_CONFIG:-0}"
COPY_CLIENT_DATA="${COPY_CLIENT_DATA:-1}"

info "Creating runtime directories under ${RUNTIME_DIR}"
ensure_runtime_dirs

install_config_file() {
  local src="$1"
  local dest="$2"
  if [[ -f "${dest}" && "${FORCE_CONFIG}" != "1" ]]; then
    info "keeping existing ${dest}"
    return
  fi
  cp "${src}" "${dest}"
  info "installed ${dest}"
}

info "Installing config templates (relative paths for runtime/)"
install_config_file "${ROOT_DIR}/contrib/nixconfig/lobby.xml" "${CONFIG_DIR}/lobby.xml"
install_config_file "${ROOT_DIR}/contrib/nixconfig/world.xml" "${CONFIG_DIR}/world.xml"
install_config_file "${ROOT_DIR}/contrib/nixconfig/channel.xml" "${CONFIG_DIR}/channel.xml"
install_config_file "${ROOT_DIR}/contrib/nixconfig/setup.xml" "${CONFIG_DIR}/setup.xml"
install_config_file "${ROOT_DIR}/contrib/testing/config/constants.xml" "${CONFIG_DIR}/constants.xml"
install_config_file "${ROOT_DIR}/contrib/winconfig/newcharacter.xml" \
  "${CONFIG_DIR}/newcharacter.xml"

# Ensure configs use portable relative paths (rewrite if someone copied old /etc files).
rewrite_runtime_paths() {
  local f="$1"
  [[ -f "${f}" ]] || return 0
  sed -i \
    -e 's|/var/lib/comp_hack/database/|database/|g' \
    -e 's|/var/lib/comp_hack/datastore|datastore|g' \
    -e 's|/var/lib/comp_hack/webroot|webroot|g' \
    -e 's|/var/log/comp_hack/lobby.log|logs/lobby.log|g' \
    -e 's|/var/log/comp_hack/world.log|logs/world.log|g' \
    -e 's|/var/log/comp_hack/channel.log|logs/channel.log|g' \
    "${f}"
}

rewrite_runtime_paths "${CONFIG_DIR}/lobby.xml"
rewrite_runtime_paths "${CONFIG_DIR}/world.xml"
rewrite_runtime_paths "${CONFIG_DIR}/channel.xml"

if ! grep -q 'ExternalIP' "${CONFIG_DIR}/channel.xml"; then
  sed -i "s|</object>|        <member name=\"ExternalIP\">${EXTERNAL_IP}</member>\n    </object>|" \
    "${CONFIG_DIR}/channel.xml"
  info "set ExternalIP=${EXTERNAL_IP} in channel.xml"
else
  sed -i "s|<member name=\"ExternalIP\">.*</member>|<member name=\"ExternalIP\">${EXTERNAL_IP}</member>|" \
    "${CONFIG_DIR}/channel.xml"
fi

info "Syncing server datastore from repo"
rsync -a --delete \
  --exclude '.git/' \
  "${ROOT_DIR}/datastore/" \
  "${DATASTORE_DIR}/"

if [[ -d "${ROOT_DIR}/migrations" ]]; then
  rsync -a "${ROOT_DIR}/migrations/" "${DATASTORE_DIR}/migrations/"
fi

if [[ -d "${ROOT_DIR}/contrib/webroot" ]]; then
  rsync -a "${ROOT_DIR}/contrib/webroot/" "${WEBROOT_DIR}/"
fi

if [[ "${COPY_CLIENT_DATA}" == "1" ]]; then
  [[ -d "${CLIENT_DIR}/BinaryData" ]] || die "client BinaryData not found at ${CLIENT_DIR}/BinaryData (set CLIENT_DIR=...)"
  [[ -d "${CLIENT_DIR}/Map/Zone/Model" ]] || die "client Map/Zone/Model not found at ${CLIENT_DIR}/Map/Zone/Model"

  info "Copying client BinaryData from ${CLIENT_DIR}"
  mkdir -p "${DATASTORE_DIR}/BinaryData"
  rsync -a "${CLIENT_DIR}/BinaryData/" "${DATASTORE_DIR}/BinaryData/"

  info "Copying zone models"
  mkdir -p "${DATASTORE_DIR}/Map/Zone/Model"
  rsync -a "${CLIENT_DIR}/Map/Zone/Model/" "${DATASTORE_DIR}/Map/Zone/Model/"
fi

info "Setup complete"
echo "  runtime: ${RUNTIME_DIR}"
echo "  config:  ${CONFIG_DIR}"
echo "  start:   ${SCRIPT_DIR}/start.sh"
echo "  (no /etc/comp_hack or /var/lib/comp_hack required)"
