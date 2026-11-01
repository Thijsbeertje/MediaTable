#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CONTAINER="kde-media-flyout-build"
APP="$ROOT/build/kde-media-flyout-overlay"

if [[ ! -x "$APP" ]]; then
  echo "Overlay is not built yet. Run ./install.sh first."
  exit 1
fi

if [[ "${XDG_SESSION_TYPE:-}" != "wayland" ]]; then
  echo "This overlay needs a Wayland session."
  exit 1
fi

exec toolbox run --container "$CONTAINER" -- env \
  QT_QPA_PLATFORM=wayland \
  "$APP" "$@"
