#!/usr/bin/env bash
set -Eeuo pipefail
ID="io.thxs.kdemediaflyout"
if ! command -v kpackagetool6 >/dev/null 2>&1; then
  echo "kpackagetool6 was not found. Run this in a KDE Plasma 6 session." >&2
  exit 1
fi
cat <<'MESSAGE'
This removes the old Plasma widget package, not just its source files.
Before continuing, make sure you have already removed every KDE Media Flyout
applet instance from your desktop and panels using Desktop Edit Mode.
MESSAGE
read -r -p 'Remove the old widget package now? [y/N] ' answer
case "$answer" in
  y|Y|yes|YES)
    kpackagetool6 --type Plasma/Applet --remove "$ID"
    echo "Old KDE Media Flyout widget package removed."
    ;;
  *)
    echo "Cancelled; the old widget package was left installed."
    ;;
esac
