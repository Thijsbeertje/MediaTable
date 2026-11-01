
# MediaTable

A floating KDE Plasma media player with an interactive vinyl record, album artwork mode, and desktop-aware visibility.

**Current baseline:** 0.5.37  
**Target platform:** KDE Plasma 6 on Wayland (tested by the author on Bazzite)

<img width="1797" height="875" alt="Polished Liquid Crystal Music Player" src="https://github.com/user-attachments/assets/2c855ee5-b8eb-4c4c-b718-e57720701dbb" />

## Features

- **Interactive vinyl playback:** The record rotates in sync with the selected player's position. Drag or scroll over the vinyl to scrub through a track, with the record movement following your input.
- **Album-art player:** Displays the current track's artwork as part of the floating player, with a dedicated record-style presentation.
- **Multiple media players:** Discovers MPRIS-compatible players and lets you switch the displayed/controlled source. Changing the selected source does not send playback commands to the players.
- **Desktop-aware visibility:** The expanded player stays on the desktop while its area is unobstructed, then gets out of the way when a foreground window overlaps it.
- **Quick access:** Summon the player from the invisible top-left hot corner or through external media-button and playback events.
- **Compact and expanded presentation:** Manually summoned popups open compact; hovering expands them. After the pointer leaves, the popup collapses and follows its auto-hide behaviour.
- **Wayland integration:** Uses a KDE/KWin visibility helper and a Wayland layer surface to behave like a lightweight desktop overlay.

## Preferences

Right-click the player to open its settings menu. Preferences are saved and restored across restarts.

- **Artwork mode:** Choose **Vinyl** (default) for the animated, interactive record; **Album** for a static square cover; or **Nothing** to hide the artwork and give track information more room.
- **Theme:** Use **Match system**, **Dark**, or **Light**.
- **Background opacity:** Choose 85%, 92%, 96% (default), or 100%.
- **Corner radius:** Choose 12, 17 (default), 22, or 26 px.
- **Hover expansion:** Enable or disable automatic expansion when the pointer enters the player.
- **Animation speed:** Choose Instant, Fast, Normal (default), or Slow for expansion and collapse.
- **Popup auto-hide:** Choose 2, 4 (default), or 8 seconds, or keep temporary popups open with **Never**.
- **Desktop visibility:** Choose whether the persistent desktop player hides when foreground windows cover its screen area (enabled by default).
- **Reset to defaults:** Restore the original Vinyl, Dark theme and default behaviour settings.

## Requirements

- KDE Plasma 6 running on Wayland.
- Fedora-based host with `toolbox` (Bazzite is the primary tested target).
- `kpackagetool6`, `kwriteconfig6`, and `qdbus6` or `qdbus` on the host.
- Internet access during the first install so Toolbox can install build dependencies.

The installer creates/uses a Fedora Toolbox named `kde-media-flyout-build` and installs CMake, GCC, Qt 6 development packages, and LayerShellQt **inside the Toolbox**, not on the immutable host.

## Install

1. Download and extract the source archive.
2. Open a terminal in the extracted `mediatable` directory.
3. Run:

   ```bash
   ./install.sh
   ```

4. Start MediaTable:

   ```bash
   ./run.sh
   ```

The first installation builds the C++ overlay in Toolbox and installs/enables the KWin popup-area monitoring script.

## Test mode

To show a test card without an active media player:

```bash
./run.sh --test
```

## Update

Run `./update.sh` from the extracted project directory. It rebuilds the application and updates the KWin helper. Keep the project directory in a stable location if your Autostart entry points to its `run.sh`.

## Autostart

In **System Settings → Session → Autostart**, add the top-level `run.sh` as a login script. The overlay becomes useful once an MPRIS player publishes media information.

## Uninstall

Run:

```bash
./uninstall.sh
```

This removes the MediaTable build and KWin helper but leaves the source directory and Toolbox container intact. To remove the build container too:

```bash
toolbox rm --force kde-media-flyout-build
```

The optional `remove-old-widget.sh` removes the legacy KDE Media Flyout Plasma widget package. Remove old widget instances from the desktop/panels first; the standalone MediaTable does not require that widget.

## Project layout

```text
mediatable/
├── install.sh
├── update.sh
├── run.sh
├── uninstall.sh
├── remove-old-widget.sh
└── overlay/kde-media-flyout-overlay/
    ├── CMakeLists.txt
    ├── src/main.cpp
    ├── thxs-media-flyout-visibility/
    ├── install.sh
    ├── run.sh
    └── uninstall.sh
```

## License

The standalone overlay component includes an MIT License at `overlay/kde-media-flyout-overlay/LICENSE`. That file applies to the overlay component. The repository as a whole does not currently declare a single project-wide licence; check individual components and third-party notices before reusing or redistributing them.
