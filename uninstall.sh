#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
OVERLAY_DIR="$ROOT/overlay/kde-media-flyout-overlay"

printf '\nMediaTable uninstaller\n'
printf '%s\n' 'This removes the MediaTable build and KWin popup-area monitor.'
printf '%s\n\n' 'The Toolbox build container is kept for faster reinstalls.'
bash "$OVERLAY_DIR/uninstall.sh"
printf '\n%s\n' 'Uninstall complete.'
printf '%s\n' 'To also remove the Toolbox container: toolbox rm --force kde-media-flyout-build'
