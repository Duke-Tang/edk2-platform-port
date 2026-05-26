#!/usr/bin/env bash
# Build script for Quanta Platform firmware image (OVMF-based, X64)
#
# Usage:  ./build.sh [DEBUG|RELEASE|NOOPT]
# Requires: gcc, nasm, python3, and edk2 tools in ~/edk2 (or set $EDK2_DIR)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EDK2_DIR="${EDK2_DIR:-${HOME}/edk2}"
TARGET="${1:-DEBUG}"

# Validate EDK2 tree
if [[ ! -f "${EDK2_DIR}/edksetup.sh" ]]; then
  echo "ERROR: EDK2 source not found at ${EDK2_DIR}"
  echo "       Set EDK2_DIR or ensure ~/edk2 exists."
  exit 1
fi

# WORKSPACE must be the project root (SCRIPT_DIR) so that QuantaPkg/* resolves
# directly without an extra path prefix in the build output, which would cause
# the FV linker to look for modules in the wrong subdirectory.
export WORKSPACE="${SCRIPT_DIR}"
export PACKAGES_PATH="${SCRIPT_DIR}:${EDK2_DIR}"
export EDK_TOOLS_PATH="${EDK2_DIR}/BaseTools"

# Bootstrap BaseTools once
if [[ ! -f "${EDK2_DIR}/BaseTools/Source/C/bin/GenFw" ]]; then
  echo "==> Building BaseTools..."
  make -C "${EDK2_DIR}/BaseTools" -j"$(nproc)"
fi

# edksetup.sh → BuildEnv → StoreCurrentConfiguration writes ${CONF_PATH}/BuildEnv.sh.
# CONF_PATH resolves to ${WORKSPACE}/Conf; that directory must already exist or the
# shell redirect fails (No such file or directory) before templates are copied.
mkdir -p "${WORKSPACE}/Conf"

# Clear positional params so edksetup.sh arg-parser doesn't see our TARGET arg.
# Disable nounset briefly: edksetup.sh tests unset vars via [ -n "$VAR" ] which
# is safe idiomatically but fails under set -u.
set --
set +u
# shellcheck source=/dev/null
source "${EDK2_DIR}/edksetup.sh"
set -u

echo "==> Building Quanta platform (TARGET=${TARGET})..."
build \
  -p QuantaPkg/QuantaPkgX64.dsc \
  -a X64 \
  -t GCC \
  -b "${TARGET}" \
  "$@"

echo ""
echo "Build complete. Firmware images:"
find "${SCRIPT_DIR}/Build/QuantaX64/${TARGET}_GCC/FV" \
     -name "QUANTA*.fd" 2>/dev/null || \
find "${SCRIPT_DIR}/Build/QuantaX64" -name "*.fd" 2>/dev/null | head -5
