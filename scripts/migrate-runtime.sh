#!/usr/bin/env bash
# One-time: copy live /etc/comp_hack + /var/lib/comp_hack (+ logs) into runtime/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

OLD_CONFIG="${OLD_CONFIG:-/etc/comp_hack}"
OLD_DATA="${OLD_DATA:-/var/lib/comp_hack}"
OLD_LOG="${OLD_LOG:-/var/log/comp_hack}"

info "Migrating into portable runtime at ${RUNTIME_DIR}"
ensure_runtime_dirs

if [[ -d "${OLD_CONFIG}" ]]; then
  info "copy config ${OLD_CONFIG} -> ${CONFIG_DIR}"
  rsync -a "${OLD_CONFIG}/" "${CONFIG_DIR}/"
else
  info "skip missing ${OLD_CONFIG}"
fi

if [[ -d "${OLD_DATA}/database" ]]; then
  info "copy database"
  rsync -a "${OLD_DATA}/database/" "${DATABASE_DIR}/"
fi
if [[ -d "${OLD_DATA}/datastore" ]]; then
  info "copy datastore (this can take a while)"
  rsync -a "${OLD_DATA}/datastore/" "${DATASTORE_DIR}/"
fi
if [[ -d "${OLD_DATA}/webroot" ]]; then
  info "copy webroot"
  rsync -a "${OLD_DATA}/webroot/" "${WEBROOT_DIR}/"
fi

if [[ -d "${OLD_LOG}" ]]; then
  info "copy logs (optional)"
  rsync -a "${OLD_LOG}/" "${LOG_DIR}/" || true
fi

info "Rewriting absolute /etc|/var paths in config XML to relative runtime paths"
for f in lobby.xml world.xml channel.xml; do
  path="${CONFIG_DIR}/${f}"
  [[ -f "${path}" ]] || continue
  sed -i \
    -e 's|/var/lib/comp_hack/database/|database/|g' \
    -e 's|/var/lib/comp_hack/datastore|datastore|g' \
    -e 's|/var/lib/comp_hack/webroot|webroot|g' \
    -e 's|/var/log/comp_hack/lobby.log|logs/lobby.log|g' \
    -e 's|/var/log/comp_hack/world.log|logs/world.log|g' \
    -e 's|/var/log/comp_hack/channel.log|logs/channel.log|g' \
    "${path}"
  info "rewrote ${path}"
done

info "Migration done. Stop any bare-metal servers using /etc paths, then:"
echo "  ${SCRIPT_DIR}/stop.sh"
echo "  ${SCRIPT_DIR}/start.sh"
echo "Old /etc and /var trees were not deleted (safe to remove after you verify)."
