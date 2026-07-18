#!/usr/bin/env bash
# Create config/data directories and install content for a local COMP_hack server.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

FORCE_CONFIG="${FORCE_CONFIG:-0}"
COPY_CLIENT_DATA="${COPY_CLIENT_DATA:-1}"

info "Creating runtime directories"
if [[ "${EUID}" -eq 0 ]]; then
  ensure_dirs_owned_by_user
else
  sudo mkdir -p \
    "${CONFIG_DIR}" \
    "${DATABASE_DIR}" \
    "${DATASTORE_DIR}" \
    "${WEBROOT_DIR}" \
    "${LOG_DIR}"
  sudo chown -R "${USER}:$(id -gn)" \
    "${CONFIG_DIR}" \
    "${DATA_ROOT}" \
    "${LOG_DIR}"
  mkdir -p "${RUN_DIR}" "${LOCAL_LOG_DIR}"
fi

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

info "Installing config templates"
install_config_file "${ROOT_DIR}/contrib/nixconfig/lobby.xml" "${CONFIG_DIR}/lobby.xml"
install_config_file "${ROOT_DIR}/contrib/nixconfig/world.xml" "${CONFIG_DIR}/world.xml"
install_config_file "${ROOT_DIR}/contrib/nixconfig/channel.xml" "${CONFIG_DIR}/channel.xml"
install_config_file "${ROOT_DIR}/contrib/nixconfig/setup.xml" "${CONFIG_DIR}/setup.xml"
install_config_file "${ROOT_DIR}/contrib/testing/config/constants.xml" "${CONFIG_DIR}/constants.xml"

if ! grep -q 'ExternalIP' "${CONFIG_DIR}/channel.xml"; then
  # Insert ExternalIP before closing </object>
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
  rsync -a "${CLIENT_DIR}/BinaryData/" "${DATASTORE_DIR}/BinaryData/"

  info "Copying QMP map models"
  mkdir -p "${DATASTORE_DIR}/Map/Zone/Model"
  rsync -a "${CLIENT_DIR}/Map/Zone/Model/" "${DATASTORE_DIR}/Map/Zone/Model/"
fi

info "Setup complete"
echo "  config:    ${CONFIG_DIR}"
echo "  datastore: ${DATASTORE_DIR}"
echo "  database:  ${DATABASE_DIR}"
echo "  webroot:   ${WEBROOT_DIR}"
echo "  logs:      ${LOG_DIR}"
echo
echo "Default admin account is in ${CONFIG_DIR}/setup.xml (usually admin / admin123)."
echo "Edit that before first lobby start if you want a different password."
