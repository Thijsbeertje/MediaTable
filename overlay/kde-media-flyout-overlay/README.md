# MediaTable 0.5.37

Right-click the visible card to configure artwork, theme, background opacity, corner radius, hover expansion, expand/collapse speed, temporary-popup timeout, and desktop window-occlusion behaviour. Preferences persist in QSettings (`THXS/MediaTable`). Vinyl, dark appearance, 96% opacity, 17 px corners, hover expansion, 220 ms animation, four-second popup timeout, and hide-when-covered are the defaults.

# MediaTable 0.5.36

A standalone Wayland media card for KDE Plasma 6. It replaces the separate Plasma desktop widget: the fully expanded card stays visible whenever its desktop area is clear, remains expanded even when the pointer leaves it, and hides whenever a foreground window overlaps the space it would occupy.

## Version 0.5.35


Moved the media-player source-switch chevron to a fixed position at the far-right edge of the expanded card's player-identity row. The source label now truncates within a reserved text area, so long player names no longer push the chevron toward the middle of the card. The button's click target moves with the visible chevron and retains a consistent 15-pixel right inset.


Fixed an interaction-mode regression from the overlay's own controls. Clicking Next/Previous no longer calls the manual-popup summon path, and the matching MPRIS metadata update is recognized as an echo of the in-card command. Clicking Play/Pause similarly suppresses its matching playback-status echo. Cycling between players using the in-card selector also preserves the current presentation mode. As a result, interacting with controls on the persistent desktop card leaves it fully expanded; interacting with controls on a manually summoned popup leaves its existing compact/hover/auto-hide lifecycle intact. External media-button and playback-status events continue to summon the compact popup.

## Version 0.5.33

Fixed a layout regression where a passive MPRIS track/metadata change could collapse the persistent expanded desktop card. Track information and artwork now update without changing the desktop card's expanded state. Explicit overlay media controls, playback-state changes, player changes, and the top-left hot corner still summon the compact popup. Track changes continue to summon the compact popup when the persistent desktop card is not active.

## Version 0.5.32

Added a 4-second auto-hide timer for manually summoned popups. The timer runs while the pointer is outside the card and pauses on hover. Leaving the card collapses it to compact mode and restarts the timeout. When the timeout expires, an unobstructed desktop returns to its persistent expanded-card state; when a foreground window occupies the popup area, the overlay returns to its hidden hot-corner state.

## Vinyl behaviour

- The album art is the circular centre label of a vinyl record with visible grooves and a rotation marker.
- During normal playback, the record rotates at 6 degrees per second (one revolution per minute).
- When dragging the LP, its visual rotation follows the mouse's angular movement **exactly 1:1**. A full 360-degree hand rotation turns the LP exactly once.
- Scrubbing uses the same physical scale as playback: **6 degrees of rotation moves the timeline by 1 second**. A full 360-degree hand rotation therefore moves 60 seconds, regardless of track duration.
- Drag and wheel scrubbing update the local preview playhead immediately. The vinyl angle is derived from that playhead, so the vinyl remains synced to the selected player position.
- Scrolling over the LP seeks in 5-second increments, with direction relative to the cursor side: on the right half, scroll down rotates clockwise and moves forward; on the left half, scroll down rotates counterclockwise and moves backward. Scroll up reverses either direction.
- Seeking works only when the player reports `CanSeek=true`.

## Other behaviour

- Uses LayerShellQt to create a Wayland layer-surface in KDE's Top layer, above normal application windows.
- Does not request keyboard focus.
- Anchors near the upper-left corner of the primary screen.
- Watches MPRIS players over the session D-Bus.
- Stays fully expanded on the desktop whenever its screen area is unobstructed. The card does not collapse when the pointer leaves it; it hides when a foreground window overlaps its footprint.
- A separate transparent 16 × 16 logical-pixel hot corner stays clickable at the exact top-left screen corner, even when an application covers the card area.
- Clicking the hot corner or using an external media-button/playback-state event summons the card in compact mode. Controls used inside the card preserve its current presentation mode. Passive track metadata updates do not collapse an already-visible persistent desktop card. Hovering expands it and pauses the auto-hide timer. Leaving collapses it to compact mode and starts a 4-second auto-hide countdown. If the desktop area is clear when the countdown expires, the persistent expanded desktop card returns; if an application covers the area, the popup hides.
- The first MPRIS player snapshot is treated as discovery rather than a media-button event, so a clear desktop still starts with the fully expanded persistent card.
- Compact LP size: 60 × 60 logical pixels. Expanded LP size: 116 × 116 logical pixels.
- Expanded mode places the LP on the left and player/track information to its right.
- Compact card: 365 × 76. Expanded card: 365 × 164.

## Build or update

Extract the archive, open a terminal in its folder, and run:

```bash
chmod +x install.sh run.sh uninstall.sh
./install.sh
```

The installer builds inside the existing Fedora Toolbox container `kde-media-flyout-build`; it does not install development packages on the immutable Bazzite host.

## Test

```bash
./run.sh --test
```

The test player is marked seekable, so you can try dragging and scrolling over the record. In test mode the card opens compact; hovering expands it, and moving away returns it to compact. Clicking the independent hot corner also summons it compact.

## Run live

Start Spotify, Firefox, or another MPRIS-compatible player, then run:

```bash
./run.sh
```

Stop the helper with Ctrl+C.

## Reversibility

- No autostart entry is installed.
- No Plasma Shell process is restarted.
- No host packages are layered with rpm-ostree.
- The old desktop plasmoid is no longer needed by the overlay.
- `./uninstall.sh` removes the build from this project folder.
- To also remove the Toolbox container: `toolbox rm --force kde-media-flyout-build`.


## Version 0.4.5

Fixed the missing drag-rotation state member declarations that prevented 0.4.4 from compiling. Also removed the deprecated `LayerShellQt::Shell::useLayerShell()` call; Qt 6.5+ uses the LayerShellQt Wayland integration without this call.


## Version 0.4.6

Changed drag scrubbing to match the vinyl's playback rotation scale: 6 degrees of mouse rotation moves the timeline by one second. A full revolution now scrubs 60 seconds instead of the entire track. The visible record still follows the mouse 1:1, and the 6 degrees/second idle playback rotation is unchanged.


## Version 0.4.7

Added a Firefox MPRIS resilience workaround. When a seek causes Firefox to temporarily report a zero playback position or omit `mpris:length`, the overlay keeps its last requested playback position and last known duration for the current track. It continues extrapolating position during playback until MPRIS reports a plausible position again. A track change resets the fallback; a paused-state position is trusted again.


## Version 0.4.8

Changed LP wheel scrubbing so the wheel behaves like pushing the record at the cursor position. Scrolling down on the right half rotates clockwise and seeks forward; scrolling down on the left half rotates counterclockwise and seeks backward. Scrolling up reverses the direction on either side. The existing 5-second increment per wheel detent is unchanged.


## Version 0.4.9

Added a stationary circular progress ring around the vinyl. The ring fills clockwise from 12 o'clock as the track advances and updates immediately during scrubbing. Added elapsed and total time readouts (for example, `1:23` and `4:56`) above the existing progress bar. The existing seek bar remains in place.


## Version 0.5.0

Added dynamic artwork lighting. The overlay samples a representative accent hue from the current album artwork and uses it for a soft ambient glow around the vinyl, a very subtle tinted card edge, and the completed portion of the vinyl progress ring. Accent colours crossfade smoothly when artwork changes. The effect stays restrained against the dark KDE-style card; monochrome or unavailable artwork falls back to a neutral accent.


## Version 0.5.1

Added a minimal media-player switch button using the selected application's themed icon. The button appears only when at least two active MPRIS sessions are available and cycles through them in a stable order. Paused sessions are included; stopped sessions are excluded. Selecting another player changes only the overlay's selected MPRIS service: it does not send Play, Pause, Stop, or any playback command to either player. The selection remains pinned while other players start or stop, and falls back to another available session only if the selected session disappears.


## Version 0.5.2

Moved the media-player selector from a separate app-icon button in the card's upper-right corner to a small right-facing chevron immediately beside the player identity label. The chevron is only shown when more than one MPRIS player is available, and is clickable to cycle sources. Switching remains a local selection change only: it does not send Play, Pause, Stop, or any other playback command to either player.


## Version 0.5.3

Updated the expanded-card timeline fill to use the current artwork accent hue, with slightly reduced saturation and brightness so the vinyl progress ring remains the stronger accent. Improved normal-playback vinyl smoothness by repainting at a precise approximately 60 FPS instead of the previous 4 FPS refresh interval. Rotation remains time-based, so playback speed is still exactly 6 degrees per second and does not depend on the repaint rate.


## Version 0.5.4

Reduced vinyl animation repainting from approximately 60 FPS to approximately 24 FPS. At the vinyl's one-revolution-per-minute playback speed, the smaller frame-to-frame angle remains visually smooth while reducing repaint work by about 60% compared with 60 FPS. The animation timer now starts when the overlay is shown and stops when it is hidden, avoiding periodic animation wake-ups while the overlay is not visible. Rotation remains elapsed-time-based at exactly 6 degrees per second.


## Version 0.5.5

Restored Firefox/YouTube scrubbing resilience in the multi-player implementation. The selected MPRIS session is now kept pinned through short service-enumeration gaps instead of immediately switching controls to another available player. After a Firefox seek, the overlay also retains the last known seek capability, track identifier, and duration when Firefox temporarily reports a zero position or incomplete metadata. The local position estimate remains authoritative until Firefox reports a usable state again, the track changes, or playback is paused. A selected player that genuinely disappears is replaced by another available player after a 3-second grace period.


## Version 0.5.7

Improved Firefox/YouTube seeking resilience across browser restarts. The selected player is now rebound by its stable MPRIS application identity when Firefox creates a new D-Bus service instance, rather than remaining attached to the previous instance name. After a Firefox seek during playback, the overlay keeps its local elapsed-time estimate authoritative for the remainder of that playing session, even if Firefox briefly reports a plausible position before returning zero. The fallback also restores intermittently missing duration, seek capability, track ID, and metadata for the same track. It resets on pause, a confirmed track change, or switching to another media player.


## 0.5.7 responsiveness improvements

- Subscribes to MPRIS `PropertiesChanged` signals so playback and track changes refresh the flyout immediately instead of waiting for the periodic poll.
- Watches D-Bus `NameOwnerChanged` for immediate media-player discovery and restart handling.
- Sends Play/Pause/Next/Previous and Seek calls asynchronously so a slow media player cannot freeze the overlay while replying.
- Shows the flyout before waiting for a playback-control command reply.


## Version 0.5.8

Ambient artwork lighting now follows the selected player's playback state. The soft artwork-coloured glow around the vinyl and subtle tinted card edge fade in while playback is active and fade out when paused or stopped. The artwork accent itself is retained, so the progress ring and progress-bar colour remain visible while paused. The fade uses a 700 ms eased animation and does not alter player playback or seeking behaviour.


## Version 0.5.9

- Adds a lightweight asynchronous check of the selected player's MPRIS `PlaybackStatus` every 120 ms. This is a fallback for browser media sessions that do not consistently emit `PropertiesChanged` notifications.
- External Play/Pause state changes now reveal the flyout promptly, including Firefox/YouTube sessions. The existing 850 ms full metadata poll remains unchanged.
- The fast status check queries only the selected player's status property; it does not repeatedly fetch artwork or the full metadata set.


## Version 0.5.10

- Keep paused browser media sessions available in the player-cycle button, including Firefox/YouTube sessions that report `Stopped` rather than `Paused`.
- Retain stopped sessions only when they still expose track metadata, a track ID, duration, or media-control capabilities, avoiding empty browser MPRIS shells.
- Switching remains a local selection change and does not send playback commands.

## Version 0.5.14

Removed the experimental bass-reactive audio monitor introduced in 0.5.11. The monitor added a separate PulseAudio/PipeWire capture path that could produce delayed and uneven visual response and unnecessary processing. This version restores the known-good 0.5.10 implementation while retaining the artwork-coloured ambient lighting that fades with playback state, the artwork-matched progress accents, the vinyl controls, Firefox/YouTube seek resilience, the compact player switch chevron, and responsive MPRIS handling. No audio is captured or analysed.


## Version 0.5.15

Removed and kept removed the experimental bass-reactive audio monitor; the player does not capture or analyse system audio. Playback-status monitoring now checks every discovered MPRIS session asynchronously rather than only the selected one. A Play/Pause transition in any player brings the flyout back and makes that player the displayed/controlled source. If one player starts while another automatically pauses, a short arbitration window prefers the newly-playing source. This also lets Firefox/YouTube trigger the flyout when it is not currently selected. Switching the selected player remains a local selection change and never sends Play, Pause, Stop, or another playback command.


## Version 0.5.16

Added a short crossfade when the selected MPRIS player changes. The media artwork, vinyl, source label, track text, controls, time readout, and progress bar fade out before the new player's content fades in; the card surface remains stable. The new artwork is loaded before fading back in when possible, with a bounded fallback so a missing or slow cover cannot leave the card invisible.

When the track changes (including when switching to a different player), the vinyl now eases back to its 12 o'clock / 0-degree position over 420 ms. Once it reaches zero, normal 6-degrees-per-second rotation continues from that new baseline. The actual playback position and timeline progress are unchanged. Starting a drag during the reset smoothly takes over from the current visible angle.


## Version 0.5.17

- Detects same-track restarts, such as Spotify's Previous action when pressed after the restart threshold, even when MPRIS keeps the same track ID and metadata. A large same-track position jump back to the first 1.2 seconds triggers the flyout and resets the vinyl visually. Firefox is guarded while its known local post-seek position fallback is active, preventing its intermittent zero-position report from being mistaken for a real restart.
- Subscribes to the MPRIS `Seeked` signal. Near-zero seek events refresh promptly so same-track restarts are detected quickly; other seek events are briefly debounced to avoid repeated full metadata refreshes during continuous scrubbing.
- Vinyl return-to-zero now uses a constant angular speed and a duration proportional to the shortest rotation distance (140–900 ms), making large turns visibly rotate instead of appearing to jump. The playback timeline remains aligned to zero after the animation completes.


## Version 0.5.18

Added an invisible top-left hot corner as a manual way to summon the flyout. When the card's normal visibility timer expires, the same Wayland layer surface shrinks to a transparent 16 × 16 logical-pixel input target anchored at the exact top-left screen corner. Clicking that area restores the compact card. No icon or hover indicator is displayed. The hot-corner mode stops the vinyl repaint timer, and only the 16 × 16 area intercepts pointer input; the rest of the desktop remains unaffected. The normal MPRIS-triggered popup behavior is unchanged.


## 0.5.19

Fixed hot-corner summon behavior: the flyout always opens at compact height and does not trust stale Qt hover state immediately after its 16 × 16 input surface expands. A fresh pointer movement is required before hover expansion is allowed, so the card remains compact while the pointer is still at the top-left summon point.

Restored visible player handoff fades around hot-corner mode. If the selected media player changes while the card is hidden in the hot corner, its latest MPRIS snapshot is retained instead of completing the transition invisibly. When the flyout is summoned, the previous content fades out, then the newly selected player's artwork and information fade in after the artwork loads (with a bounded fallback). Player changes while the card is visible continue to crossfade normally. The source fade uses 180 ms out / 240 ms in timings.


## Version 0.5.20

Fixed a Qt 6 compile error in the hot-corner pointer-movement guard. The event position is a `QPointF`, so the window bounds are now converted to `QRectF` before the containment test. This preserves the intended behavior: after summoning from the invisible corner, the card stays compact until the pointer has moved and then entered the card. No media, animation, or popup behavior was otherwise changed.


## Version 0.5.21

Fixed Spotify album artwork loading in the standalone overlay. Spotify may publish an `open.spotify.com/image/...` artwork URL, while the Plasma widget already normalises that URL to Spotify's image CDN. The overlay now applies the same normalisation before comparing artwork state or requesting the image. Other artwork URL types, including local `file://` URLs used by browser MPRIS sessions, are left unchanged.


## Version 0.5.22

- Derives vinyl orientation directly from the selected player's estimated MPRIS position at 6 degrees per second. This is the same deterministic phase used by the Plasma widget, so both representations align when showing the same player and playhead position.
- Removes the overlay's independent post-reset/drag rotation offset, which could cause the popup and widget to drift into different vinyl orientations.
- Drag and wheel scrubbing continue to update the local preview playhead immediately; the vinyl angle follows that same preview position.
- Track-change and same-track restart animations are retained; after the animation, the vinyl returns to the shared playhead-derived phase.


## Version 0.5.23

Fixed Firefox/YouTube scrubbing in the standalone overlay to match the working Plasma widget. Firefox track continuity and its post-seek local-position fallback now use stable title/artist/album metadata rather than treating a changing MPRIS track ID as a new track. This prevents transient Firefox track-ID changes during seeks from clearing the local playhead estimate, which caused reverse scrubs to clamp at zero and the vinyl phase to reset. Album metadata is retained across transient incomplete snapshots and participates in real track-change detection. The desktop widget is unchanged.


## Version 0.5.25

Improved timeline accuracy and consistency. The playhead now keeps its monotonic local estimate through small MPRIS polling/rounding differences instead of snapping back to slightly stale samples. Larger position discontinuities are still accepted as external seeks, and same-track restarts continue to reset the vinyl. Playback-rate-aware interpolation now uses the MPRIS `Rate` property, so elapsed time and vinyl rotation remain correct when a player runs above or below normal speed. The Firefox post-seek fallback remains intact.


## Desktop widget coexistence (0.5.25, superseded in 0.5.26)

The initial coexistence implementation suppressed the popup whenever the widget
was configured on the desktop. Version 0.5.26 replaces that coarse check with
actual window-occlusion tracking through KWin.


## Version 0.5.26 — actual widget visibility

Replaced the simple “widget exists in the desktop layout” suppression rule with a KWin 6 visibility helper. The helper samples the widget's 365 × 164 logical-pixel footprint against KWin's current window stack. It uses the documented `KWin::Window` properties (`hidden`, `minimized`, `desktops`, `activities`, `opacity`, and geometry) and only counts foreground windows on the current virtual desktop and activity. It deliberately does not use `EffectWindow::visible`, which is not a property of the `KWin::Window` objects returned by `workspace.stackingOrder`.

The floating popup is suppressed only while at least 64% of the desktop widget's sampled area is exposed. When another window covers most of the widget, the popup and its invisible hot corner become available again. The installer packages and enables the KWin helper script and reconfigures KWin without restarting Plasma Shell. Uninstalling the overlay also removes the helper script. The Plasma widget itself is unchanged.


## Version 0.5.27 — strict desktop-widget occlusion

Tightened desktop widget coexistence. The KWin helper now checks only windows
stacked above the desktop surface and calculates whether their combined
rectangles cover the widget's entire content area. Any exposed portion keeps
the floating popup suppressed; the popup becomes available only when the
widget is fully covered. Visibility-detection failures now fail closed and
keep the popup suppressed instead of accidentally enabling it.


## Version 0.5.28 — QtDBus build fix

Fixed a Qt 6 compile error in the widget-visibility D-Bus bridge setup. The
session-bus connection is now non-const because `registerService()`,
`registerObject()`, and `unregisterService()` are non-const QtDBus methods.
This is a build-only correction; runtime behavior is unchanged.


## Version 0.5.29 — standalone desktop overlay

Removed the Plasma widget dependency and the old widget-presence detection. The KWin helper now checks the overlay's own maximum card footprint (365 × 164 logical pixels, offset 18 pixels from the primary screen's top-left) and the 16 × 16 invisible summon target at the screen corner. If any visible foreground window on the current virtual desktop/activity intersects either area, the overlay is hidden. As soon as the area is clear, the card returns. The wallpaper/desktop window does not need to be found, and even a partial overlap suppresses the card. Detection errors fail closed.

The fully expanded card remains visible on the desktop when its screen area is clear, including when the pointer is outside it. With no media session available, an invisible 16 × 16 hot-corner target remains available until media is discovered or the user clicks it. The standalone package no longer installs or updates the old Plasma widget.


## Version 0.5.30 — persistent expanded desktop state

Changed the unobstructed-desktop state to keep the full expanded 365 × 164 card open, even when the pointer is outside it. Hover-collapse timers and any notification/player-switch code paths that request compact height are ignored while KWin reports the desktop area as clear. When a foreground window overlaps the card footprint, the overlay is hidden and the forced-expanded state is released; when the area becomes clear again, the card returns expanded. The invisible hot-corner mode remains for the no-media state.


## Version 0.5.31 — manual compact popup and independent hot corner

The always-expanded unobstructed-desktop mode remains unchanged, but explicit popup triggers now temporarily use the notification-style compact/hover-expanded behavior. Media/player/track events open compact (or expanded immediately if the pointer is already over the card); hover expands it and leaving collapses it. These user-triggered openings are allowed above an application window, even while KWin reports the desktop footprint as occupied.

Moved the invisible top-left input target into a separate transparent 16 × 16 LayerShellQt window. It remains clickable while the main card is hidden behind an application and also remains available while the persistent desktop card is visible. The KWin monitor now tests only the card footprint, not the hot-corner rectangle, and explicitly ignores both overlay surfaces.

The initial MPRIS player snapshot is treated as player discovery rather than an explicit media event: with a clear desktop it activates the persistent expanded desktop card; with the card area blocked it leaves the card hidden and preserves the hot corner.


## MediaTable 0.5.36 — Personalisation

Right-click the visible player to open the MediaTable settings menu. The Artwork section offers three mutually exclusive modes:

- **Vinyl** — the default; animated and interactive, including playhead scrubbing.
- **Album** — a static square album cover.
- **Nothing** — hides the artwork and gives the track text more room.

The selected mode is stored with QSettings under the THXS / MediaTable application settings and survives upgrades and restarts. “Reset to defaults” restores Vinyl. Existing v0.5.35 installations therefore start in Vinyl mode unless a preference has already been saved.
