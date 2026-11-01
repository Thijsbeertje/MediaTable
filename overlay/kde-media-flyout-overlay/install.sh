#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CONTAINER="kde-media-flyout-build"
FEDORA_VERSION="$(rpm -E '%{fedora}' 2>/dev/null | tr -cd '0-9')"
SCRIPT_ID="thxs-media-flyout-visibility"
SCRIPT_DIR="$ROOT/$SCRIPT_ID"

if ! command -v toolbox >/dev/null 2>&1; then
  echo "Toolbox was not found. Bazzite normally includes Toolbox."
  exit 1
fi

if [[ -z "$FEDORA_VERSION" ]]; then
  echo "Could not determine the Fedora version with rpm -E '%{fedora}'."
  exit 1
fi

if ! toolbox run --container "$CONTAINER" -- true >/dev/null 2>&1; then
  echo "Creating a Fedora $FEDORA_VERSION build container..."
  toolbox create --container "$CONTAINER" \
    --image "registry.fedoraproject.org/fedora:${FEDORA_VERSION}"
fi

echo "Installing build dependencies inside Toolbox (not on the immutable host)..."
toolbox run --container "$CONTAINER" -- sudo dnf install -y \
  cmake gcc-c++ make \
  qt6-qtbase-devel qt6-qtwayland \
  layer-shell-qt layer-shell-qt-devel

echo "Stopping any running MediaTable before updating..."
toolbox run --container "$CONTAINER" -- pkill -x kde-media-flyout-overlay 2>/dev/null || true

echo "Building MediaTable..."
toolbox run --container "$CONTAINER" -- cmake \
  -S "$ROOT" -B "$ROOT/build" \
  -DCMAKE_BUILD_TYPE=Release

toolbox run --container "$CONTAINER" -- cmake \
  --build "$ROOT/build" --parallel 2

echo
echo "Installing the KWin popup-area monitor..."
# Unload the previous script before replacing its files, so KWin cannot keep
# executing an older copy from its in-memory script engine.
if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /Scripting unloadScript "$SCRIPT_ID" >/dev/null 2>&1 || true
elif command -v qdbus >/dev/null 2>&1; then
  qdbus org.kde.KWin /Scripting unloadScript "$SCRIPT_ID" >/dev/null 2>&1 || true
fi

if ! command -v kpackagetool6 >/dev/null 2>&1; then
  echo "ERROR: kpackagetool6 is not available on the host."
  echo "Install the KWin script manually from: $SCRIPT_DIR"
  exit 1
fi

if kpackagetool6 --type=KWin/Script --list 2>/dev/null | grep -Fq "$SCRIPT_ID"; then
  kpackagetool6 --type=KWin/Script -u "$SCRIPT_DIR"
else
  kpackagetool6 --type=KWin/Script -i "$SCRIPT_DIR"
fi

if ! command -v kwriteconfig6 >/dev/null 2>&1; then
  echo "ERROR: kwriteconfig6 is not available; the KWin helper was installed but could not be enabled."
  exit 1
fi
kwriteconfig6 --file kwinrc --group Plugins --key "${SCRIPT_ID}Enabled" true

if command -v qdbus6 >/dev/null 2>&1; then
  if ! qdbus6 org.kde.KWin /KWin reconfigure; then
    echo "KWin helper is enabled, but live reconfiguration failed. Log out and back in to activate it."
  fi
elif command -v qdbus >/dev/null 2>&1; then
  if ! qdbus org.kde.KWin /KWin reconfigure; then
    echo "KWin helper is enabled, but live reconfiguration failed. Log out and back in to activate it."
  fi
else
  echo "KWin helper enabled in kwinrc. Log out and back in to activate it (qdbus was not found)."
fi

echo
echo "Build complete."
echo "The MediaTable KWin popup-area monitor is installed and enabled."
echo "The desktop card stays expanded while unobstructed and hides when a foreground window overlaps it."
echo "The top-left hot corner and media events summon a compact popup; it auto-hides 4 seconds after the pointer leaves, unless the desktop card should return."
echo
echo "Test the overlay with:"
echo "  ./run.sh --test"
echo
echo "Run the media-aware overlay with:"
echo "  ./run.sh"
echo
echo "The overlay remains visible on the desktop when the card area is clear and hides whenever a foreground window overlaps it."
echo "The old Plasma widget is no longer required; it is not installed by this package."
echo "This version does not add itself to autostart."
