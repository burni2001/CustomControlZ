#!/usr/bin/env bash
# Builds CustomControlZ for macOS and packages it as a DMG.
#
# Prerequisites (install once):
#   brew install cmake qt@6 create-dmg
#
# Usage:
#   chmod +x package_mac.sh
#   ./package_mac.sh
#
# Output: CustomControlZ-mac.dmg in the project root.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-mac"
APP_NAME="CustomControlZ"
DMG_NAME="${APP_NAME}-mac.dmg"

# ── Locate Qt (Homebrew) ──────────────────────────────────────────────────────
QT_PREFIX="$(brew --prefix qt@6 2>/dev/null || true)"
if [ -z "${QT_PREFIX}" ]; then
    echo "ERROR: Qt@6 not found via Homebrew. Run: brew install qt@6" >&2
    exit 1
fi
export PATH="${QT_PREFIX}/bin:${PATH}"

# ── Configure ─────────────────────────────────────────────────────────────────
echo "── Configuring (cmake) ──────────────────────────────────"
cmake -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="${QT_PREFIX}" \
      "${SCRIPT_DIR}"

# ── Build ─────────────────────────────────────────────────────────────────────
echo "── Building ─────────────────────────────────────────────"
cmake --build "${BUILD_DIR}" --config Release --parallel "$(sysctl -n hw.logicalcpu)"

# ── Locate the .app ───────────────────────────────────────────────────────────
APP_PATH="${BUILD_DIR}/${APP_NAME}.app"
if [ ! -d "${APP_PATH}" ]; then
    echo "ERROR: ${APP_PATH} not found after build." >&2
    exit 1
fi

# ── Create DMG ────────────────────────────────────────────────────────────────
DMG_OUT="${SCRIPT_DIR}/${DMG_NAME}"
rm -f "${DMG_OUT}"

if command -v create-dmg &>/dev/null; then
    echo "── Packaging with create-dmg ────────────────────────────"
    create-dmg \
        --volname "${APP_NAME}" \
        --volicon "${APP_PATH}/Contents/Resources/AppIcon.icns" \
        --window-pos 200 120 \
        --window-size 600 400 \
        --icon-size 100 \
        --icon "${APP_NAME}.app" 175 190 \
        --hide-extension "${APP_NAME}.app" \
        --app-drop-link 425 190 \
        "${DMG_OUT}" \
        "${APP_PATH}" || {
            # create-dmg returns non-zero even on success sometimes; fall back
            echo "create-dmg returned non-zero, falling back to hdiutil"
            hdiutil_pack
        }
else
    hdiutil_pack
fi

hdiutil_pack() {
    echo "── Packaging with hdiutil ───────────────────────────────"
    STAGING_DIR=$(mktemp -d)
    cp -R "${APP_PATH}" "${STAGING_DIR}/"
    ln -s /Applications "${STAGING_DIR}/Applications"
    hdiutil create -volname "${APP_NAME}" \
                   -srcfolder "${STAGING_DIR}" \
                   -ov -format UDZO \
                   "${DMG_OUT}"
    rm -rf "${STAGING_DIR}"
}

echo ""
echo "✓ Done: ${DMG_OUT}"
echo ""
echo "To install: open the DMG and drag ${APP_NAME}.app to Applications."
echo "First launch: grant Accessibility permission in"
echo "  System Settings → Privacy & Security → Accessibility"
