#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
OVERLAY_DIR="$ROOT/overlay/kde-media-flyout-overlay"

[[ -f "$OVERLAY_DIR/CMakeLists.txt" ]] || {
  echo "ERROR: Overlay source files are missing: $OVERLAY_DIR" >&2
  exit 1
}

printf '\nMediaTable installer\n'
printf '%s\n' 'This installs/updates the standalone MediaTable desktop player, its KWin popup-area monitor, and its independent top-left hot corner.'
printf '%s\n\n' 'The old Plasma widget is not required.'
bash "$OVERLAY_DIR/install.sh"
printf '\n%s\n' 'Installation complete.'
printf '%s\n' 'Start the overlay with: ./run.sh'
printf '%s\n' 'Test it with:          ./run.sh --test'
printf '%s\n' 'Update later with:     ./update.sh'
