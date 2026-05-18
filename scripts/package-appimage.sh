#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="0.1.0-alpha"
ARCH="$(uname -m)"
BUILD_DIR="${PROJECT_ROOT}/build-appimage"
APPDIR="${PROJECT_ROOT}/AppDir"
OUTPUT_NAME="PulseForge-v${VERSION}-${ARCH}.AppImage"

cd "${PROJECT_ROOT}"

command -v cmake >/dev/null 2>&1 || {
  echo "cmake is required to build PulseForge." >&2
  exit 1
}

command -v linuxdeploy >/dev/null 2>&1 || {
  echo "linuxdeploy is required. Download it from https://github.com/linuxdeploy/linuxdeploy/releases" >&2
  exit 1
}

command -v linuxdeploy-plugin-qt >/dev/null 2>&1 || {
  echo "linuxdeploy-plugin-qt is required for bundling Qt." >&2
  exit 1
}

rm -rf "${BUILD_DIR}" "${APPDIR}" "${OUTPUT_NAME}" "${OUTPUT_NAME}.sha256"

cmake -S . -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr

cmake --build "${BUILD_DIR}" --parallel
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}"

export OUTPUT="${OUTPUT_NAME}"
linuxdeploy \
  --appdir "${APPDIR}" \
  --desktop-file "${APPDIR}/usr/share/applications/io.github.MoeAra.PulseForge.desktop" \
  --icon-file "${PROJECT_ROOT}/data/icons/pulseforge.png" \
  --plugin qt \
  --output appimage

sha256sum "${OUTPUT_NAME}" > "${OUTPUT_NAME}.sha256"
echo "Built ${OUTPUT_NAME}"
echo "Wrote ${OUTPUT_NAME}.sha256"
