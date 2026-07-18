#!/usr/bin/env bash
# Configure and build lobby/world/channel for modern Debian/GCC.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

JOBS="${JOBS:-1}"
RECONFIGURE="${RECONFIGURE:-0}"

need_cmd cmake
need_cmd g++

if [[ "${RECONFIGURE}" == "1" ]] || [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
  info "Configuring ${BUILD_DIR}"
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DNO_WARNINGS=ON \
    -DDISABLE_TESTING=ON \
    -DUSE_SYSTEM_ZLIB=ON \
    -DUSE_SYSTEM_OPENSSL=ON \
    -DUSE_SYSTEM_PHYSFS=ON \
    -DUSE_SYSTEM_MARIADB=ON
fi

info "Building server targets (-j${JOBS})"
cmake --build "${BUILD_DIR}" --target comp_lobby comp_world comp_channel -j"${JOBS}" || true

# Dependency packaging quirks on modern distros.
GTEST_DEATH="${BUILD_DIR}/googletest/src/googletest/googletest/src/gtest-death-test.cc"
if [[ -f "${GTEST_DEATH}" ]]; then
  sed -i 's/int dummy;/int dummy = 0;/' "${GTEST_DEATH}" || true
fi

if [[ -d "${BUILD_DIR}/squirrel3/src/squirrel3/include" ]]; then
  mkdir -p "${BUILD_DIR}/squirrel3/include"
  cp -a "${BUILD_DIR}/squirrel3/src/squirrel3/include/." "${BUILD_DIR}/squirrel3/include/"
fi

if [[ -f "${BUILD_DIR}/civetweb/lib/libcivetweb-cpp.a" ]]; then
  ln -sfn libcivetweb-cpp.a "${BUILD_DIR}/civetweb/lib/libcxx-library.a"
fi

if [[ -f "${BUILD_DIR}/squirrel3/lib/libsquirrel_static.a" ]]; then
  ln -sfn libsquirrel_static.a "${BUILD_DIR}/squirrel3/lib/libsquirrel.a"
fi

if [[ -f "${BUILD_DIR}/squirrel3/lib/libsqstdlib_static.a" ]]; then
  ln -sfn libsqstdlib_static.a "${BUILD_DIR}/squirrel3/lib/libsqstdlib.a"
fi

info "Retrying server link/build if needed"
cmake --build "${BUILD_DIR}" --target comp_lobby comp_world comp_channel -j"${JOBS}"

for name in "${SERVICES[@]}"; do
  [[ -x "$(bin_path "${name}")" ]] || die "missing binary: $(bin_path "${name}")"
done

info "Build complete:"
ls -lh "$(bin_path comp_lobby)" "$(bin_path comp_world)" "$(bin_path comp_channel)"
