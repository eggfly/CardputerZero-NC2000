#!/bin/bash
#
# Publish NC2000 .deb to M5Stack FirmwareManagementV3.
#
# Usage:
#   ./packaging/publish_firmware.sh                       # latest CI pre-release
#   ./packaging/publish_firmware.sh <release-tag>         # specific tag
#   ./packaging/publish_firmware.sh --local build/nc2000_1.0.0_arm64.deb
#
#   # Override upload version (server requires strictly greater than latest):
#   ./packaging/publish_firmware.sh -v v1.0.4
#   ./packaging/publish_firmware.sh -v v1.0.4 nc2000-20260430-184403
#   ./packaging/publish_firmware.sh -v v1.0.4 --local build/nc2000_1.0.0_arm64.deb
#
# Env overrides:
#   FW_MANAGER   path to firmware_manager.py
#                (default: ../M5CardputerZero-UserDemo-Features/doc/firmware_manager.py)
#   SKU          default: M5Stack-Zero
#   FW_CLASS     default: firmware
#   GH_REPO      default: eggfly/CardputerZero-NC2000
#
# Requires: gh (logged in), python3 + requests, and a saved fw_manager token
# (run `firmware_manager.py login ...` once beforehand).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

FW_MANAGER="${FW_MANAGER:-$ROOT_DIR/../M5CardputerZero-UserDemo/doc/firmware_manager.py}"
SKU="${SKU:-M5Stack-Zero}"
FW_CLASS="${FW_CLASS:-firmware}"
GH_REPO="${GH_REPO:-eggfly/CardputerZero-NC2000}"
ICON="$SCRIPT_DIR/nc2000_icon.png"
VERSION_FILE="$SCRIPT_DIR/VERSION"

# Bypass local MITM proxy (m5stack-ota SSL fails through Clash/Surge-style proxies).
export HTTPS_PROXY= HTTP_PROXY= ALL_PROXY= no_proxy='*'

# ── parse -v|--version override ───────────────────────────────
VERSION_OVERRIDE=""
while [[ "${1:-}" == "-v" || "${1:-}" == "--version" ]]; do
    [[ -z "${2:-}" ]] && { echo "✗ $1 needs an argument"; exit 1; }
    VERSION_OVERRIDE="$2"
    shift 2
done

# ── locate .deb ───────────────────────────────────────────────
DEB_PATH=""
RELEASE_TAG=""
if [[ "${1:-}" == "--local" ]]; then
    [[ -z "${2:-}" ]] && { echo "✗ --local needs a file path"; exit 1; }
    DEB_PATH="$2"
else
    RELEASE_TAG="${1:-}"
    if [[ -z "$RELEASE_TAG" ]]; then
        echo "→ Resolving latest pre-release tag from $GH_REPO"
        RELEASE_TAG=$(gh release list -R "$GH_REPO" -L 1 \
            --json tagName --jq '.[0].tagName')
        [[ -z "$RELEASE_TAG" ]] && { echo "✗ no releases found"; exit 1; }
    fi
    echo "→ Using release: $RELEASE_TAG"

    TMP_DIR=$(mktemp -d)
    trap 'rm -rf "$TMP_DIR"' EXIT
    echo "→ Downloading .deb to $TMP_DIR"
    gh release download "$RELEASE_TAG" -R "$GH_REPO" \
        -p '*.deb' --dir "$TMP_DIR" --clobber
    DEB_PATH=$(ls "$TMP_DIR"/*.deb | head -1)
    [[ -z "$DEB_PATH" ]] && { echo "✗ no .deb in release"; exit 1; }
fi

# ── derive version ────────────────────────────────────────────
BASENAME=$(basename "$DEB_PATH")
# nc2000_<version>_<arch>.deb  →  <version>
DEB_VERSION=$(echo "$BASENAME" | sed -n 's/^nc2000_\(.*\)_[^_]*\.deb$/\1/p')
if [[ -z "$DEB_VERSION" && -f "$VERSION_FILE" ]]; then
    DEB_VERSION=$(cat "$VERSION_FILE")
fi
[[ -z "$DEB_VERSION" ]] && { echo "✗ cannot derive version"; exit 1; }
if [[ -n "$VERSION_OVERRIDE" ]]; then
    UPLOAD_VERSION="$VERSION_OVERRIDE"
else
    UPLOAD_VERSION="v${DEB_VERSION}"
    [[ -n "$RELEASE_TAG" ]] && UPLOAD_VERSION="v${DEB_VERSION}+${RELEASE_TAG}"
fi

# ── sanity checks ─────────────────────────────────────────────
[[ -f "$FW_MANAGER" ]] || { echo "✗ firmware_manager.py not found: $FW_MANAGER"; exit 1; }
[[ -f "$ICON" ]]       || { echo "✗ icon not found: $ICON"; exit 1; }
[[ -f "$DEB_PATH" ]]   || { echo "✗ deb not found: $DEB_PATH"; exit 1; }

DESC="NC2000 emulator port for M5CardputerZero"
[[ -n "$RELEASE_TAG" ]] && DESC="$DESC (CI build $RELEASE_TAG)"

# ── summary ───────────────────────────────────────────────────
echo "──────────── upload ────────────"
echo "  file:    $DEB_PATH"
echo "  avatar:  $ICON"
echo "  name:    NC2000"
echo "  sku:     $SKU"
echo "  version: $UPLOAD_VERSION"
echo "  class:   $FW_CLASS"
echo "  desc:    $DESC"
echo "────────────────────────────────"

python3 "$FW_MANAGER" upload \
    --file        "$DEB_PATH" \
    --avatar      "$ICON" \
    --name        "NC2000" \
    --sku         "$SKU" \
    --version     "$UPLOAD_VERSION" \
    --description "$DESC" \
    --class       "$FW_CLASS"
