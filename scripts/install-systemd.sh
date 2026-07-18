#!/usr/bin/env bash
# Install binaries + systemd units (optional; for boot/auto-restart).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

require_root
need_cmd systemctl
need_cmd install

PREFIX="${PREFIX:-/usr/local}"
UNIT_DIR="${UNIT_DIR:-/etc/systemd/system}"

for name in "${SERVICES[@]}"; do
  [[ -x "$(bin_path "${name}")" ]] || die "missing $(bin_path "${name}") — run scripts/build.sh first"
done

[[ -f "${CONFIG_DIR}/lobby.xml" ]] || die "missing ${CONFIG_DIR}/lobby.xml — run scripts/setup.sh first"
[[ -d "${DATASTORE_DIR}/BinaryData" ]] || die "missing ${DATASTORE_DIR}/BinaryData — run scripts/setup.sh first"

info "Creating system user comphack"
if ! id comphack >/dev/null 2>&1; then
  useradd --system --group --home-dir "${DATA_ROOT}" --shell /usr/sbin/nologin comphack
fi

info "Installing binaries to ${PREFIX}/bin"
install -d "${PREFIX}/bin"
for name in "${SERVICES[@]}"; do
  install -m 0755 "$(bin_path "${name}")" "${PREFIX}/bin/${name}"
done

info "Fixing ownership for service account"
chown -R comphack:comphack "${DATA_ROOT}" "${LOG_DIR}"
# Config is read-only for the service user.
chown -R root:comphack "${CONFIG_DIR}"
chmod -R u=rwX,g=rX,o= "${CONFIG_DIR}"
chmod -R u=rwX,g=rwX,o= "${DATA_ROOT}" "${LOG_DIR}"

info "Installing systemd units"
install -m 0644 "${SCRIPT_DIR}/systemd/comp_lobby.service" "${UNIT_DIR}/comp_lobby.service"
install -m 0644 "${SCRIPT_DIR}/systemd/comp_world.service" "${UNIT_DIR}/comp_world.service"
install -m 0644 "${SCRIPT_DIR}/systemd/comp_channel.service" "${UNIT_DIR}/comp_channel.service"

# Rewrite ExecStart if PREFIX is not /usr/local
if [[ "${PREFIX}" != "/usr/local" ]]; then
  sed -i "s|/usr/local/bin/|${PREFIX}/bin/|g" \
    "${UNIT_DIR}/comp_lobby.service" \
    "${UNIT_DIR}/comp_world.service" \
    "${UNIT_DIR}/comp_channel.service"
fi

systemctl daemon-reload
systemctl enable comp_lobby.service comp_world.service comp_channel.service

info "Installed and enabled. Start with:"
echo "  sudo systemctl start comp_lobby"
echo "  sudo systemctl start comp_world"
echo "  sudo systemctl start comp_channel"
echo "or:"
echo "  sudo systemctl start comp_channel   # pulls in world+lobby via Requires="
echo
echo "Important: stop any local scripts/start.sh processes first to avoid port conflicts:"
echo "  ${SCRIPT_DIR}/stop.sh"
