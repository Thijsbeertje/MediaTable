/*
 * KDE Media Flyout popup-area monitor for KWin 6.
 *
 * The automatic desktop card is anchored 18 logical pixels from the primary
 * screen's top-left corner. This helper reports whether that maximum card
 * footprint is clear. The overlay hides its persistent desktop card while
 * occupied, but explicit hot-corner/media triggers may temporarily show a
 * compact popup above an application window. The independent hot-corner
 * surface is deliberately not part of this occlusion test.
 */
"use strict";

var overlayService = "org.thxs.KDEMediaFlyout.PopupArea";
var overlayPath = "/org/thxs/KDEMediaFlyout/PopupArea";
var overlayInterface = "org.thxs.KDEMediaFlyout.PopupArea";

var popupWidth = 365;
var popupMaximumHeight = 164;
var popupLeftMargin = 18;
var popupTopMargin = 18;
var lastSentBlocked = null;
var lastSentAt = 0;

function isOverlayWindow(window) {
    var caption = String(window.caption || "").toLowerCase();
    var resourceClass = String(window.resourceClass || "").toLowerCase();
    // Ignore both the main MediaTable layer surface and Qt popup/menu windows
    // that belong to the same application. Keep the legacy identifiers too so
    // upgrades remain compatible with any older running instance.
    return caption.indexOf("mediatable") >= 0
        || resourceClass.indexOf("mediatable") >= 0
        || caption.indexOf("kde media flyout") >= 0
        || resourceClass.indexOf("kde-media-flyout") >= 0;
}

function windowIsOnCurrentDesktop(window, desktop) {
    if (window.onAllDesktops) {
        return true;
    }

    var desktops = window.desktops;
    if (!desktops || desktops.length === 0) {
        return true;
    }

    var currentId = desktop ? String(desktop.id) : "";
    if (!currentId) {
        return false;
    }

    for (var i = 0; i < desktops.length; ++i) {
        if (desktops[i] && String(desktops[i].id) === currentId) {
            return true;
        }
    }
    return false;
}

function windowIsOnCurrentActivity(window) {
    var activities = window.activities;
    if (!activities || activities.length === 0) {
        return true;
    }

    var currentActivity = String(workspace.currentActivity || "");
    return currentActivity !== "" && activities.indexOf(currentActivity) >= 0;
}

function canObscurePopup(window, desktop) {
    if (!window || window.deleted || window.minimized || window.hidden
            || window.desktopWindow || isOverlayWindow(window)) {
        return false;
    }

    if (!windowIsOnCurrentDesktop(window, desktop)
            || !windowIsOnCurrentActivity(window)) {
        return false;
    }

    // A translucent application window still occupies the popup's screen
    // area. Only effectively invisible windows are ignored.
    var opacity = Number(window.opacity);
    if (!isNaN(opacity) && opacity <= 0.01) {
        return false;
    }

    return window.normalWindow || window.dialog || window.utility
        || window.splash || window.dock || window.toolbar || window.menu
        || window.popupWindow || window.popupMenu || window.dropdownMenu
        || window.comboBox || window.notification || window.criticalNotification
        || window.onScreenDisplay || window.appletPopup || window.tooltip;
}

function windowRect(window) {
    var left = Number(window.x);
    var top = Number(window.y);
    var width = Number(window.width);
    var height = Number(window.height);

    if (!isFinite(left) || !isFinite(top) || !isFinite(width) || !isFinite(height)
            || width <= 0 || height <= 0) {
        return null;
    }

    return {
        left: left,
        top: top,
        right: left + width,
        bottom: top + height
    };
}

function rectanglesIntersect(a, b) {
    return Math.min(a.right, b.right) > Math.max(a.left, b.left)
        && Math.min(a.bottom, b.bottom) > Math.max(a.top, b.top);
}

function calculatePopupAreaBlocked() {
    if (!workspace.screens || workspace.screens.length === 0) {
        // Fail closed: uncertain geometry must not put the popup over an app.
        return true;
    }

    // The overlay is anchored to the top-left of the primary output, with an
    // 18 px margin in card mode. Check its maximum size so it remains safe
    // when the user hovers and the compact card expands.
    var screen = workspace.screens[0];
    if (!screen || !screen.geometry) {
        return true;
    }

    var geometry = screen.geometry;
    var originX = Number(geometry.x);
    var originY = Number(geometry.y);
    if (!isFinite(originX) || !isFinite(originY)) {
        return true;
    }

    var popupRect = {
        left: originX + popupLeftMargin,
        top: originY + popupTopMargin,
        right: originX + popupLeftMargin + popupWidth,
        bottom: originY + popupTopMargin + popupMaximumHeight
    };
    var desktop = workspace.currentDesktopForScreen(screen);
    if (!desktop) {
        return true;
    }

    var windows = workspace.stackingOrder || [];
    for (var i = 0; i < windows.length; ++i) {
        var window = windows[i];
        if (!canObscurePopup(window, desktop)) {
            continue;
        }

        var rect = windowRect(window);
        if (rect && rectanglesIntersect(rect, popupRect)) {
            return true;
        }
    }

    // No foreground window intersects the card footprint. The independent
    // 16 × 16 top-left hot corner remains clickable even when this area is blocked.
    return false;
}

function publishPopupAreaBlocked(blocked) {
    var now = Date.now();
    if (blocked === lastSentBlocked && now - lastSentAt < 1000) {
        return;
    }

    lastSentBlocked = blocked;
    lastSentAt = now;

    callDBus("org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "NameHasOwner", overlayService,
        function (hasOwner) {
            if (hasOwner) {
                callDBus(overlayService, overlayPath, overlayInterface,
                    "setPopupAreaBlocked", blocked);
            }
        });
}

function updatePopupArea() {
    try {
        publishPopupAreaBlocked(calculatePopupAreaBlocked());
    } catch (error) {
        publishPopupAreaBlocked(true);
        print("Media Flyout popup-area check failed; popup remains suppressed: " + error);
    }
}

var popupAreaTimer = new QTimer();
popupAreaTimer.interval = 150;
popupAreaTimer.timeout.connect(updatePopupArea);
popupAreaTimer.start();
updatePopupArea();
