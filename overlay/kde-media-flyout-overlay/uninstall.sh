#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CONTAINER="kde-media-flyout-build"
SCRIPT_ID="thxs-media-flyout-visibility"

# Stop the overlay from its Toolbox container.
toolbox run --container "$CONTAINER" -- pkill -x kde-media-flyout-overlay 2>/dev/null || true
rm -rf "$ROOT/build"

# Remove the companion KWin script so it no longer monitors the popup area.
if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /Scripting unloadScript "$SCRIPT_ID" >/dev/null 2>&1 || true
elif command -v qdbus >/dev/null 2>&1; then
  qdbus org.kde.KWin /Scripting unloadScript "$SCRIPT_ID" >/dev/null 2>&1 || true
fi

if command -v kwriteconfig6 >/dev/null 2>&1; then
  kwriteconfig6 --file kwinrc --group Plugins --key "${SCRIPT_ID}Enabled" false || true
fi

if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
elif command -v qdbus >/dev/null 2>&1; then
  qdbus org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true
fi

if command -v kpackagetool6 >/dev/null 2>&1; then
  kpackagetool6 --type=KWin/Script -r "$SCRIPT_ID" >/dev/null 2>&1 || true
fi

echo "Removed the MediaTable build and its KWin popup-area monitor."
echo "Source files and the Toolbox container were left intact."
echo "To delete the build container too, run:"
echo "  toolbox rm --force $CONTAINER"
