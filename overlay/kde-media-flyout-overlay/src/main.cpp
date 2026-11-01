#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>

#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QColor>
#include <QCursor>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>
#include <QEasingCurve>
#include <QElapsedTimer>
#include <QEnterEvent>
#include <QEvent>
#include <QImage>
#include <QHideEvent>
#include <QHash>
#include <QShowEvent>
#include <QSet>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPaintEvent>
#include <QPoint>
#include <QFontMetrics>
#include <QFile>
#include <QPolygonF>
#include <QRadialGradient>
#include <QPointF>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVariantAnimation>
#include <QVariantMap>
#include <QWidget>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <utility>

namespace
{
constexpr int CardWidth = 365;
constexpr int HotCornerSize = 16; // Separate invisible click target at the primary screen's top-left corner.
constexpr int CompactHeight = 76;
constexpr int ExpandedHeight = 164;
constexpr int HoverCollapseDelayMilliseconds = 160;
constexpr int ExpandAnimationMilliseconds = 220;
constexpr int CompactVinylDiameter = 60;
constexpr int ExpandedVinylDiameter = 116;
constexpr int VinylX = 15;
constexpr int CompactVinylY = 8;
constexpr int ExpandedVinylY = 17;
constexpr qreal PlayerSwitchButtonWidth = 20.0;
constexpr qreal PlayerSwitchButtonRightMargin = 15.0;
constexpr qreal PlayerSwitchTextGap = 3.0;
constexpr int SeekDispatchMilliseconds = 55;
constexpr int PreviewResetMilliseconds = 1500;
// Browser MPRIS sessions can briefly disappear while refreshing media metadata.
// Keep the selected session pinned during short gaps instead of switching sources.
constexpr int SelectedPlayerMissingGraceMilliseconds = 3000;
constexpr double VinylRotationDegreesPerSecond = 6.0; // one turn per minute
constexpr qint64 WheelSeekStepUsec = 5LL * 1000000LL;
constexpr double Pi = 3.14159265358979323846;
constexpr auto MprisPath = "/org/mpris/MediaPlayer2";
constexpr auto PlayerInterface = "org.mpris.MediaPlayer2.Player";
constexpr auto RootInterface = "org.mpris.MediaPlayer2";
constexpr auto PropertiesInterface = "org.freedesktop.DBus.Properties";
QString normalizeArtworkUrl(QString value)
{
    value = value.trimmed();

    // Spotify can publish a web-app artwork URL that some Qt network/image
    // paths don't handle consistently. The CDN endpoint serves the same
    // image as a directly decodable image resource. Keep all other URL types
    // (including file:// URLs used by browser MPRIS sessions) unchanged.
    const QString spotifyWebPrefix = QStringLiteral("https://open.spotify.com/image/");
    const QString spotifyHttpPrefix = QStringLiteral("http://open.spotify.com/image/");
    const QString spotifyCdnPrefix = QStringLiteral("https://i.scdn.co/image/");

    if (value.startsWith(spotifyWebPrefix, Qt::CaseInsensitive)) {
        value.replace(0, spotifyWebPrefix.size(), spotifyCdnPrefix);
    } else if (value.startsWith(spotifyHttpPrefix, Qt::CaseInsensitive)) {
        value.replace(0, spotifyHttpPrefix.size(), spotifyCdnPrefix);
    }

    return value;
}

QVariant unwrapDbusVariant(const QVariant &value)
{
    if (value.metaType().id() == qMetaTypeId<QDBusVariant>()) {
        return value.value<QDBusVariant>().variant();
    }
    return value;
}

QVariantMap toVariantMap(const QVariant &value)
{
    const QVariant unwrapped = unwrapDbusVariant(value);

    if (unwrapped.metaType().id() == QMetaType::QVariantMap) {
        return unwrapped.toMap();
    }

    if (unwrapped.metaType().id() == qMetaTypeId<QDBusArgument>()) {
        return qdbus_cast<QVariantMap>(unwrapped.value<QDBusArgument>());
    }

    return {};
}

QString stringListToText(const QVariant &value)
{
    const QVariant unwrapped = unwrapDbusVariant(value);

    if (unwrapped.canConvert<QStringList>()) {
        return unwrapped.toStringList().join(", ");
    }

    if (unwrapped.canConvert<QVariantList>()) {
        QStringList parts;
        for (const QVariant &item : unwrapped.toList()) {
            const QString text = unwrapDbusVariant(item).toString();
            if (!text.isEmpty()) {
                parts.append(text);
            }
        }
        return parts.join(", ");
    }

    return unwrapped.toString();
}

QVariant callPropertyGet(const QDBusConnection &bus,
                         const QString &service,
                         const QString &interfaceName,
                         const QString &propertyName)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        service, MprisPath, PropertiesInterface, "Get");
    message.setArguments(QList<QVariant>{QVariant(interfaceName), QVariant(propertyName)});

    const QDBusMessage reply = bus.call(message, QDBus::Block, 500);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }

    return unwrapDbusVariant(reply.arguments().constFirst());
}

QVariantMap callPropertyGetAll(const QDBusConnection &bus,
                               const QString &service,
                               const QString &interfaceName)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        service, MprisPath, PropertiesInterface, "GetAll");
    message.setArguments(QList<QVariant>{QVariant(interfaceName)});

    const QDBusMessage reply = bus.call(message, QDBus::Block, 700);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }

    return toVariantMap(reply.arguments().constFirst());
}

struct PlayerState
{
    QString service;
    QString identity;
    QString trackId;
    QString title;
    QString artist;
    QString album;
    QString artUrl;
    QString playbackStatus;
    qint64 lengthUsec = 0;
    qint64 positionUsec = 0;
    double rate = 1.0;
    bool canGoPrevious = false;
    bool canGoNext = false;
    bool canPlay = false;
    bool canPause = false;
    bool canSeek = false;

    bool isValid() const
    {
        // Some browser MPRIS integrations (notably Firefox) report Stopped
        // for a paused/idle media session, or temporarily clear its metadata.
        // Keep that session selectable when it still exposes track information
        // or media controls. This avoids hiding paused YouTube from the source
        // cycle while filtering out empty browser MPRIS shells.
        const bool hasMediaContext =
            (title != QStringLiteral("Now playing") && !title.isEmpty())
            || !trackId.isEmpty()
            || lengthUsec > 0
            || canPlay || canPause || canSeek || canGoPrevious || canGoNext;
        const bool hasKnownStatus = playbackStatus == QStringLiteral("Playing")
            || playbackStatus == QStringLiteral("Paused")
            || playbackStatus == QStringLiteral("Stopped");
        return !service.isEmpty() && hasMediaContext && hasKnownStatus;
    }

    bool isPlaying() const
    {
        return playbackStatus == QStringLiteral("Playing");
    }

    QString changeKey() const
    {
        return service + QLatin1Char('|') + title + QLatin1Char('|')
            + artist + QLatin1Char('|') + playbackStatus;
    }
};

QColor sampleArtworkAccent(const QImage &source)
{
    if (source.isNull()) {
        return QColor(112, 151, 255);
    }

    const QImage sample = source.scaled(48, 48, Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation)
                              .convertToFormat(QImage::Format_ARGB32);
    constexpr int HueBucketCount = 36;
    struct HueBucket
    {
        double weight = 0.0;
        double red = 0.0;
        double green = 0.0;
        double blue = 0.0;
        double saturation = 0.0;
        double value = 0.0;
    };
    std::array<HueBucket, HueBucketCount> buckets{};

    for (int y = 0; y < sample.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(sample.constScanLine(y));
        for (int x = 0; x < sample.width(); ++x) {
            const QColor pixel = QColor::fromRgb(line[x]);
            int hue = -1;
            int saturation = 0;
            int value = 0;
            pixel.getHsv(&hue, &saturation, &value);
            if (hue < 0 || saturation < 38 || value < 36 || value > 248) {
                continue;
            }

            const int bucketIndex = std::clamp(hue * HueBucketCount / 360,
                                               0, HueBucketCount - 1);
            const double normalizedSaturation = double(saturation) / 255.0;
            const double normalizedValue = double(value) / 255.0;
            const double weight = std::pow(normalizedSaturation, 1.35)
                * (0.45 + 0.55 * normalizedValue);
            HueBucket &bucket = buckets[size_t(bucketIndex)];
            bucket.weight += weight;
            bucket.red += double(pixel.red()) * weight;
            bucket.green += double(pixel.green()) * weight;
            bucket.blue += double(pixel.blue()) * weight;
            bucket.saturation += double(saturation) * weight;
            bucket.value += double(value) * weight;
        }
    }

    const auto dominant = std::max_element(
        buckets.begin(), buckets.end(),
        [](const HueBucket &a, const HueBucket &b) { return a.weight < b.weight; });

    if (dominant != buckets.end() && dominant->weight > 0.5) {
        const QColor average(
            qBound(0, qRound(dominant->red / dominant->weight), 255),
            qBound(0, qRound(dominant->green / dominant->weight), 255),
            qBound(0, qRound(dominant->blue / dominant->weight), 255));
        int hue = 0;
        int saturation = 0;
        int value = 0;
        average.getHsv(&hue, &saturation, &value);
        if (hue >= 0) {
            // Lift very dark or muted artwork colours so the ambient light remains visible.
            return QColor::fromHsv(hue, std::clamp(saturation, 118, 215),
                                   std::clamp(value, 155, 225));
        }
    }

    // Monochrome covers still get a restrained neutral light rather than an arbitrary hue.
    return QColor(145, 158, 181);
}

bool hasUsableTrackMetadata(const PlayerState &state)
{
    // Browser MPRIS sessions often omit xesam:artist (the reader substitutes
    // the application identity in that case). A real title is enough to identify
    // the current video; requiring an artist would incorrectly classify valid
    // Firefox metadata as missing and keep the old fallback forever.
    return !state.title.isEmpty()
        && state.title != QStringLiteral("Now playing");
}

bool isSameTrack(const PlayerState &previous, const PlayerState &next)
{
    if (previous.service.isEmpty() || previous.service != next.service) {
        return false;
    }

    const bool isFirefox = next.service.contains(QStringLiteral("firefox"),
                                                  Qt::CaseInsensitive);
    if (isFirefox) {
        // Firefox may change mpris:trackid during a seek or metadata refresh even
        // though the same video is still active. Use the stable application +
        // title/artist/album identity for the local position fallback: compare
        // usable metadata first and
        // treat incomplete snapshots as transient rather than as a new track.
        // This prevents a seek from clearing the local position estimate, which
        // otherwise makes reverse seeks clamp at zero and resets the platter.
        const bool previousHasMetadata = hasUsableTrackMetadata(previous);
        const bool nextHasMetadata = hasUsableTrackMetadata(next);
        if (previousHasMetadata && nextHasMetadata) {
            const bool previousArtistKnown = !previous.artist.isEmpty()
                && previous.artist != previous.identity;
            const bool nextArtistKnown = !next.artist.isEmpty()
                && next.artist != next.identity;
            const bool artistMatches = !previousArtistKnown || !nextArtistKnown
                || previous.artist == next.artist;
            const bool albumMatches = previous.album.isEmpty() || next.album.isEmpty()
                || previous.album == next.album;
            return previous.title == next.title && artistMatches && albumMatches;
        }
        return true; // One snapshot is temporarily incomplete.
    }

    if (!previous.trackId.isEmpty() && !next.trackId.isEmpty()) {
        return previous.trackId == next.trackId;
    }

    return previous.title == next.title && previous.artist == next.artist;
}

PlayerState readPlayerState(const QDBusConnection &bus, const QString &service)
{
    PlayerState state;
    state.service = service;

    const QVariantMap properties = callPropertyGetAll(bus, service, QString::fromLatin1(PlayerInterface));
    if (properties.isEmpty()) {
        return state;
    }

    const QVariantMap metadata = toVariantMap(properties.value(QStringLiteral("Metadata")));
    state.identity = callPropertyGet(bus, service, QString::fromLatin1(RootInterface),
                                     QStringLiteral("Identity")).toString();

    state.trackId = unwrapDbusVariant(metadata.value(QStringLiteral("mpris:trackid"))).toString();
    state.title = unwrapDbusVariant(metadata.value(QStringLiteral("xesam:title"))).toString();
    state.artist = stringListToText(metadata.value(QStringLiteral("xesam:artist")));
    state.album = unwrapDbusVariant(metadata.value(QStringLiteral("xesam:album"))).toString();
    state.artUrl = normalizeArtworkUrl(unwrapDbusVariant(metadata.value(QStringLiteral("mpris:artUrl"))).toString());
    state.lengthUsec = unwrapDbusVariant(metadata.value(QStringLiteral("mpris:length"))).toLongLong();
    state.positionUsec = callPropertyGet(bus, service, QString::fromLatin1(PlayerInterface),
                                         QStringLiteral("Position")).toLongLong();
    const double reportedRate = unwrapDbusVariant(properties.value(QStringLiteral("Rate"))).toDouble();
    state.rate = std::isfinite(reportedRate) && reportedRate > 0.0 ? reportedRate : 1.0;
    state.playbackStatus = unwrapDbusVariant(properties.value(QStringLiteral("PlaybackStatus"))).toString();
    state.canGoPrevious = unwrapDbusVariant(properties.value(QStringLiteral("CanGoPrevious"))).toBool();
    state.canGoNext = unwrapDbusVariant(properties.value(QStringLiteral("CanGoNext"))).toBool();
    state.canPlay = unwrapDbusVariant(properties.value(QStringLiteral("CanPlay"))).toBool();
    state.canPause = unwrapDbusVariant(properties.value(QStringLiteral("CanPause"))).toBool();
    state.canSeek = unwrapDbusVariant(properties.value(QStringLiteral("CanSeek"))).toBool();

    if (state.identity.isEmpty()) {
        state.identity = service.section(QLatin1Char('.'), -1);
    }
    if (state.title.isEmpty()) {
        state.title = QStringLiteral("Now playing");
    }
    if (state.artist.isEmpty()) {
        state.artist = state.identity;
    }

    return state;
}

QList<PlayerState> findPlayers(const QDBusConnection &bus)
{
    QList<PlayerState> players;
    QDBusConnectionInterface *interface = bus.interface();
    if (!interface) {
        return players;
    }

    const QDBusReply<QStringList> namesReply = interface->registeredServiceNames();
    if (!namesReply.isValid()) {
        return players;
    }

    QStringList names = namesReply.value();
    names.erase(std::remove_if(names.begin(), names.end(), [](const QString &name) {
        return !name.startsWith(QStringLiteral("org.mpris.MediaPlayer2."));
    }), names.end());

    std::sort(names.begin(), names.end());

    for (const QString &name : names) {
        const PlayerState state = readPlayerState(bus, name);
        if (state.isValid()) {
            players.append(state);
        }
    }

    return players;
}

class MediaOverlay final : public QWidget
{
public:
    enum class ArtworkMode { Vinyl, Album, Nothing };
    enum class ThemeMode { System, Dark, Light };

    explicit MediaOverlay(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_network(this)
        , m_settings(QStringLiteral("THXS"), QStringLiteral("MediaTable"))
    {
        // Vinyl is the first-run default. A user selection is stored separately
        // from the installation so upgrades never reset their preference.
        const QString savedArtworkMode = m_settings.value(
            QStringLiteral("appearance/artworkMode"), QStringLiteral("vinyl")).toString();
        if (savedArtworkMode.compare(QStringLiteral("album"), Qt::CaseInsensitive) == 0) {
            m_artworkMode = ArtworkMode::Album;
        } else if (savedArtworkMode.compare(QStringLiteral("nothing"), Qt::CaseInsensitive) == 0) {
            m_artworkMode = ArtworkMode::Nothing;
        } else {
            m_artworkMode = ArtworkMode::Vinyl;
        }

        const QString savedTheme = m_settings.value(
            QStringLiteral("appearance/theme"), QStringLiteral("dark")).toString();
        if (savedTheme.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0) {
            m_themeMode = ThemeMode::Light;
        } else if (savedTheme.compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0) {
            m_themeMode = ThemeMode::System;
        } else {
            m_themeMode = ThemeMode::Dark;
        }
        m_backgroundOpacity = std::clamp(
            m_settings.value(QStringLiteral("appearance/backgroundOpacity"), 96).toInt(), 70, 100);
        m_cornerRadius = std::clamp(
            m_settings.value(QStringLiteral("appearance/cornerRadius"), 17).toInt(), 8, 28);
        m_hoverExpansionEnabled = m_settings.value(
            QStringLiteral("behaviour/hoverExpansion"), true).toBool();
        m_animationDurationMs = std::clamp(
            m_settings.value(QStringLiteral("behaviour/animationDurationMs"), ExpandAnimationMilliseconds).toInt(),
            0, 1000);
        m_popupAutoHideSeconds = std::clamp(
            m_settings.value(QStringLiteral("behaviour/popupAutoHideSeconds"), 4).toInt(), 0, 60);
        m_hideWhenCovered = m_settings.value(
            QStringLiteral("desktop/hideWhenCovered"), true).toBool();

        setMinimumSize(HotCornerSize, HotCornerSize);
        setMaximumSize(CardWidth, ExpandedHeight);
        resize(HotCornerSize, HotCornerSize);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setFocusPolicy(Qt::NoFocus);
        setMouseTracking(true);
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);

        m_hoverCollapseTimer.setSingleShot(true);
        m_hoverCollapseTimer.setInterval(HoverCollapseDelayMilliseconds);
        QObject::connect(&m_hoverCollapseTimer, &QTimer::timeout, this, [this] {
            if (!m_desktopForcedOpen && m_hoverExpansionEnabled) {
                animateToHeight(CompactHeight);
            }
        });

        // A manually summoned popup is temporary. Keep it visible while the
        // pointer is over the card; once the pointer leaves, collapse it and
        // dismiss it after a short delay. If the desktop is unobstructed when
        // the timer expires, restore the persistent expanded desktop card.
        m_popupAutoHideTimer.setSingleShot(true);
        m_popupAutoHideTimer.setInterval(m_popupAutoHideSeconds * 1000);
        QObject::connect(&m_popupAutoHideTimer, &QTimer::timeout, this, [this] {
            if (!m_userPopupActive || m_desktopForcedOpen) {
                return;
            }

            m_userPopupActive = false;
            if (!m_popupAreaBlocked && m_state.isValid()) {
                activateOnDesktop();
            } else {
                enterHotCornerMode();
                hide();
                update();
            }
        });

        m_seekDispatchTimer.setSingleShot(true);
        QObject::connect(&m_seekDispatchTimer, &QTimer::timeout, this, [this] {
            flushPendingSeek();
        });

        m_previewResetTimer.setSingleShot(true);
        QObject::connect(&m_previewResetTimer, &QTimer::timeout, this, [this] {
            m_previewPositionValid = false;
            update();
        });

        m_accentAnimation.setDuration(650);
        m_accentAnimation.setEasingCurve(QEasingCurve::InOutCubic);
        QObject::connect(&m_accentAnimation, &QVariantAnimation::valueChanged,
                         this, [this](const QVariant &value) {
            m_artworkAccent = value.value<QColor>();
            update();
        });

        // Ambient artwork lighting is only visible while the selected player is
        // actively playing. Fade it independently from the accent colour so the
        // progress ring and timeline keep their artwork-matched colour when paused.
        m_ambientAnimation.setDuration(700);
        m_ambientAnimation.setEasingCurve(QEasingCurve::InOutCubic);
        QObject::connect(&m_ambientAnimation, &QVariantAnimation::valueChanged,
                         this, [this](const QVariant &value) {
            m_ambientOpacity = std::clamp(value.toDouble(), 0.0, 1.0);
            update();
        });

        // Crossfade the visible media content when the selected player changes.
        // Keep the card surface itself stable so the transition feels like one
        // player handing off to another rather than the entire popup blinking.
        m_contentFadeAnimation.setDuration(145);
        m_contentFadeAnimation.setEasingCurve(QEasingCurve::InOutCubic);
        QObject::connect(&m_contentFadeAnimation, &QVariantAnimation::valueChanged,
                         this, [this](const QVariant &value) {
            m_contentOpacity = std::clamp(value.toDouble(), 0.0, 1.0);
            update();
        });
        QObject::connect(&m_contentFadeAnimation, &QVariantAnimation::finished,
                         this, [this] {
            if (!m_pendingPlayerStateValid) {
                return;
            }

            const PlayerState nextPlayer = m_pendingPlayerState;
            m_pendingPlayerState = {};
            m_pendingPlayerStateValid = false;
            m_contentOpacity = 0.0;
            m_fadeInAfterPlayerTransition = true;
            applyPlayerState(nextPlayer);
        });

        m_transitionArtworkFallbackTimer.setSingleShot(true);
        m_transitionArtworkFallbackTimer.setInterval(700);
        QObject::connect(&m_transitionArtworkFallbackTimer, &QTimer::timeout,
                         this, [this] {
            finishPlayerTransitionFadeIn();
        });

        // Track changes and same-track restarts return the platter to 12 o'clock.
        // The duration scales with the actual rotation distance so a large sweep
        // visibly turns instead of looking like a teleport. Linear easing keeps
        // the angular speed constant, like a physical platter being repositioned.
        m_vinylResetAnimation.setEasingCurve(QEasingCurve::Linear);
        QObject::connect(&m_vinylResetAnimation, &QVariantAnimation::valueChanged,
                         this, [this](const QVariant &value) {
            m_vinylResetAngle = value.toDouble();
            update();
        });
        QObject::connect(&m_vinylResetAnimation, &QVariantAnimation::finished,
                         this, [this] {
            m_vinylResetActive = false;
            // Return to the shared playhead-derived phase rather than retaining
            // an independent post-reset offset.
            m_vinylResetAngle = 0.0;
            update();
        });

        m_heightAnimation.setDuration(m_animationDurationMs);
        m_heightAnimation.setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(&m_heightAnimation, &QVariantAnimation::valueChanged,
                         this, [this](const QVariant &value) {
            setCardHeight(value.toInt());
        });

        // At one revolution per minute, 24 FPS is visually smooth while avoiding
        // the unnecessary repaint cost of 60 FPS. Run the timer only while visible.
        m_positionTimer.setTimerType(Qt::PreciseTimer);
        m_positionTimer.setInterval(42); // approximately 24 FPS
        QObject::connect(&m_positionTimer, &QTimer::timeout, this, [this] {
            update();
        });
    }

    void setLayerWindow(LayerShellQt::Window *layerWindow)
    {
        m_layerWindow = layerWindow;
        syncLayerSurfaceGeometry();
    }

    void activateOnDesktop()
    {
        m_userPopupActive = false;
        m_popupAutoHideTimer.stop();

        if (m_popupAreaBlocked) {
            hide();
            return;
        }

        if (m_state.isValid()) {
            // When the desktop area is clear, the overlay becomes the persistent
            // desktop media widget. Keep it fully expanded even when the pointer
            // is outside the card. Explicit hot-corner/media events can temporarily
            // switch it back to the ordinary compact/hover-expanded popup mode.
            m_desktopForcedOpen = true;
            m_hoverCollapseTimer.stop();
            m_hotCornerOnly = false;
            m_waitForPointerMovementBeforeExpand = false;
            resize(CardWidth, std::max(height(), CompactHeight));
            syncLayerSurfaceGeometry();
            show();
            raise();
            if (!m_positionTimer.isActive()) {
                m_positionTimer.start();
            }
            animateToHeight(ExpandedHeight);
            revealHiddenPlayerTransition();
            update();
            return;
        }

        // The independent transparent PopupHotCorner window remains available
        // while the main card itself is hidden.
        m_desktopForcedOpen = false;
        enterHotCornerMode();
        hide();
        update();
    }

    // A passive MPRIS metadata/track update must not collapse the persistent
    // expanded desktop card. Track changes can still summon the compact popup
    // while the card is suppressed or while a manually summoned popup is active.
    bool shouldTreatTrackChangeAsPopupEvent() const
    {
        return !m_desktopForcedOpen;
    }

    void setPopupAreaBlocked(bool blocked)
    {
        if (!m_hideWhenCovered) {
            blocked = false;
        }
        if (m_popupAreaBlocked == blocked) {
            return;
        }

        m_popupAreaBlocked = blocked;
        if (blocked) {
            m_desktopForcedOpen = false;
            // Keep the card out of the way when a foreground window crosses its
            // footprint. The separate top-left hot corner remains available.
            m_hoverCollapseTimer.stop();
            m_seekDispatchTimer.stop();
            m_previewResetTimer.stop();
            m_transitionArtworkFallbackTimer.stop();
            m_heightAnimation.stop();
            m_contentFadeAnimation.stop();
            m_vinylResetAnimation.stop();
            m_positionTimer.stop();
            m_draggingVinyl = false;
            m_waitForPointerMovementBeforeExpand = false;
            enterHotCornerMode();
            hide();
            qInfo() << "Popup area is occupied by a foreground window; floating card suppressed, hot corner retained.";
        } else {
            qInfo() << "Popup area is clear; floating popup enabled on the desktop.";
            activateOnDesktop();
        }
    }

    void setControlCallback(std::function<void(const QString &)> callback)
    {
        m_controlCallback = std::move(callback);
    }

    void setSeekCallback(std::function<void(qint64)> callback)
    {
        m_seekCallback = std::move(callback);
    }

    void setPlayerSwitchCallback(std::function<void()> callback)
    {
        m_playerSwitchCallback = std::move(callback);
    }

    void setAvailablePlayerCount(int count)
    {
        const bool hasMultiplePlayers = count > 1;
        if (m_hasMultiplePlayers == hasMultiplePlayers) {
            return;
        }

        m_hasMultiplePlayers = hasMultiplePlayers;
        setToolTip(m_hasMultiplePlayers ? QStringLiteral("Switch media player") : QString());
        update();
    }

    void setArtworkMode(ArtworkMode mode)
    {
        if (m_artworkMode == mode) {
            return;
        }

        m_artworkMode = mode;
        if (mode != ArtworkMode::Vinyl) {
            m_draggingVinyl = false;
            setCursor(Qt::ArrowCursor);
        }

        QString storedValue = QStringLiteral("vinyl");
        if (mode == ArtworkMode::Album) {
            storedValue = QStringLiteral("album");
        } else if (mode == ArtworkMode::Nothing) {
            storedValue = QStringLiteral("nothing");
        }
        m_settings.setValue(QStringLiteral("appearance/artworkMode"), storedValue);
        m_settings.sync();
        update();
    }

    bool usesLightAppearance() const
    {
        if (m_themeMode == ThemeMode::Light) {
            return true;
        }
        if (m_themeMode == ThemeMode::Dark) {
            return false;
        }
        return QApplication::palette().color(QPalette::Window).lightness() >= 128;
    }

    QColor cardBackgroundColor() const
    {
        const int alpha = qRound(255.0 * double(m_backgroundOpacity) / 100.0);
        return usesLightAppearance() ? QColor(247, 249, 253, alpha)
                                     : QColor(25, 28, 41, alpha);
    }

    QColor cardOutlineColor() const
    {
        return usesLightAppearance() ? QColor(154, 164, 181, 125)
                                     : QColor(91, 103, 128, 115);
    }

    QColor primaryTextColor() const
    {
        return usesLightAppearance() ? QColor(28, 32, 41) : QColor(245, 247, 252);
    }

    QColor secondaryTextColor() const
    {
        return usesLightAppearance() ? QColor(76, 84, 99) : QColor(174, 186, 209);
    }

    QColor mutedTextColor() const
    {
        return usesLightAppearance() ? QColor(103, 112, 128) : QColor(157, 169, 193);
    }

    QColor identityTextColor() const
    {
        return usesLightAppearance() ? QColor(92, 102, 119) : QColor(171, 185, 211);
    }

    QColor progressTrackColor() const
    {
        return usesLightAppearance() ? QColor(204, 210, 220) : QColor(61, 68, 88);
    }

    QColor enabledControlColor() const
    {
        return usesLightAppearance() ? QColor(44, 51, 65) : QColor(244, 247, 252);
    }

    QColor disabledControlColor() const
    {
        return usesLightAppearance() ? QColor(164, 171, 184) : QColor(103, 111, 130);
    }

    void setThemeMode(ThemeMode mode)
    {
        m_themeMode = mode;
        const QString value = mode == ThemeMode::Light ? QStringLiteral("light")
            : mode == ThemeMode::System ? QStringLiteral("system")
                                        : QStringLiteral("dark");
        m_settings.setValue(QStringLiteral("appearance/theme"), value);
        m_settings.sync();
        update();
    }

    void setBackgroundOpacity(int opacity)
    {
        m_backgroundOpacity = std::clamp(opacity, 70, 100);
        m_settings.setValue(QStringLiteral("appearance/backgroundOpacity"), m_backgroundOpacity);
        m_settings.sync();
        update();
    }

    void setCornerRadius(int radius)
    {
        m_cornerRadius = std::clamp(radius, 8, 28);
        m_settings.setValue(QStringLiteral("appearance/cornerRadius"), m_cornerRadius);
        m_settings.sync();
        update();
    }

    void startPopupAutoHideTimer()
    {
        m_popupAutoHideTimer.stop();
        if (m_popupAutoHideSeconds > 0) {
            m_popupAutoHideTimer.setInterval(m_popupAutoHideSeconds * 1000);
            m_popupAutoHideTimer.start();
        }
    }

    void setHoverExpansionEnabled(bool enabled)
    {
        m_hoverExpansionEnabled = enabled;
        m_settings.setValue(QStringLiteral("behaviour/hoverExpansion"), enabled);
        m_settings.sync();
        m_hoverCollapseTimer.stop();

        if (!enabled && !m_desktopForcedOpen && height() > CompactHeight) {
            animateToHeight(CompactHeight);
            if (m_userPopupActive && !underMouse()) {
                startPopupAutoHideTimer();
            } else {
                m_popupAutoHideTimer.stop();
            }
        } else if (enabled && m_userPopupActive && underMouse()) {
            m_popupAutoHideTimer.stop();
            animateToHeight(ExpandedHeight);
        }
        update();
    }

    void setAnimationDuration(int milliseconds)
    {
        m_animationDurationMs = std::clamp(milliseconds, 0, 1000);
        m_settings.setValue(QStringLiteral("behaviour/animationDurationMs"), m_animationDurationMs);
        m_settings.sync();
        m_heightAnimation.setDuration(m_animationDurationMs);
    }

    void setPopupAutoHideSeconds(int seconds)
    {
        m_popupAutoHideSeconds = std::clamp(seconds, 0, 60);
        m_settings.setValue(QStringLiteral("behaviour/popupAutoHideSeconds"), m_popupAutoHideSeconds);
        m_settings.sync();
        m_popupAutoHideTimer.setInterval(m_popupAutoHideSeconds * 1000);

        if (m_popupAutoHideSeconds == 0 || !m_userPopupActive || underMouse()) {
            m_popupAutoHideTimer.stop();
        } else {
            startPopupAutoHideTimer();
        }
    }

    void setHideWhenCovered(bool enabled)
    {
        m_hideWhenCovered = enabled;
        m_settings.setValue(QStringLiteral("desktop/hideWhenCovered"), enabled);
        m_settings.sync();
        if (!enabled) {
            setPopupAreaBlocked(false);
        }
    }

    void resetSettings()
    {
        setArtworkMode(ArtworkMode::Vinyl);
        setThemeMode(ThemeMode::Dark);
        setBackgroundOpacity(96);
        setCornerRadius(17);
        setHoverExpansionEnabled(true);
        setAnimationDuration(ExpandAnimationMilliseconds);
        setPopupAutoHideSeconds(4);
        setHideWhenCovered(true);
    }

    void showSettingsMenu(const QPoint &globalPosition)
    {
        QMenu menu(this);
        const bool light = usesLightAppearance();
        const QString menuBackground = light ? QStringLiteral("#f7f9fd") : QStringLiteral("#191c29");
        const QString menuForeground = light ? QStringLiteral("#202633") : QStringLiteral("#f3f5fb");
        const QString menuHover = light ? QStringLiteral("#e5eaf3") : QStringLiteral("#30384b");
        const QString menuBorder = light ? QStringLiteral("#c5cddb") : QStringLiteral("#42495d");
        const QString menuMuted = light ? QStringLiteral("#747e90") : QStringLiteral("#aab4c8");
        menu.setStyleSheet(QStringLiteral(
            "QMenu { background-color: %1; color: %2; border: 1px solid %3;"
            " border-radius: 11px; padding: 6px; }"
            "QMenu::item { padding: 8px 30px 8px 12px; border-radius: 6px; margin: 2px 3px; }"
            "QMenu::item:selected { background-color: %4; }"
            "QMenu::item:disabled { color: %5; }"
            "QMenu::separator { height: 1px; background: %3; margin: 5px 8px; }"
        ).arg(menuBackground).arg(menuForeground).arg(menuBorder)
         .arg(menuHover).arg(menuMuted));

        QAction *heading = menu.addAction(QStringLiteral("MediaTable settings"));
        heading->setEnabled(false);

        QMenu *artworkMenu = menu.addMenu(QStringLiteral("Artwork"));
        QActionGroup artworkGroup(artworkMenu);
        artworkGroup.setExclusive(true);
        const auto addArtworkChoice = [&](const QString &label, ArtworkMode mode) {
            QAction *action = artworkMenu->addAction(label);
            action->setCheckable(true);
            action->setChecked(m_artworkMode == mode);
            artworkGroup.addAction(action);
            QObject::connect(action, &QAction::triggered, &menu, [this, mode] {
                setArtworkMode(mode);
            });
        };
        addArtworkChoice(QStringLiteral("Vinyl"), ArtworkMode::Vinyl);
        addArtworkChoice(QStringLiteral("Album"), ArtworkMode::Album);
        addArtworkChoice(QStringLiteral("Nothing"), ArtworkMode::Nothing);

        QMenu *appearanceMenu = menu.addMenu(QStringLiteral("Appearance"));
        QMenu *themeMenu = appearanceMenu->addMenu(QStringLiteral("Theme"));
        QActionGroup themeGroup(themeMenu);
        themeGroup.setExclusive(true);
        const auto addThemeChoice = [&](const QString &label, ThemeMode mode) {
            QAction *action = themeMenu->addAction(label);
            action->setCheckable(true);
            action->setChecked(m_themeMode == mode);
            themeGroup.addAction(action);
            QObject::connect(action, &QAction::triggered, &menu, [this, mode] {
                setThemeMode(mode);
            });
        };
        addThemeChoice(QStringLiteral("Match system"), ThemeMode::System);
        addThemeChoice(QStringLiteral("Dark"), ThemeMode::Dark);
        addThemeChoice(QStringLiteral("Light"), ThemeMode::Light);

        QMenu *opacityMenu = appearanceMenu->addMenu(QStringLiteral("Background opacity"));
        QActionGroup opacityGroup(opacityMenu);
        opacityGroup.setExclusive(true);
        for (const int opacity : {85, 92, 96, 100}) {
            QAction *action = opacityMenu->addAction(QStringLiteral("%1%").arg(opacity));
            action->setCheckable(true);
            action->setChecked(m_backgroundOpacity == opacity);
            opacityGroup.addAction(action);
            QObject::connect(action, &QAction::triggered, &menu, [this, opacity] {
                setBackgroundOpacity(opacity);
            });
        }

        QMenu *radiusMenu = appearanceMenu->addMenu(QStringLiteral("Corner radius"));
        QActionGroup radiusGroup(radiusMenu);
        radiusGroup.setExclusive(true);
        for (const int radius : {12, 17, 22, 26}) {
            QAction *action = radiusMenu->addAction(QStringLiteral("%1 px").arg(radius));
            action->setCheckable(true);
            action->setChecked(m_cornerRadius == radius);
            radiusGroup.addAction(action);
            QObject::connect(action, &QAction::triggered, &menu, [this, radius] {
                setCornerRadius(radius);
            });
        }

        QMenu *behaviourMenu = menu.addMenu(QStringLiteral("Behaviour"));
        QAction *hoverAction = behaviourMenu->addAction(QStringLiteral("Expand on hover"));
        hoverAction->setCheckable(true);
        hoverAction->setChecked(m_hoverExpansionEnabled);
        QObject::connect(hoverAction, &QAction::toggled, &menu, [this](bool enabled) {
            setHoverExpansionEnabled(enabled);
        });

        QMenu *animationMenu = behaviourMenu->addMenu(QStringLiteral("Expand/collapse speed"));
        QActionGroup animationGroup(animationMenu);
        animationGroup.setExclusive(true);
        const std::array<std::pair<const char *, int>, 4> animationChoices{{
            {"Instant", 0}, {"Fast", 140}, {"Normal", ExpandAnimationMilliseconds}, {"Slow", 360}
        }};
        for (const auto &[label, milliseconds] : animationChoices) {
            QAction *action = animationMenu->addAction(QString::fromLatin1(label));
            action->setCheckable(true);
            action->setChecked(m_animationDurationMs == milliseconds);
            animationGroup.addAction(action);
            QObject::connect(action, &QAction::triggered, &menu, [this, milliseconds] {
                setAnimationDuration(milliseconds);
            });
        }

        QMenu *autoHideMenu = behaviourMenu->addMenu(QStringLiteral("Temporary popup auto-hide"));
        QActionGroup autoHideGroup(autoHideMenu);
        autoHideGroup.setExclusive(true);
        const std::array<std::pair<const char *, int>, 4> autoHideChoices{{
            {"2 seconds", 2}, {"4 seconds", 4}, {"8 seconds", 8}, {"Never", 0}
        }};
        for (const auto &[label, seconds] : autoHideChoices) {
            QAction *action = autoHideMenu->addAction(QString::fromLatin1(label));
            action->setCheckable(true);
            action->setChecked(m_popupAutoHideSeconds == seconds);
            autoHideGroup.addAction(action);
            QObject::connect(action, &QAction::triggered, &menu, [this, seconds] {
                setPopupAutoHideSeconds(seconds);
            });
        }

        QMenu *desktopMenu = menu.addMenu(QStringLiteral("Desktop behaviour"));
        QAction *hideWhenCoveredAction = desktopMenu->addAction(
            QStringLiteral("Hide when windows cover the card"));
        hideWhenCoveredAction->setCheckable(true);
        hideWhenCoveredAction->setChecked(m_hideWhenCovered);
        QObject::connect(hideWhenCoveredAction, &QAction::toggled, &menu, [this](bool enabled) {
            setHideWhenCovered(enabled);
        });

        menu.addSeparator();
        QAction *resetAction = menu.addAction(QStringLiteral("Reset to defaults"));
        QObject::connect(resetAction, &QAction::triggered, &menu, [this] {
            resetSettings();
        });

        menu.exec(globalPosition);
    }

    void setPlayerState(const PlayerState &state)
    {
        const bool selectedPlayerChanged = !m_state.service.isEmpty()
            && !state.service.isEmpty() && state.service != m_state.service;

        // If the selected player changes while the card is hidden in the hot
        // corner, retain the destination snapshot instead of finishing the entire
        // transition off-screen. The old content can then fade out visibly as the
        // popup is summoned, followed by the new player's artwork and text fading in.
        if (m_hotCornerOnly && selectedPlayerChanged) {
            m_transitionArtworkFallbackTimer.stop();
            m_waitingForTransitionArtwork = false;
            m_fadeInAfterPlayerTransition = false;
            m_contentFadeAnimation.stop();
            m_pendingPlayerState = state;
            m_pendingPlayerStateValid = true;
            m_hiddenPlayerTransitionPending = true;
            return;
        }

        // If a transition is already fading out, keep the latest snapshot for
        // the destination player. This avoids queuing several rapid MPRIS updates.
        if (m_pendingPlayerStateValid) {
            m_pendingPlayerState = state;
            return;
        }

        if (selectedPlayerChanged) {
            m_transitionArtworkFallbackTimer.stop();
            m_waitingForTransitionArtwork = false;
            m_fadeInAfterPlayerTransition = false;
            m_contentFadeAnimation.stop();
            m_pendingPlayerState = state;
            m_pendingPlayerStateValid = true;
            m_contentFadeAnimation.setDuration(180);
            m_contentFadeAnimation.setStartValue(m_contentOpacity);
            m_contentFadeAnimation.setEndValue(0.0);
            m_contentFadeAnimation.start();
            return;
        }

        applyPlayerState(state);
    }

    void applyPlayerState(const PlayerState &state)
    {
        const bool hadPreviousTrack = !m_state.service.isEmpty();
        const double previousVinylAngle = currentVinylRotationDegrees();
        const bool sameTrack = isSameTrack(m_state, state);
        const bool artworkChanged = state.artUrl != m_state.artUrl;
        const bool playbackChanged = m_state.isPlaying() != state.isPlaying();
        const bool hadLocalPosition = sameTrack && m_positionOverrideValid;
        const qint64 estimatedBeforeUpdate = estimatedPositionUsec();
        const bool firefoxPositionMayBeStale =
            state.service.contains(QStringLiteral("firefox"), Qt::CaseInsensitive)
            && m_positionOverrideValid;
        // Spotify's Previous button restarts the current track when it has played
        // for a few seconds; its track ID/title can remain unchanged. Detect the
        // resulting large jump to the beginning separately from a metadata change.
        // Firefox is excluded while its known post-seek zero-position fallback is
        // active, so that browser regression cannot masquerade as a track restart.
        const bool sameTrackRestarted = sameTrack
            && !firefoxPositionMayBeStale
            && estimatedBeforeUpdate >= 2500000LL
            && state.positionUsec >= 0
            && state.positionUsec <= 1200000LL
            && (estimatedBeforeUpdate - state.positionUsec) >= 2000000LL
            && state.playbackStatus != QStringLiteral("Stopped");

        PlayerState nextState = state;
        if (sameTrack) {
            // Firefox may temporarily drop mpris:length after a seek. Keep the
            // last known duration for this same track so further scrubbing remains enabled.
            if (nextState.lengthUsec <= 0) {
                nextState.lengthUsec = m_state.lengthUsec;
            }

            // Firefox can briefly publish an incomplete media state after a seek.
            // Preserve its last known seek capability and track identifier during
            // that transient zero-position / missing-duration update.
            const bool firefoxTransientSeekUpdate =
                state.service.contains(QStringLiteral("firefox"), Qt::CaseInsensitive)
                && m_positionOverrideValid;
            if (firefoxTransientSeekUpdate) {
                if (!nextState.canSeek && m_state.canSeek) {
                    nextState.canSeek = true;
                }
                if (nextState.trackId.isEmpty()) {
                    nextState.trackId = m_state.trackId;
                }
            }

            // Likewise, retain the visible metadata during a transient empty Metadata update.
            if (nextState.title == QStringLiteral("Now playing")) {
                nextState.title = m_state.title;
                if (nextState.artist == nextState.identity) {
                    nextState.artist = m_state.artist;
                }
            }
        }

        // MPRIS Position is a polled property, not a continuous signal. Some
        // players (Firefox in particular) return a rounded/stale sample. Keep the
        // monotonic local playhead for small sample differences; only accept a
        // new sample as a discontinuity when it differs enough to represent a
        // real external seek. This avoids the timeline jumping backwards on each
        // poll while preserving larger external seeks and track restarts.
        if (sameTrack && !sameTrackRestarted) {
            const bool isFirefox = state.service.contains(QStringLiteral("firefox"),
                                                           Qt::CaseInsensitive);
            const qint64 toleranceUsec = isFirefox ? 1100000LL : 550000LL;
            const qint64 sampleDifference = state.positionUsec - estimatedBeforeUpdate;
            if (std::abs(sampleDifference) <= toleranceUsec) {
                nextState.positionUsec = estimatedBeforeUpdate;
            }
        }

        m_state = nextState;
        m_positionClock.restart();

        if (playbackChanged) {
            animateAmbientLighting(m_state.isPlaying());
        }

        if (!sameTrack || sameTrackRestarted) {
            m_positionOverrideValid = false;
            m_positionOverrideClock.invalidate();
            m_previewPositionValid = false;
            m_pendingSeekUsec = 0;
            m_previewResetTimer.stop();
            m_seekDispatchTimer.stop();
            m_pendingSeekUsec = 0;

            if (hadPreviousTrack) {
                startVinylReset(previousVinylAngle);
            } else {
                m_vinylResetAnimation.stop();
                m_vinylResetActive = false;
                m_vinylResetAngle = 0.0;
            }
            if (sameTrackRestarted) {
                // A Spotify Previous action can restart the same track without
                // changing metadata or playback status. Treat it as a media event
                // so the flyout appears and the platter visibly returns to zero.
                showCompactCard();
            }
        } else if (hadLocalPosition) {
            if (m_state.playbackStatus == QStringLiteral("Paused")) {
                // Firefox reports the correct position while paused, and Pause/Play
                // is known to restore its MPRIS position reporting.
                m_positionOverrideValid = false;
                m_positionOverrideClock.invalidate();
            } else if (m_state.isPlaying()) {
                const bool isFirefox = state.service.contains(
                    QStringLiteral("firefox"), Qt::CaseInsensitive);
                const qint64 reported = state.positionUsec;
                const qint64 difference = std::abs(reported - estimatedBeforeUpdate);
                const bool reportLooksValid = reported > 0
                    && difference <= (isFirefox ? 1100000LL : 550000LL);

                // Firefox's MPRIS Position can appear valid briefly and then return
                // zero after the same seek. Once we've sought in a Firefox playing
                // session, keep our elapsed-time estimate authoritative until pause
                // or a confirmed track change instead of trusting one good sample.
                if (isFirefox) {
                    m_positionOverrideUsec = estimatedBeforeUpdate;
                    m_positionOverrideClock.restart();
                    m_positionOverrideValid = true;
                } else if (reportLooksValid) {
                    m_positionOverrideValid = false;
                    m_positionOverrideClock.invalidate();
                } else {
                    m_positionOverrideUsec = estimatedBeforeUpdate;
                    m_positionOverrideClock.restart();
                    m_positionOverrideValid = true;
                }
            } else {
                m_positionOverrideValid = false;
                m_positionOverrideClock.invalidate();
            }
        }

        if (m_fadeInAfterPlayerTransition && !m_waitingForTransitionArtwork) {
            if (artworkChanged) {
                m_waitingForTransitionArtwork = true;
                m_transitionArtworkFallbackTimer.start();
            } else {
                finishPlayerTransitionFadeIn();
            }
        }

        if (artworkChanged) {
            if (m_waitingForTransitionArtwork) {
                m_transitionArtworkFallbackTimer.start();
            }
            loadArtwork(state.artUrl);
        }

        update();
    }

    // Reveal the card after an explicit media event or top-left hot-corner click.
    // This temporarily overrides the persistent expanded-desktop state and allows
    // the popup to appear over an application window; hovering expands it, while
    // leaving it returns it to compact height.
    void showCompactCard(bool summonedFromHotCorner = false)
    {
        m_desktopForcedOpen = false;
        m_userPopupActive = true;
        m_hoverCollapseTimer.stop();
        m_popupAutoHideTimer.stop();
        m_hotCornerOnly = false;
        m_waitForPointerMovementBeforeExpand = summonedFromHotCorner;
        if (summonedFromHotCorner) {
            m_hotCornerSummonPosition = QCursor::pos();
        }

        resize(CardWidth, std::max(height(), CompactHeight));
        syncLayerSurfaceGeometry();
        show();
        raise();
        if (!m_positionTimer.isActive()) {
            m_positionTimer.start();
        }

        // The user-triggered popup is allowed above foreground windows even when
        // KWin currently reports the desktop footprint as occupied. The next
        // blocked/unblocked transition restores the automatic desktop behavior.
        const bool pointerOverCard = underMouse() && !m_waitForPointerMovementBeforeExpand;
        if (summonedFromHotCorner || !pointerOverCard || !m_hoverExpansionEnabled) {
            animateToHeight(CompactHeight);
            if (pointerOverCard) {
                m_popupAutoHideTimer.stop();
            } else {
                startPopupAutoHideTimer();
            }
        } else {
            m_popupAutoHideTimer.stop();
            animateToHeight(ExpandedHeight);
        }

        if (m_state.isValid()) {
            revealHiddenPlayerTransition();
        }
        update();
    }

    void revealHiddenPlayerTransition()
    {
        if (!m_hiddenPlayerTransitionPending || !m_pendingPlayerStateValid) {
            m_hiddenPlayerTransitionPending = false;
            return;
        }

        m_hiddenPlayerTransitionPending = false;

        // If the selection returned to the already displayed player while the
        // flyout was hidden, apply its latest snapshot without a pointless fade.
        if (m_pendingPlayerState.service == m_state.service) {
            const PlayerState latest = m_pendingPlayerState;
            m_pendingPlayerState = {};
            m_pendingPlayerStateValid = false;
            applyPlayerState(latest);
            return;
        }

        m_transitionArtworkFallbackTimer.stop();
        m_waitingForTransitionArtwork = false;
        m_fadeInAfterPlayerTransition = false;
        m_contentFadeAnimation.stop();
        m_contentFadeAnimation.setDuration(180);
        m_contentFadeAnimation.setStartValue(m_contentOpacity);
        m_contentFadeAnimation.setEndValue(0.0);
        m_contentFadeAnimation.start();
    }

    void showTestCard()
    {
        PlayerState demo;
        demo.service = QStringLiteral("test");
        demo.identity = QStringLiteral("MediaTable");
        demo.title = QStringLiteral("Overlay test");
        demo.artist = QStringLiteral("Hover to expand this card");
        demo.playbackStatus = QStringLiteral("Playing");
        demo.lengthUsec = 215000000;
        demo.positionUsec = 72000000;
        demo.canGoPrevious = true;
        demo.canGoNext = true;
        demo.canPause = true;
        demo.canSeek = true;
        setPlayerState(demo);
        showCompactCard();
    }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        if (!m_hotCornerOnly && !m_waitForPointerMovementBeforeExpand) {
            m_hoverCollapseTimer.stop();
            m_popupAutoHideTimer.stop();
            if (m_hoverExpansionEnabled) {
                animateToHeight(ExpandedHeight);
            }
        }
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        if (!m_hotCornerOnly && !m_desktopForcedOpen) {
            if (m_hoverExpansionEnabled) {
                m_hoverCollapseTimer.start();
            }
            if (m_userPopupActive) {
                startPopupAutoHideTimer();
            }
        }
        QWidget::leaveEvent(event);
    }

    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        if (!m_hotCornerOnly && !m_positionTimer.isActive()) {
            m_positionTimer.start();
        }
    }

    void hideEvent(QHideEvent *event) override
    {
        m_positionTimer.stop();
        QWidget::hideEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        if (m_hotCornerOnly) {
            QPainter clearPainter(this);
            clearPainter.setCompositionMode(QPainter::CompositionMode_Source);
            clearPainter.fillRect(rect(), Qt::transparent);
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRectF cardRect(0.5, 0.5, width() - 1.0, height() - 1.0);
        QPainterPath cardPath;
        cardPath.addRoundedRect(cardRect, m_cornerRadius, m_cornerRadius);
        painter.fillPath(cardPath, cardBackgroundColor());
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(cardOutlineColor(), 0.8));
        painter.drawPath(cardPath);
        painter.save();
        painter.setOpacity(m_contentOpacity);
        drawArtworkLighting(painter, cardPath, cardRect);

        // Artwork is user-selectable. Vinyl remains the default and keeps its
        // interactive scrub behaviour; Album shows a static cover; Nothing
        // reclaims the artwork column for a wider text layout.
        if (m_artworkMode == ArtworkMode::Vinyl) {
            drawVinyl(painter);
        } else if (m_artworkMode == ArtworkMode::Album) {
            drawAlbumArtwork(painter);
        }

        const double expansion = expansionProgress();
        const double textX = m_artworkMode == ArtworkMode::Nothing
            ? 18.0 : 84.0 + (145.0 - 84.0) * expansion;
        const double titleY = 17.0 + (43.0 - 17.0) * expansion;
        const double artistY = 43.0 + (75.0 - 43.0) * expansion;
        const double textWidth = CardWidth - textX - 15.0;

        // In expanded mode the player name occupies the small header line,
        // matching the original full-size media card layout.
        if (expansion > 0.02) {
            painter.save();
            painter.setOpacity(expansion * m_contentOpacity);
            QFont identityFont = font();
            identityFont.setPixelSize(11);
            painter.setFont(identityFont);
            painter.setPen(identityTextColor());
            const QRectF identityRect = playerIdentityTextRect();
            const QString identity = displayedPlayerIdentity(identityFont);
            painter.drawText(identityRect, Qt::AlignVCenter | Qt::AlignLeft, identity);
            if (m_hasMultiplePlayers) {
                drawPlayerSwitchButton(painter);
            }
            painter.restore();
        }

        QFont titleFont = font();
        titleFont.setPixelSize(16);
        titleFont.setWeight(QFont::DemiBold);
        painter.setFont(titleFont);
        painter.setPen(primaryTextColor());
        const QString title = QFontMetrics(titleFont).elidedText(
            m_state.title, Qt::ElideRight, int(textWidth));
        painter.drawText(QRectF(textX, titleY, textWidth, 26),
                         Qt::AlignVCenter | Qt::AlignLeft, title);

        QFont artistFont = font();
        artistFont.setPixelSize(12);
        painter.setFont(artistFont);
        painter.setPen(secondaryTextColor());
        const QString artist = QFontMetrics(artistFont).elidedText(
            m_state.artist, Qt::ElideRight, int(textWidth));
        painter.drawText(QRectF(textX, artistY, textWidth, 19),
                         Qt::AlignVCenter | Qt::AlignLeft, artist);


        // The extra content is simply clipped until the card animates downward.
        if (height() <= CompactHeight + 2) {
            painter.restore();
            return;
        }

        const double controlShift = 56.0 * expansion;
        drawPrevious(painter, QRectF(116 + controlShift, 94, 42, 38), m_state.canGoPrevious);
        drawPlayPause(painter, QRectF(162 + controlShift, 94, 42, 38),
                      m_state.isPlaying(), m_state.canPause || m_state.canPlay);
        drawNext(painter, QRectF(208 + controlShift, 94, 42, 38), m_state.canGoNext);

        const qint64 currentPositionUsec = estimatedPositionUsec();
        const QString elapsedText = formatTrackTime(currentPositionUsec);
        const QString durationText = m_state.lengthUsec > 0
            ? formatTrackTime(m_state.lengthUsec) : QStringLiteral("--:--");

        // Numeric elapsed and total times sit above the existing seek bar.
        // Keeping the readout here preserves the vinyl as the main visual focus.
        QFont timeFont = font();
        timeFont.setPixelSize(10);
        painter.setFont(timeFont);
        painter.setPen(mutedTextColor());
        painter.drawText(QRectF(15, 135, 75, 12), Qt::AlignLeft | Qt::AlignVCenter,
                         elapsedText);
        painter.drawText(QRectF(CardWidth - 90, 135, 75, 12),
                         Qt::AlignRight | Qt::AlignVCenter, durationText);

        const QRectF progressRect(15, 153, CardWidth - 30, 3);
        painter.setPen(Qt::NoPen);
        painter.setBrush(progressTrackColor());
        painter.drawRoundedRect(progressRect, 1.5, 1.5);

        const double totalSeconds = m_state.lengthUsec > 0
            ? double(m_state.lengthUsec) / 1000000.0 : 0.0;
        const double currentSeconds = double(currentPositionUsec) / 1000000.0;
        const double fraction = totalSeconds > 0.0
            ? std::clamp(currentSeconds / totalSeconds, 0.0, 1.0) : 0.0;
        QRectF filledProgress = progressRect;
        filledProgress.setWidth(progressRect.width() * fraction);
        painter.setBrush(progressBarAccentColor());
        painter.drawRoundedRect(filledProgress, 1.5, 1.5);
        painter.restore();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        const QPointF point = event->position();

        if (m_waitForPointerMovementBeforeExpand) {
            const QPoint globalPosition = event->globalPosition().toPoint();
            if ((globalPosition - m_hotCornerSummonPosition).manhattanLength() >= 2) {
                m_waitForPointerMovementBeforeExpand = false;
                if (QRectF(rect()).contains(point)) {
                    m_popupAutoHideTimer.stop();
                    if (m_hoverExpansionEnabled) {
                        m_hoverCollapseTimer.stop();
                        animateToHeight(ExpandedHeight);
                    }
                }
            }
        }

        if (m_draggingVinyl && (event->buttons() & Qt::LeftButton)) {
            const double angle = vinylAngleAt(point);
            double deltaDegrees = angle - m_lastVinylAngle;
            if (deltaDegrees > 180.0) {
                deltaDegrees -= 360.0;
            } else if (deltaDegrees < -180.0) {
                deltaDegrees += 360.0;
            }

            m_lastVinylAngle = angle;
            update();

            if (m_state.lengthUsec > 0 && std::abs(deltaDegrees) > 0.02) {
                // Match the platter's physical speed: 6 degrees of drag = 1 second.
                // A full 360-degree hand rotation therefore scrubs exactly 60 seconds,
                // independent of the current track's total duration.
                const qint64 offset = qint64(
                    (deltaDegrees / VinylRotationDegreesPerSecond) * 1000000.0);
                queueSeek(offset);
            }

            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }

        const bool overVinyl = isOverVinyl(point);
        const double controlShift = 56.0 * expansionProgress();
        const bool overPlayerSwitch = m_hasMultiplePlayers
            && playerSwitchButtonRect().contains(point);
        const bool overButton = overPlayerSwitch
            || (height() > CompactHeight
                && (QRectF(116 + controlShift, 94, 42, 38).contains(point)
                    || QRectF(162 + controlShift, 94, 42, 38).contains(point)
                    || QRectF(208 + controlShift, 94, 42, 38).contains(point)));
        setCursor(overVinyl && m_state.canSeek
                      ? Qt::OpenHandCursor
                      : (overButton ? Qt::PointingHandCursor : Qt::ArrowCursor));
        QWidget::mouseMoveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (m_hotCornerOnly) {
            if (event->button() == Qt::LeftButton) {
                m_waitForPointerMovementBeforeExpand = true;
                m_hotCornerSummonPosition = event->globalPosition().toPoint();
                showCompactCard();
                event->accept();
                return;
            }
            event->ignore();
            return;
        }

        if (event->button() == Qt::RightButton) {
            showSettingsMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        }

        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }

        const QPointF point = event->position();
        if (m_hasMultiplePlayers && playerSwitchButtonRect().contains(point)) {
            if (m_playerSwitchCallback) {
                m_playerSwitchCallback();
            }
            event->accept();
            return;
        }

        if (isOverVinyl(point) && m_state.canSeek && m_state.lengthUsec > 0) {
            if (m_vinylResetActive) {
                m_vinylResetAnimation.stop();
                m_vinylResetActive = false;
                // The canonical phase is always derived from the playhead. If a
                // user grabs the record mid-reset, resume from that phase.
            }
            m_draggingVinyl = true;
            m_lastVinylAngle = vinylAngleAt(point);
            m_hoverCollapseTimer.stop();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }

        if (point.y() >= CompactHeight && point.y() <= ExpandedHeight) {
            const double controlShift = 56.0 * expansionProgress();
            if (QRectF(116 + controlShift, 94, 42, 38).contains(point) && m_state.canGoPrevious) {
                invokeControl(QStringLiteral("Previous"));
            } else if (QRectF(162 + controlShift, 94, 42, 38).contains(point)
                       && (m_state.canPause || m_state.canPlay)) {
                invokeControl(QStringLiteral("PlayPause"));
            } else if (QRectF(208 + controlShift, 94, 42, 38).contains(point) && m_state.canGoNext) {
                invokeControl(QStringLiteral("Next"));
            }
        }
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_draggingVinyl) {
            // The accepted preview seek already determines the matching vinyl
            // phase (6 degrees per second), so no local rotation offset is kept.
            m_draggingVinyl = false;
            flushPendingSeek();
            update();
            setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }

        QWidget::mouseReleaseEvent(event);
    }

    void wheelEvent(QWheelEvent *event) override
    {
        if (!isOverVinyl(event->position()) || !m_state.canSeek
            || m_state.lengthUsec <= 0) {
            QWidget::wheelEvent(event);
            return;
        }

        double detents = double(event->angleDelta().y()) / 120.0;
        if (std::abs(detents) < 0.001 && !event->pixelDelta().isNull()) {
            detents = double(event->pixelDelta().y()) / 80.0;
        }

        if (std::abs(detents) >= 0.001) {
            // Treat the wheel as pushing the record at the cursor position.
            // In Qt's screen coordinates, positive wheel delta is up and
            // positive X is the right half of the record. Scrolling down on
            // the right therefore rotates clockwise (forward); on the left
            // it rotates counterclockwise (backward). Scrolling up reverses it.
            const double horizontalFromCenter =
                event->position().x() - currentVinylRect().center().x();
            const double side = horizontalFromCenter < 0.0 ? -1.0 : 1.0;
            const qint64 offset = qint64(
                -detents * side * double(WheelSeekStepUsec));
            queueSeek(offset);
        }
        event->accept();
    }

private:
    QRectF playerIdentityTextRect() const
    {
        const double expansion = expansionProgress();
        const double textX = m_artworkMode == ArtworkMode::Nothing
            ? 18.0 : 84.0 + (145.0 - 84.0) * expansion;
        const double availableWidth = CardWidth - textX - 15.0;
        // Reserve a fixed slot at the far-right edge for the source-switch
        // chevron. The identity label truncates before this slot instead of
        // pushing the button along with the end of the label.
        const double switchReserve = m_hasMultiplePlayers
            ? PlayerSwitchButtonWidth + PlayerSwitchTextGap : 0.0;
        return QRectF(textX, 16.0,
                      std::max(0.0, availableWidth - switchReserve), 22.0);
    }

    QString displayedPlayerIdentity(const QFont &identityFont) const
    {
        const int availableWidth = std::max(0, int(playerIdentityTextRect().width()));
        return QFontMetrics(identityFont).elidedText(
            m_state.identity, Qt::ElideRight, availableWidth);
    }

    QRectF playerSwitchButtonRect() const
    {
        if (!m_hasMultiplePlayers || expansionProgress() <= 0.02) {
            return {};
        }

        // Keep the switch chevron anchored to the card's right edge. Its
        // position is independent of the source name length, and the identity
        // text rectangle above reserves space so the two never overlap.
        return QRectF(CardWidth - PlayerSwitchButtonRightMargin
                          - PlayerSwitchButtonWidth,
                      16.0, PlayerSwitchButtonWidth, 22.0);
    }

    void drawPlayerSwitchButton(QPainter &painter)
    {
        const QRectF buttonRect = playerSwitchButtonRect();
        if (buttonRect.isEmpty()) {
            return;
        }

        painter.save();
        QPainterPath chevron;
        const QPointF center = buttonRect.center();
        chevron.moveTo(center.x() - 2.5, center.y() - 4.2);
        chevron.lineTo(center.x() + 1.8, center.y());
        chevron.lineTo(center.x() - 2.5, center.y() + 4.2);

        painter.setBrush(Qt::NoBrush);
        QColor switchColor = identityTextColor();
        switchColor.setAlpha(225);
        painter.setPen(QPen(switchColor, 1.5,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(chevron);
        painter.restore();
    }

    double expansionProgress() const
    {
        return std::clamp(
            double(height() - CompactHeight) / double(ExpandedHeight - CompactHeight),
            0.0, 1.0);
    }

    QRectF currentVinylRect() const
    {
        const double amount = expansionProgress();
        const double diameter = CompactVinylDiameter
            + (ExpandedVinylDiameter - CompactVinylDiameter) * amount;
        const double y = CompactVinylY
            + (ExpandedVinylY - CompactVinylY) * amount;
        return QRectF(VinylX, y, diameter, diameter);
    }

    bool isOverVinyl(const QPointF &point) const
    {
        if (m_artworkMode != ArtworkMode::Vinyl) {
            return false;
        }
        const QRectF rect = currentVinylRect();
        const QPointF center = rect.center();
        const double radius = rect.width() / 2.0;
        const double dx = point.x() - center.x();
        const double dy = point.y() - center.y();
        return (dx * dx + dy * dy) <= radius * radius;
    }

    double vinylAngleAt(const QPointF &point) const
    {
        const QPointF center = currentVinylRect().center();
        return std::atan2(point.y() - center.y(), point.x() - center.x())
            * 180.0 / Pi;
    }

    static double normalizeDegrees(double degrees)
    {
        double result = std::fmod(degrees, 360.0);
        if (result < 0.0) {
            result += 360.0;
        }
        return result;
    }

    static QString formatTrackTime(qint64 positionUsec)
    {
        const qint64 totalSeconds = std::max<qint64>(0, positionUsec / 1000000LL);
        const qint64 seconds = totalSeconds % 60;
        const qint64 totalMinutes = totalSeconds / 60;

        if (totalMinutes >= 60) {
            const qint64 hours = totalMinutes / 60;
            const qint64 minutes = totalMinutes % 60;
            return QStringLiteral("%1:%2:%3")
                .arg(hours)
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(seconds, 2, 10, QLatin1Char('0'));
        }

        return QStringLiteral("%1:%2")
            .arg(totalMinutes)
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    double timelineRotationDegrees() const
    {
        const double playbackSeconds = double(estimatedPositionUsec()) / 1000000.0;
        return playbackSeconds * VinylRotationDegreesPerSecond;
    }

    double currentVinylRotationDegrees() const
    {
        if (m_vinylResetActive) {
            return normalizeDegrees(m_vinylResetAngle);
        }
        // Canonical phase: six degrees for every second of the selected player's
        // current (or locally previewed) position.
        return normalizeDegrees(timelineRotationDegrees());
    }

    void startVinylReset(double startingAngle)
    {
        m_vinylResetAnimation.stop();
        m_draggingVinyl = false;
        m_vinylResetAngle = normalizeDegrees(startingAngle);

        // Rotate by the shortest route back to the 12 o'clock marker. The
        // animation target may be an equivalent 360-degree value; the finished
        // handler then establishes the exact zero-degree playback baseline.
        double deltaToZero = std::fmod(-m_vinylResetAngle, 360.0);
        if (deltaToZero > 180.0) {
            deltaToZero -= 360.0;
        } else if (deltaToZero < -180.0) {
            deltaToZero += 360.0;
        }

        const double degreesToTravel = std::abs(deltaToZero);
        const int resetDurationMilliseconds = std::clamp(
            qRound((degreesToTravel / 360.0) * 900.0), 140, 900);
        m_vinylResetAnimation.setDuration(resetDurationMilliseconds);
        m_vinylResetActive = true;
        m_vinylResetAnimation.setStartValue(m_vinylResetAngle);
        m_vinylResetAnimation.setEndValue(m_vinylResetAngle + deltaToZero);
        m_vinylResetAnimation.start();
        update();
    }

    void finishPlayerTransitionFadeIn()
    {
        if (!m_fadeInAfterPlayerTransition) {
            return;
        }

        m_transitionArtworkFallbackTimer.stop();
        m_waitingForTransitionArtwork = false;
        m_fadeInAfterPlayerTransition = false;
        m_contentFadeAnimation.stop();
        m_contentFadeAnimation.setDuration(240);
        m_contentFadeAnimation.setStartValue(m_contentOpacity);
        m_contentFadeAnimation.setEndValue(1.0);
        m_contentFadeAnimation.start();
    }

    qint64 estimatedPositionUsec() const
    {
        qint64 position = m_state.positionUsec;
        if (m_positionOverrideValid) {
            position = m_positionOverrideUsec;
            if (m_state.isPlaying() && m_positionOverrideClock.isValid()) {
                position += qRound64(double(m_positionOverrideClock.elapsed())
                                      * 1000.0 * m_state.rate);
            }
        } else if (m_previewPositionValid) {
            position = m_previewPositionUsec;
            if (m_state.isPlaying() && m_previewClock.isValid()) {
                position += qRound64(double(m_previewClock.elapsed())
                                      * 1000.0 * m_state.rate);
            }
        } else if (m_state.isPlaying() && m_positionClock.isValid()) {
            position += qRound64(double(m_positionClock.elapsed())
                                  * 1000.0 * m_state.rate);
        }

        if (m_state.lengthUsec > 0) {
            position = std::clamp<qint64>(position, 0, m_state.lengthUsec);
        }
        return std::max<qint64>(0, position);
    }

    void queueSeek(qint64 offsetUsec)
    {
        if (!m_state.canSeek || m_state.lengthUsec <= 0 || offsetUsec == 0) {
            return;
        }

        const qint64 current = estimatedPositionUsec();
        const qint64 target = std::clamp<qint64>(
            current + offsetUsec, 0, m_state.lengthUsec);
        const qint64 acceptedOffset = target - current;
        if (acceptedOffset == 0) {
            return;
        }

        m_previewPositionUsec = target;
        m_previewPositionValid = true;
        m_previewClock.restart();
        m_positionOverrideUsec = target;
        m_positionOverrideClock.restart();
        m_positionOverrideValid = true;
        m_previewResetTimer.start(PreviewResetMilliseconds);
        m_pendingSeekUsec += acceptedOffset;
        update();

        if (!m_seekDispatchTimer.isActive()) {
            m_seekDispatchTimer.start(SeekDispatchMilliseconds);
        }
    }

    void flushPendingSeek()
    {
        m_seekDispatchTimer.stop();
        if (m_pendingSeekUsec == 0) {
            return;
        }

        const qint64 offset = m_pendingSeekUsec;
        m_pendingSeekUsec = 0;
        if (m_seekCallback) {
            m_seekCallback(offset);
        }
    }

    void drawAlbumArtwork(QPainter &painter)
    {
        const QRectF artworkRect = currentVinylRect();
        QPainterPath artworkClip;
        artworkClip.addRoundedRect(artworkRect, 9.0, 9.0);

        painter.save();
        painter.setClipPath(artworkClip);
        if (!m_artwork.isNull()) {
            const int side = std::max(1, qRound(artworkRect.width()));
            const QImage cover = m_artwork.scaled(
                side, side, Qt::KeepAspectRatioByExpanding,
                Qt::SmoothTransformation);
            const int sourceX = std::max(0, (cover.width() - side) / 2);
            const int sourceY = std::max(0, (cover.height() - side) / 2);
            painter.drawImage(artworkRect, cover,
                              QRect(sourceX, sourceY, side, side));
        } else {
            QLinearGradient fallback(artworkRect.topLeft(), artworkRect.bottomRight());
            fallback.setColorAt(0.0, usesLightAppearance() ? QColor(226, 231, 240)
                                                               : QColor(54, 62, 82));
            fallback.setColorAt(1.0, usesLightAppearance() ? QColor(199, 207, 220)
                                                               : QColor(23, 27, 39));
            painter.fillRect(artworkRect, fallback);
            QFont glyphFont = font();
            glyphFont.setPixelSize(std::max(18, qRound(artworkRect.width() * 0.38)));
            painter.setFont(glyphFont);
            QColor fallbackText = primaryTextColor();
            fallbackText.setAlpha(205);
            painter.setPen(fallbackText);
            painter.drawText(artworkRect, Qt::AlignCenter, QStringLiteral("♫"));
        }
        painter.restore();

        painter.save();
        painter.setBrush(Qt::NoBrush);
        QColor artworkBorder = cardOutlineColor();
        artworkBorder.setAlpha(140);
        painter.setPen(QPen(artworkBorder, 1.0));
        const qreal artworkRadius = std::max(4.0, qreal(m_cornerRadius) * 0.55);
        painter.drawRoundedRect(artworkRect.adjusted(0.5, 0.5, -0.5, -0.5),
                                artworkRadius, artworkRadius);
        painter.restore();
    }

    void drawVinyl(QPainter &painter)
    {
        const QRectF recordRect = currentVinylRect();
        const double diameter = recordRect.width();
        const QPointF center = recordRect.center();

        // A stationary progress ring surrounds the spinning vinyl. The arc starts
        // at 12 o'clock and fills clockwise as the track advances.
        const double totalSeconds = m_state.lengthUsec > 0
            ? double(m_state.lengthUsec) / 1000000.0 : 0.0;
        const double currentSeconds = double(estimatedPositionUsec()) / 1000000.0;
        const double progressFraction = totalSeconds > 0.0
            ? std::clamp(currentSeconds / totalSeconds, 0.0, 1.0) : 0.0;
        const QRectF ringRect = recordRect.adjusted(-4.0, -4.0, 4.0, 4.0);

        painter.save();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(74, 84, 107, 175), 2.2,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawEllipse(ringRect);
        if (progressFraction > 0.0) {
            QColor progressColor = m_artworkAccent;
            progressColor.setAlpha(235);
            painter.setPen(QPen(progressColor, 2.5,
                                Qt::SolidLine, Qt::RoundCap));
            const int progressSpan = -qRound(progressFraction * 360.0 * 16.0);
            painter.drawArc(ringRect, 90 * 16, progressSpan);
        }
        painter.restore();

        // Playback rotation is derived from the selected player's playhead at
        // six degrees per second. Drag and wheel scrubbing update the preview
        // playhead immediately, so deriving the visual angle from that position
        // keeps the vinyl in sync with playback even while the pointer is moving.
        const double rotationDegrees = currentVinylRotationDegrees();

        painter.save();
        QPainterPath discClip;
        discClip.addEllipse(recordRect);
        painter.setClipPath(discClip);

        QLinearGradient discGradient(recordRect.topLeft(), recordRect.bottomRight());
        discGradient.setColorAt(0.0, QColor(43, 47, 59));
        discGradient.setColorAt(0.45, QColor(10, 12, 18));
        discGradient.setColorAt(1.0, QColor(32, 36, 47));
        painter.setPen(QPen(QColor(83, 91, 111, 180), 1.0));
        painter.setBrush(discGradient);
        painter.drawEllipse(recordRect);

        painter.translate(center);
        painter.rotate(rotationDegrees);

        // Fine concentric grooves scale with the disc, retaining detail when expanded.
        painter.setBrush(Qt::NoBrush);
        const int outerGroove = int(diameter * 0.91);
        const int innerGroove = int(diameter * 0.54);
        const int grooveStep = std::max(3, int(diameter * 0.055));
        for (int groove = outerGroove; groove >= innerGroove; groove -= grooveStep) {
            const int alpha = ((groove / grooveStep) % 2 == 0) ? 48 : 25;
            painter.setPen(QPen(QColor(174, 185, 208, alpha), 0.75));
            painter.drawEllipse(QRectF(-groove / 2.0, -groove / 2.0,
                                       groove, groove));
        }

        // Glossy arc and radial marker make rotation visible.
        const double markerRadius = diameter * 0.44;
        painter.setPen(QPen(QColor(193, 204, 226, 105),
                            std::max(0.8, diameter * 0.009),
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(QRectF(-diameter * 0.45, -diameter * 0.45,
                               diameter * 0.90, diameter * 0.90),
                        28 * 16, 44 * 16);
        painter.setPen(QPen(QColor(214, 222, 240, 190),
                            std::max(1.2, diameter * 0.014),
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(0, -markerRadius),
                         QPointF(0, -markerRadius + diameter * 0.055));

        // Circular centre label scales with the LP.
        const double labelDiameter = diameter * 0.46;
        const QRectF labelRect(-labelDiameter / 2.0, -labelDiameter / 2.0,
                               labelDiameter, labelDiameter);
        QPainterPath labelClip;
        labelClip.addEllipse(labelRect);
        painter.save();
        painter.setClipPath(labelClip);
        if (!m_artwork.isNull()) {
            const int labelPixelSize = std::max(1, int(std::ceil(labelDiameter)));
            const QImage labelImage = m_artwork.scaled(
                labelPixelSize, labelPixelSize,
                Qt::KeepAspectRatioByExpanding,
                Qt::SmoothTransformation);
            const int sx = std::max(0, (labelImage.width() - labelPixelSize) / 2);
            const int sy = std::max(0, (labelImage.height() - labelPixelSize) / 2);
            painter.drawImage(labelRect, labelImage,
                              QRect(sx, sy, labelPixelSize, labelPixelSize));
        } else {
            painter.fillRect(labelRect, QColor(61, 78, 115));
            painter.setPen(Qt::white);
            QFont labelFont = font();
            labelFont.setPixelSize(std::max(16, int(labelDiameter * 0.6)));
            painter.setFont(labelFont);
            painter.drawText(labelRect, Qt::AlignCenter, QStringLiteral("♫"));
        }
        painter.restore();

        painter.setPen(QPen(QColor(214, 220, 232, 210), 0.8));
        painter.setBrush(QColor(19, 22, 31));
        painter.drawEllipse(QPointF(0, 0), std::max(1.5, diameter * 0.013),
                            std::max(1.5, diameter * 0.013));
        painter.restore();
    }

    void animateToHeight(int targetHeight)
    {
        // Any path that normally requests compact height (hover timeout, media
        // notification, or player switch) must respect the persistent desktop
        // state while KWin says the popup footprint is unobstructed.
        if (m_desktopForcedOpen) {
            targetHeight = ExpandedHeight;
        }
        targetHeight = std::clamp(targetHeight, CompactHeight, ExpandedHeight);
        if (height() == targetHeight) {
            return;
        }
        m_heightAnimation.stop();
        if (m_animationDurationMs <= 0) {
            setCardHeight(targetHeight);
            return;
        }
        m_heightAnimation.setDuration(m_animationDurationMs);
        m_heightAnimation.setStartValue(height());
        m_heightAnimation.setEndValue(targetHeight);
        m_heightAnimation.start();
    }

    void setCardHeight(int cardHeight)
    {
        cardHeight = std::clamp(cardHeight, CompactHeight, ExpandedHeight);
        resize(CardWidth, cardHeight);
        syncLayerSurfaceGeometry();
        update();
    }

    void syncLayerSurfaceGeometry()
    {
        if (!m_layerWindow) {
            return;
        }

        if (m_hotCornerOnly) {
            m_layerWindow->setDesiredSize(QSize(HotCornerSize, HotCornerSize));
            m_layerWindow->setMargins(QMargins(0, 0, 0, 0));
        } else {
            m_layerWindow->setDesiredSize(QSize(CardWidth, height()));
            m_layerWindow->setMargins(QMargins(18, 18, 0, 0));
        }
    }

    void enterHotCornerMode()
    {
        m_desktopForcedOpen = false;
        m_userPopupActive = false;
        m_hoverCollapseTimer.stop();
        m_popupAutoHideTimer.stop();
        m_heightAnimation.stop();
        m_positionTimer.stop();
        m_draggingVinyl = false;
        m_waitForPointerMovementBeforeExpand = false;
        m_hotCornerOnly = true;
        resize(HotCornerSize, HotCornerSize);
        syncLayerSurfaceGeometry();
        update();
    }

    void invokeControl(const QString &method)
    {
        if (m_controlCallback) {
            m_controlCallback(method);
        }
    }

    QColor progressBarAccentColor() const
    {
        // Keep the timeline in the same colour family as the vinyl ring, but
        // slightly softer so the ring remains the more prominent accent.
        QColor color = m_artworkAccent.toHsv();
        int hue = color.hue();
        if (hue < 0) {
            hue = 215;
        }
        const int saturation = std::clamp(qRound(color.saturation() * 0.82), 0, 255);
        const int value = std::clamp(qRound(color.value() * 0.86), 0, 255);
        color.setHsv(hue, saturation, value, 230);
        return color;
    }

    void animateArtworkAccent(const QColor &targetColor)
    {
        if (!targetColor.isValid()) {
            return;
        }
        m_accentAnimation.stop();
        m_accentAnimation.setStartValue(m_artworkAccent);
        m_accentAnimation.setEndValue(targetColor);
        m_accentAnimation.start();
    }

    void animateAmbientLighting(bool enabled)
    {
        const double targetOpacity = enabled ? 1.0 : 0.0;
        if (std::abs(m_ambientOpacity - targetOpacity) < 0.001) {
            return;
        }

        m_ambientAnimation.stop();
        m_ambientAnimation.setStartValue(m_ambientOpacity);
        m_ambientAnimation.setEndValue(targetOpacity);
        m_ambientAnimation.start();
    }

    void drawArtworkLighting(QPainter &painter,
                             const QPainterPath &cardPath,
                             const QRectF &cardRect)
    {
        if (m_artworkMode == ArtworkMode::Nothing || m_ambientOpacity <= 0.001) {
            return;
        }

        const QRectF vinylRect = currentVinylRect();
        const double glowRadius = std::max(118.0, vinylRect.width() * 1.9);
        QColor centerColor = m_artworkAccent;
        centerColor.setAlpha(qRound(55.0 * m_ambientOpacity));
        QColor middleColor = m_artworkAccent;
        middleColor.setAlpha(qRound(27.0 * m_ambientOpacity));
        QColor transparentColor = m_artworkAccent;
        transparentColor.setAlpha(0);

        QRadialGradient glow(vinylRect.center(), glowRadius);
        glow.setColorAt(0.0, centerColor);
        glow.setColorAt(0.42, middleColor);
        glow.setColorAt(1.0, transparentColor);

        painter.save();
        painter.setClipPath(cardPath);
        painter.fillRect(cardRect, glow);
        painter.restore();

        // The tinted edge fades with the ambient glow, while timeline accents remain visible.
        QColor edgeColor = m_artworkAccent;
        edgeColor.setAlpha(qRound(34.0 * m_ambientOpacity));
        painter.save();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(edgeColor, 0.8));
        painter.drawPath(cardPath);
        painter.restore();
    }

    void loadArtwork(const QString &artUrl)
    {
        m_artwork = QImage();
        animateArtworkAccent(QColor(112, 125, 153));
        if (artUrl.isEmpty()) {
            finishPlayerTransitionFadeIn();
            update();
            return;
        }

        const QUrl url(artUrl);
        if (url.isLocalFile()) {
            if (m_artwork.load(url.toLocalFile())) {
                animateArtworkAccent(sampleArtworkAccent(m_artwork));
            }
            finishPlayerTransitionFadeIn();
            update();
            return;
        }

        if (url.scheme() != QStringLiteral("http")
            && url.scheme() != QStringLiteral("https")) {
            finishPlayerTransitionFadeIn();
            update();
            return;
        }

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("KDE-Media-Flyout/0.2"));
        QNetworkReply *reply = m_network.get(request);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, artUrl] {
            if (reply->error() == QNetworkReply::NoError
                && reply->bytesAvailable() <= 12 * 1024 * 1024) {
                QImage image;
                if (image.loadFromData(reply->readAll()) && m_state.artUrl == artUrl) {
                    m_artwork = image;
                    animateArtworkAccent(sampleArtworkAccent(m_artwork));
                    update();
                }
            }
            if (m_state.artUrl == artUrl) {
                finishPlayerTransitionFadeIn();
            }
            reply->deleteLater();
        });
    }

    void drawPrevious(QPainter &painter, const QRectF &rect, bool enabled)
    {
        const QColor color = enabled ? enabledControlColor() : disabledControlColor();
        const QPointF c = rect.center();
        painter.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(c.x() - 8, c.y() - 8), QPointF(c.x() - 8, c.y() + 8));
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        QPolygonF triangle;
        triangle << QPointF(c.x() + 9, c.y() - 9)
                 << QPointF(c.x() - 3, c.y())
                 << QPointF(c.x() + 9, c.y() + 9);
        painter.drawPolygon(triangle);
    }

    void drawNext(QPainter &painter, const QRectF &rect, bool enabled)
    {
        const QColor color = enabled ? enabledControlColor() : disabledControlColor();
        const QPointF c = rect.center();
        painter.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(c.x() + 8, c.y() - 8), QPointF(c.x() + 8, c.y() + 8));
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        QPolygonF triangle;
        triangle << QPointF(c.x() - 9, c.y() - 9)
                 << QPointF(c.x() + 3, c.y())
                 << QPointF(c.x() - 9, c.y() + 9);
        painter.drawPolygon(triangle);
    }

    void drawPlayPause(QPainter &painter, const QRectF &rect, bool playing, bool enabled)
    {
        const QColor color = enabled ? enabledControlColor() : disabledControlColor();
        const QPointF c = rect.center();
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        if (playing) {
            painter.drawRoundedRect(QRectF(c.x() - 7, c.y() - 9, 5, 18), 1, 1);
            painter.drawRoundedRect(QRectF(c.x() + 2, c.y() - 9, 5, 18), 1, 1);
        } else {
            QPolygonF triangle;
            triangle << QPointF(c.x() - 6, c.y() - 10)
                     << QPointF(c.x() + 10, c.y())
                     << QPointF(c.x() - 6, c.y() + 10);
            painter.drawPolygon(triangle);
        }
    }

    PlayerState m_state;
    QImage m_artwork;
    QColor m_artworkAccent = QColor(112, 151, 255);
    QNetworkAccessManager m_network;
    QSettings m_settings;
    ArtworkMode m_artworkMode = ArtworkMode::Vinyl;
    ThemeMode m_themeMode = ThemeMode::Dark;
    int m_backgroundOpacity = 96;
    int m_cornerRadius = 17;
    int m_animationDurationMs = ExpandAnimationMilliseconds;
    int m_popupAutoHideSeconds = 4;
    QTimer m_hoverCollapseTimer;
    QTimer m_popupAutoHideTimer;
    QTimer m_positionTimer;
    QTimer m_seekDispatchTimer;
    QTimer m_previewResetTimer;
    QElapsedTimer m_positionClock;
    QElapsedTimer m_previewClock;
    QElapsedTimer m_positionOverrideClock;
    QVariantAnimation m_heightAnimation;
    QVariantAnimation m_accentAnimation;
    QVariantAnimation m_ambientAnimation;
    QVariantAnimation m_contentFadeAnimation;
    QVariantAnimation m_vinylResetAnimation;
    QTimer m_transitionArtworkFallbackTimer;
    PlayerState m_pendingPlayerState;
    double m_contentOpacity = 1.0;
    double m_vinylResetAngle = 0.0;
    std::function<void(const QString &)> m_controlCallback;
    std::function<void(qint64)> m_seekCallback;
    std::function<void()> m_playerSwitchCallback;
    LayerShellQt::Window *m_layerWindow = nullptr;
    qint64 m_pendingSeekUsec = 0;
    qint64 m_previewPositionUsec = 0;
    qint64 m_positionOverrideUsec = 0;
    double m_lastVinylAngle = 0.0;
    double m_ambientOpacity = 0.0;
    bool m_draggingVinyl = false;
    bool m_hoverExpansionEnabled = true;
    bool m_hideWhenCovered = true;
    bool m_previewPositionValid = false;
    bool m_positionOverrideValid = false;
    bool m_hasMultiplePlayers = false;
    bool m_pendingPlayerStateValid = false;
    bool m_fadeInAfterPlayerTransition = false;
    bool m_waitingForTransitionArtwork = false;
    bool m_vinylResetActive = false;
    bool m_hotCornerOnly = true;
    bool m_popupAreaBlocked = true;
    bool m_desktopForcedOpen = false;
    bool m_userPopupActive = false;
    bool m_hiddenPlayerTransitionPending = false;
    bool m_waitForPointerMovementBeforeExpand = false;
    QPoint m_hotCornerSummonPosition;
};


// A dedicated transparent LayerShellQt input surface keeps the top-left summon
// corner clickable even while the main card is hidden behind/over application UI.
class PopupHotCorner final : public QWidget
{
public:
    explicit PopupHotCorner(MediaOverlay *overlay)
        : QWidget(nullptr)
        , m_overlay(overlay)
    {
        setMinimumSize(HotCornerSize, HotCornerSize);
        setMaximumSize(HotCornerSize, HotCornerSize);
        resize(HotCornerSize, HotCornerSize);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setFocusPolicy(Qt::NoFocus);
        setMouseTracking(true);
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        setWindowTitle(QStringLiteral("MediaTable Hot Corner"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_overlay) {
            m_overlay->showCompactCard(true);
            event->accept();
            return;
        }
        event->ignore();
    }

private:
    MediaOverlay *m_overlay = nullptr;
};

} // namespace

// Receives popup-area occlusion updates from the KWin helper.
class PopupAreaBridge final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.thxs.KDEMediaFlyout.PopupArea")
public:
    explicit PopupAreaBridge(MediaOverlay *overlay, QObject *parent = nullptr)
        : QObject(parent)
        , m_overlay(overlay)
    {
    }

public slots:
    void setPopupAreaBlocked(bool blocked)
    {
        if (m_overlay) {
            m_overlay->setPopupAreaBlocked(blocked);
        }
    }

private:
    MediaOverlay *m_overlay = nullptr;
};

// Converts MPRIS D-Bus notifications into debounced, immediate refreshes.
// The regular poll remains as a fallback for players that omit signals.
class MprisSignalRelay final : public QObject
{
    Q_OBJECT
public:
    explicit MprisSignalRelay(const QDBusConnection &bus,
                              std::function<void()> callback,
                              QObject *parent = nullptr)
        : QObject(parent)
        , m_bus(bus)
        , m_callback(std::move(callback))
    {
        m_debounce.setSingleShot(true);
        m_debounce.setInterval(25);
        QObject::connect(&m_debounce, &QTimer::timeout, this, [this] {
            if (m_callback) {
                m_callback();
            }
        });

        const bool propertiesConnected = m_bus.connect(
            QString(), QString::fromLatin1(MprisPath),
            QString::fromLatin1(PropertiesInterface), QStringLiteral("PropertiesChanged"),
            QStringLiteral("sa{sv}as"), this,
            SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
        if (!propertiesConnected) {
            qWarning() << "Could not subscribe to MPRIS PropertiesChanged signals";
        }

        m_seekedDebounce.setSingleShot(true);
        m_seekedDebounce.setInterval(90);
        QObject::connect(&m_seekedDebounce, &QTimer::timeout, this, [this] {
            if (m_callback) {
                m_callback();
            }
        });

        const bool seekedConnected = m_bus.connect(
            QString(), QString::fromLatin1(MprisPath),
            QString::fromLatin1(PlayerInterface), QStringLiteral("Seeked"),
            QStringLiteral("x"), this,
            SLOT(onSeeked(qlonglong)));
        if (!seekedConnected) {
            qWarning() << "Could not subscribe to MPRIS Seeked signals";
        }

        const bool namesConnected = m_bus.connect(
            QStringLiteral("org.freedesktop.DBus"),
            QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"),
            QStringLiteral("sss"), this,
            SLOT(onNameOwnerChanged(QString,QString,QString)));
        if (!namesConnected) {
            qWarning() << "Could not subscribe to D-Bus NameOwnerChanged signals";
        }
    }

private slots:
    void onPropertiesChanged(QString interfaceName,
                             QVariantMap changedProperties,
                             QStringList invalidatedProperties)
    {
        const QString playerInterface = QString::fromLatin1(PlayerInterface);
        const QString rootInterface = QString::fromLatin1(RootInterface);
        if (interfaceName != playerInterface && interfaceName != rootInterface) {
            return;
        }

        static const QStringList importantProperties{
            QStringLiteral("PlaybackStatus"),
            QStringLiteral("Metadata"),
            QStringLiteral("CanGoPrevious"),
            QStringLiteral("CanGoNext"),
            QStringLiteral("CanPlay"),
            QStringLiteral("CanPause"),
            QStringLiteral("CanSeek"),
            QStringLiteral("Identity")
        };

        const bool changed = std::any_of(
            importantProperties.cbegin(), importantProperties.cend(),
            [&changedProperties, &invalidatedProperties](const QString &property) {
                return changedProperties.contains(property)
                    || invalidatedProperties.contains(property);
            });
        if (changed) {
            m_debounce.start();
        }
    }

    void onSeeked(qlonglong positionUsec)
    {
        // A seek close to zero often means Spotify's Previous action restarted
        // the current track without changing its metadata. Refresh immediately.
        // Regular scrubbing is debounced so repeated Seeked signals don't cause
        // a burst of full metadata/artwork polls while the user is dragging.
        if (positionUsec >= 0 && positionUsec <= 1200000LL) {
            m_debounce.start();
        } else {
            m_seekedDebounce.start();
        }
    }

    void onNameOwnerChanged(QString serviceName, QString oldOwner, QString newOwner)
    {
        Q_UNUSED(oldOwner);
        Q_UNUSED(newOwner);
        if (serviceName.startsWith(QStringLiteral("org.mpris.MediaPlayer2."))) {
            m_debounce.start();
        }
    }

private:
    QDBusConnection m_bus;
    std::function<void()> m_callback;
    QTimer m_debounce;
    QTimer m_seekedDebounce;
};

class MediaController final : public QObject
{
public:
    explicit MediaController(MediaOverlay *overlay, bool testMode)
        : m_overlay(overlay)
        , m_bus(QDBusConnection::sessionBus())
        , m_testMode(testMode)
    {
        m_overlay->setControlCallback([this](const QString &method) {
            sendPlayerMethod(method);
        });
        m_overlay->setSeekCallback([this](qint64 offsetUsec) {
            sendSeekOffset(offsetUsec);
        });
        m_overlay->setPlayerSwitchCallback([this] {
            cyclePlayer();
        });

        // MPRIS sends PropertiesChanged as soon as playback/track state changes.
        // Use it for UI responsiveness instead of waiting for the 850 ms fallback poll.
        m_signalRelay = new MprisSignalRelay(m_bus, [this] {
            poll();
        }, this);

        m_pollTimer.setInterval(850);
        QObject::connect(&m_pollTimer, &QTimer::timeout, this, [this] {
            poll();
        });

        // Some browser MPRIS integrations (notably Firefox/YouTube) do not
        // consistently emit PlaybackStatus notifications. Check every known
        // player's status asynchronously, not just the currently selected one,
        // so media keys and browser play/pause actions are detected globally.
        m_playbackStatusTimer.setInterval(140);
        m_playbackStatusTimer.setTimerType(Qt::CoarseTimer);
        QObject::connect(&m_playbackStatusTimer, &QTimer::timeout, this, [this] {
            checkAllPlaybackStatuses();
        });

        m_playbackChangeDebounce.setSingleShot(true);
        m_playbackChangeDebounce.setInterval(85);
        QObject::connect(&m_playbackChangeDebounce, &QTimer::timeout, this, [this] {
            commitPendingPlaybackChange();
        });

        // Commands sent from the overlay's own buttons can produce the same MPRIS
        // metadata/status signals as external media keys. Keep those echoes from
        // changing the card's presentation mode: clicking Next inside the persistent
        // desktop card must not turn it into a temporary compact popup.
        m_localControlEchoTimer.setSingleShot(true);
        m_localControlEchoTimer.setInterval(2200);
        QObject::connect(&m_localControlEchoTimer, &QTimer::timeout, this, [this] {
            m_localControlEchoService.clear();
            m_localControlTrackChangeExpected = false;
            m_localControlPlaybackChangeExpected = false;
        });

        if (m_testMode) {
            QTimer::singleShot(250, this, [this] {
                m_overlay->showTestCard();
            });
            QTimer::singleShot(9000, qApp, &QCoreApplication::quit);
        } else {
            m_pollTimer.start();
            m_playbackStatusTimer.start();
            QTimer::singleShot(250, this, [this] {
                poll();
            });
        }
    }

private:
    void checkAllPlaybackStatuses()
    {
        if (!m_bus.isConnected() || m_players.isEmpty()) {
            return;
        }

        // Only request the tiny PlaybackStatus property. The heavier metadata,
        // artwork, capabilities and position refresh still happens in poll().
        // A per-service pending set prevents overlapping requests to a slow player.
        const QList<PlayerState> playersSnapshot = m_players;
        for (const PlayerState &player : playersSnapshot) {
            if (m_pendingStatusChecks.contains(player.service)) {
                continue;
            }

            QDBusMessage message = QDBusMessage::createMethodCall(
                player.service, MprisPath, PropertiesInterface, "Get");
            message.setArguments(QList<QVariant>{
                QVariant(QString::fromLatin1(PlayerInterface)),
                QVariant(QStringLiteral("PlaybackStatus"))
            });

            const QString requestedService = player.service;
            m_pendingStatusChecks.insert(requestedService);
            auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 300), this);
            QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this,
                             [this, requestedService](QDBusPendingCallWatcher *finished) {
                m_pendingStatusChecks.remove(requestedService);
                if (!finished->isError()) {
                    const QList<QVariant> arguments = finished->reply().arguments();
                    if (!arguments.isEmpty()) {
                        const QString status =
                            unwrapDbusVariant(arguments.constFirst()).toString();
                        if (!status.isEmpty()) {
                            recordPlaybackStatus(requestedService, status);
                        }
                    }
                }
                finished->deleteLater();
            });
        }
    }

    void recordPlaybackStatus(const QString &service, const QString &status)
    {
        auto playerIt = std::find_if(
            m_players.begin(), m_players.end(), [&service](const PlayerState &player) {
                return player.service == service;
            });
        if (playerIt == m_players.end()) {
            return;
        }

        const auto knownStatus = m_observedPlaybackStatuses.constFind(service);
        if (knownStatus == m_observedPlaybackStatuses.cend()) {
            m_observedPlaybackStatuses.insert(service, status);
            playerIt->playbackStatus = status;
            return;
        }

        if (knownStatus.value() == status) {
            return;
        }

        m_observedPlaybackStatuses.insert(service, status);
        playerIt->playbackStatus = status;
        queuePlaybackStatusChange(*playerIt);
    }

    void observePlaybackStatusesFromPoll()
    {
        QSet<QString> presentServices;
        presentServices.reserve(m_players.size());

        for (PlayerState &player : m_players) {
            presentServices.insert(player.service);
            const auto knownStatus = m_observedPlaybackStatuses.constFind(player.service);
            if (knownStatus == m_observedPlaybackStatuses.cend()) {
                // A newly appearing player that is already playing is treated as
                // the active source. Initial startup is seeded quietly instead.
                m_observedPlaybackStatuses.insert(player.service, player.playbackStatus);
                if (m_havePlaybackStatusSnapshot && player.isPlaying()) {
                    queuePlaybackStatusChange(player);
                }
            } else if (knownStatus.value() != player.playbackStatus) {
                m_observedPlaybackStatuses.insert(player.service, player.playbackStatus);
                queuePlaybackStatusChange(player);
            }
        }

        // Drop stale service names. Firefox can restart under a new D-Bus owner;
        // the stable application identity is handled separately by poll().
        for (auto it = m_observedPlaybackStatuses.begin();
             it != m_observedPlaybackStatuses.end();) {
            if (!presentServices.contains(it.key())) {
                it = m_observedPlaybackStatuses.erase(it);
            } else {
                ++it;
            }
        }
        m_havePlaybackStatusSnapshot = true;
    }

    void queuePlaybackStatusChange(const PlayerState &player)
    {
        if (player.service.isEmpty()) {
            return;
        }

        m_pendingLastChangedService = player.service;
        if (player.isPlaying()) {
            // Prefer a newly-playing source over another player's automatic
            // pause event when both happen together (common when switching apps).
            m_pendingPlayingService = player.service;
        } else if (m_pendingPlayingService == player.service) {
            m_pendingPlayingService.clear();
        }

        m_playbackChangeDebounce.start();
    }

    void commitPendingPlaybackChange()
    {
        QString targetService = m_pendingPlayingService;
        if (targetService.isEmpty()) {
            targetService = m_pendingLastChangedService;
        }
        m_pendingPlayingService.clear();
        m_pendingLastChangedService.clear();

        if (targetService.isEmpty()) {
            return;
        }

        auto targetIt = std::find_if(
            m_players.cbegin(), m_players.cend(), [&targetService](const PlayerState &player) {
                return player.service == targetService;
            });
        if (targetIt == m_players.cend()) {
            return;
        }

        // A play/pause action on any media session makes that session the one
        // represented by the flyout. This is a local selection change only: it
        // never sends Play, Pause, Stop or any other command to either player.
        const PlayerState target = *targetIt;
        if (target.service != m_selectedService) {
            clearFirefoxFallback();
        }
        m_selectedService = target.service;
        m_selectedIdentity = target.identity;
        m_selectedPlayerMissingClock.invalidate();
        m_current = target;
        m_haveCurrentPlayer = true;
        m_overlay->setAvailablePlayerCount(m_players.size());
        m_overlay->setPlayerState(target);

        const bool isLocalPlaybackEcho = m_localControlEchoTimer.isActive()
            && m_localControlPlaybackChangeExpected
            && target.service == m_localControlEchoService;
        if (!isLocalPlaybackEcho) {
            m_overlay->showCompactCard();
        }
    }

    void clearFirefoxFallback()
    {
        m_firefoxSeekFallbackActive = false;
        m_firefoxFallbackService.clear();
        m_firefoxFallbackTitle.clear();
        m_firefoxFallbackArtist.clear();
        m_firefoxFallbackAlbum.clear();
    }

    void sendSeekOffset(qint64 offsetUsec)
    {
        if (m_current.service.isEmpty() || !m_current.canSeek || offsetUsec == 0) {
            return;
        }

        if (m_current.service.contains(QStringLiteral("firefox"), Qt::CaseInsensitive)) {
            // Firefox currently has an MPRIS regression where Position can remain
            // zero after a seek while playback continues. Remember that this track
            // has entered the fallback path so a transient CanSeek/length update
            // cannot disable the user's next scrub gesture.
            m_firefoxSeekFallbackActive = true;
            m_firefoxFallbackService = m_current.service;
            m_firefoxFallbackTitle = m_current.title;
            m_firefoxFallbackArtist = m_current.artist;
            m_firefoxFallbackAlbum = m_current.album;
        }

        QDBusMessage message = QDBusMessage::createMethodCall(
            m_current.service, MprisPath, PlayerInterface, "Seek");
        message.setArguments(QList<QVariant>{
            QVariant::fromValue(static_cast<qlonglong>(offsetUsec))
        });

        auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 800), this);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this,
                         [](QDBusPendingCallWatcher *finished) {
            if (finished->isError()) {
                qWarning().noquote() << "MPRIS seek failed:" << finished->error().message();
            }
            finished->deleteLater();
        });
    }

    void poll()
    {
        if (!m_bus.isConnected()) {
            return;
        }

        // findPlayers returns a stable, service-name-sorted list of MPRIS media
        // sessions. Paused and stopped-but-retained sessions remain available
        // when they still expose track metadata or media controls.
        m_players = findPlayers(m_bus);
        observePlaybackStatusesFromPoll();
        m_overlay->setAvailablePlayerCount(m_players.size());

        if (m_players.isEmpty()) {
            // Keep the last selected session through short MPRIS enumeration gaps.
            // Firefox may briefly return incomplete properties after a seek; clearing
            // the selection here can make the overlay silently jump to another player.
            if (m_haveCurrentPlayer && !m_selectedService.isEmpty()) {
                if (!m_selectedPlayerMissingClock.isValid()) {
                    m_selectedPlayerMissingClock.start();
                }
                if (m_selectedPlayerMissingClock.elapsed()
                    < SelectedPlayerMissingGraceMilliseconds) {
                    return;
                }
            }

            m_current = {};
            m_selectedService.clear();
            m_haveCurrentPlayer = false;
            m_selectedPlayerMissingClock.invalidate();
            clearFirefoxFallback();
            return;
        }

        auto selectedIt = std::find_if(
            m_players.cbegin(), m_players.cend(), [this](const PlayerState &player) {
                return !m_selectedService.isEmpty() && player.service == m_selectedService;
            });

        bool reboundService = false;
        if (selectedIt == m_players.cend() && !m_selectedIdentity.isEmpty()) {
            // Firefox registers a new org.mpris service instance after a browser
            // restart. Reattach to the same application identity rather than
            // keeping the dead instance name or silently selecting Spotify.
            selectedIt = std::find_if(
                m_players.cbegin(), m_players.cend(), [this](const PlayerState &player) {
                    return player.identity.compare(m_selectedIdentity,
                                                   Qt::CaseInsensitive) == 0;
                });
            reboundService = selectedIt != m_players.cend();
        }

        PlayerState selected;
        if (selectedIt != m_players.cend()) {
            selected = *selectedIt;
            m_selectedPlayerMissingClock.invalidate();

            if (selected.service != m_selectedService) {
                m_selectedService = selected.service;
                reboundService = true;
            }
            m_selectedIdentity = selected.identity;

            // A restarted Firefox process has a new MPRIS service and must start
            // with a fresh fallback snapshot. The new service is still selected
            // by the stable application identity above.
            if (reboundService) {
                clearFirefoxFallback();
            }

            const bool selectedHasMetadata = hasUsableTrackMetadata(selected);
            const bool fallbackHasMetadata =
                !m_firefoxFallbackTitle.isEmpty()
                && m_firefoxFallbackTitle != QStringLiteral("Now playing");
            const bool selectedArtistKnown = !selected.artist.isEmpty()
                && selected.artist != selected.identity;
            const bool fallbackArtistKnown = !m_firefoxFallbackArtist.isEmpty()
                && m_firefoxFallbackArtist != selected.identity;
            const bool artistMatchesFallback = !selectedArtistKnown || !fallbackArtistKnown
                || selected.artist == m_firefoxFallbackArtist;
            const bool metadataMatchesFallback = selectedHasMetadata
                && fallbackHasMetadata
                && selected.title == m_firefoxFallbackTitle
                && artistMatchesFallback;
            const bool metadataTemporarilyMissing = !selectedHasMetadata;
            const bool albumMatchesFallback = m_firefoxFallbackAlbum.isEmpty()
                || selected.album.isEmpty()
                || selected.album == m_firefoxFallbackAlbum;
            const bool fallbackTrackStillSelected = m_firefoxSeekFallbackActive
                && selected.service == m_firefoxFallbackService
                && selected.playbackStatus != QStringLiteral("Paused")
                && (metadataTemporarilyMissing
                    || (metadataMatchesFallback && albumMatchesFallback));

            if (m_firefoxSeekFallbackActive
                && (selected.service != m_firefoxFallbackService
                    || selected.playbackStatus == QStringLiteral("Paused")
                    || !fallbackTrackStillSelected)) {
                clearFirefoxFallback();
            }

            // Restore any Firefox properties that intermittently disappear after
            // seeking, not only when Position itself is zero. The fallback is
            // scoped to the same visible track and current Firefox service.
            if (fallbackTrackStillSelected) {
                if (selected.lengthUsec <= 0) {
                    selected.lengthUsec = m_current.lengthUsec;
                }
                if (!selected.canSeek && m_current.canSeek) {
                    selected.canSeek = true;
                }
                if (selected.trackId.isEmpty()) {
                    selected.trackId = m_current.trackId;
                }
                if (selected.album.isEmpty()) {
                    selected.album = m_firefoxFallbackAlbum;
                }
                if (metadataTemporarilyMissing) {
                    selected.title = m_firefoxFallbackTitle;
                    selected.artist = m_firefoxFallbackArtist;
                }
            }
        } else {
            // A briefly missing selected service is not proof that it closed. Keep
            // the last selected source for a short grace period before falling back.
            if (m_haveCurrentPlayer && !m_selectedService.isEmpty()) {
                if (!m_selectedPlayerMissingClock.isValid()) {
                    m_selectedPlayerMissingClock.start();
                }
                if (m_selectedPlayerMissingClock.elapsed()
                    < SelectedPlayerMissingGraceMilliseconds) {
                    return;
                }
            }

            // On startup (or if the selected app genuinely disappears), prefer a
            // playing session; otherwise use the first available paused session.
            const auto playingIt = std::find_if(
                m_players.cbegin(), m_players.cend(), [](const PlayerState &player) {
                    return player.isPlaying();
                });
            selected = playingIt != m_players.cend() ? *playingIt : m_players.constFirst();
            m_selectedService = selected.service;
            m_selectedIdentity = selected.identity;
            m_selectedPlayerMissingClock.invalidate();
            clearFirefoxFallback();
        }

        const bool hadCurrentPlayer = m_haveCurrentPlayer;
        const bool playerChanged = !m_haveCurrentPlayer
            || m_current.service != selected.service;
        const bool trackChanged = m_haveCurrentPlayer
            && m_current.service == selected.service
            && (m_current.title != selected.title
                || m_current.artist != selected.artist
                || m_current.album != selected.album);
        const bool playbackChanged = m_haveCurrentPlayer
            && m_current.service == selected.service
            && m_current.playbackStatus != selected.playbackStatus;

        m_current = selected;
        m_haveCurrentPlayer = true;
        m_overlay->setPlayerState(selected);

        // The first player snapshot is discovery, not a user media event: let the
        // unobstructed desktop decide whether to show its persistent expanded card.
        // Player and playback-state changes still summon the compact popup. A track
        // metadata change alone must not collapse the persistent desktop card: it
        // may simply be natural playback advancing to the next song. If the card is
        // already suppressed or a manual popup is active, retain the existing
        // track-change summon behavior.
        const bool localTrackChangeEcho = trackChanged
            && m_localControlEchoTimer.isActive()
            && m_localControlTrackChangeExpected
            && selected.service == m_localControlEchoService;
        const bool localPlaybackChangeEcho = playbackChanged
            && m_localControlEchoTimer.isActive()
            && m_localControlPlaybackChangeExpected
            && selected.service == m_localControlEchoService;

        if (!hadCurrentPlayer) {
            m_overlay->activateOnDesktop();
        } else if (playerChanged
                   || (playbackChanged && !localPlaybackChangeEcho)
                   || (trackChanged && !localTrackChangeEcho
                       && m_overlay->shouldTreatTrackChangeAsPopupEvent())) {
            m_overlay->showCompactCard();
        }
    }

    void cyclePlayer()
    {
        if (m_players.size() < 2) {
            return;
        }

        int currentIndex = -1;
        for (int i = 0; i < m_players.size(); ++i) {
            if (m_players.at(i).service == m_selectedService) {
                currentIndex = i;
                break;
            }
        }

        const int nextIndex = (currentIndex + 1) % m_players.size();
        const PlayerState nextPlayer = m_players.at(nextIndex);
        if (nextPlayer.service == m_selectedService) {
            return;
        }

        // This is intentionally only a local selection change. Do not send Play,
        // Pause, Stop, or any other command to either the old or new player.
        m_selectedService = nextPlayer.service;
        m_selectedIdentity = nextPlayer.identity;
        m_selectedPlayerMissingClock.invalidate();
        clearFirefoxFallback();
        m_current = nextPlayer;
        m_haveCurrentPlayer = true;
        m_overlay->setPlayerState(nextPlayer);
        // This is an in-card selection action, not an external media event.
        // Preserve the current desktop/popup presentation mode.
    }

    void sendPlayerMethod(const QString &method)
    {
        if (m_current.service.isEmpty()) {
            return;
        }

        const QString commandService = m_current.service;
        const bool expectsTrackChange = method == QStringLiteral("Next")
            || method == QStringLiteral("Previous");
        const bool expectsPlaybackChange = method == QStringLiteral("PlayPause")
            || method == QStringLiteral("Play")
            || method == QStringLiteral("Pause");
        if (expectsTrackChange || expectsPlaybackChange) {
            m_localControlEchoService = commandService;
            m_localControlTrackChangeExpected = expectsTrackChange;
            m_localControlPlaybackChangeExpected = expectsPlaybackChange;
            m_localControlEchoTimer.start();
        }

        QDBusMessage message = QDBusMessage::createMethodCall(
            commandService, MprisPath, PlayerInterface, method);
        // The user is already interacting with the visible card. Send the command
        // without summoning a new popup or changing its current display mode. The
        // matching MPRIS echo is also suppressed in poll()/commitPendingPlaybackChange().
        auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 800), this);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this,
                         [this](QDBusPendingCallWatcher *finished) {
            if (finished->isError()) {
                qWarning().noquote() << "MPRIS command failed:" << finished->error().message();
            }
            finished->deleteLater();
            QTimer::singleShot(25, this, [this] { poll(); });
        });
    }

    MediaOverlay *m_overlay = nullptr;
    MprisSignalRelay *m_signalRelay = nullptr;
    QDBusConnection m_bus;
    QTimer m_pollTimer;
    QTimer m_playbackStatusTimer;
    QTimer m_playbackChangeDebounce;
    QTimer m_localControlEchoTimer;
    QSet<QString> m_pendingStatusChecks;
    QHash<QString, QString> m_observedPlaybackStatuses;
    QString m_pendingPlayingService;
    QString m_pendingLastChangedService;
    QString m_localControlEchoService;
    bool m_localControlTrackChangeExpected = false;
    bool m_localControlPlaybackChangeExpected = false;
    bool m_havePlaybackStatusSnapshot = false;
    QList<PlayerState> m_players;
    PlayerState m_current;
    QString m_selectedService;
    QString m_selectedIdentity;
    QElapsedTimer m_selectedPlayerMissingClock;
    QString m_firefoxFallbackService;
    QString m_firefoxFallbackTitle;
    QString m_firefoxFallbackArtist;
    QString m_firefoxFallbackAlbum;
    bool m_firefoxSeekFallbackActive = false;
    bool m_testMode = false;
    bool m_haveCurrentPlayer = false;
};

int main(int argc, char *argv[])
{
    // Qt 6.5+ selects LayerShellQt through the Wayland shell integration automatically.

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("THXS"));
    QCoreApplication::setApplicationName(QStringLiteral("MediaTable"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.37"));

    if (QGuiApplication::platformName() != QStringLiteral("wayland")) {
        qCritical() << "MediaTable needs a Wayland session.";
        return 2;
    }

    const bool testMode = app.arguments().contains(QStringLiteral("--test-compact"))
        || app.arguments().contains(QStringLiteral("--test"));

    MediaOverlay overlay;
    overlay.setWindowTitle(QStringLiteral("MediaTable"));
    overlay.winId(); // Create the QWindow before attaching LayerShellQt settings.

    LayerShellQt::Window *layerWindow = LayerShellQt::Window::get(overlay.windowHandle());
    if (!layerWindow) {
        qCritical() << "Could not create a LayerShellQt window. Is layer-shell-qt installed?";
        return 3;
    }

    layerWindow->setLayer(LayerShellQt::Window::LayerTop);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow->setCloseOnDismissed(false);
    layerWindow->setExclusiveZone(0);
    layerWindow->setDesiredSize(QSize(HotCornerSize, HotCornerSize));

    LayerShellQt::Window::Anchors anchors;
    anchors |= LayerShellQt::Window::AnchorTop;
    anchors |= LayerShellQt::Window::AnchorLeft;
    layerWindow->setAnchors(anchors);
    layerWindow->setMargins(QMargins(0, 0, 0, 0));
    overlay.setLayerWindow(layerWindow);

    PopupHotCorner hotCorner(&overlay);
    hotCorner.winId(); // Create the independent transparent input surface.
    LayerShellQt::Window *hotCornerLayerWindow =
        LayerShellQt::Window::get(hotCorner.windowHandle());
    if (!hotCornerLayerWindow) {
        qCritical() << "Could not create the LayerShellQt hot-corner surface.";
        return 3;
    }
    hotCornerLayerWindow->setLayer(LayerShellQt::Window::LayerTop);
    hotCornerLayerWindow->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
    hotCornerLayerWindow->setCloseOnDismissed(false);
    hotCornerLayerWindow->setExclusiveZone(0);
    hotCornerLayerWindow->setDesiredSize(QSize(HotCornerSize, HotCornerSize));
    LayerShellQt::Window::Anchors hotCornerAnchors;
    hotCornerAnchors |= LayerShellQt::Window::AnchorTop;
    hotCornerAnchors |= LayerShellQt::Window::AnchorLeft;
    hotCornerLayerWindow->setAnchors(hotCornerAnchors);
    hotCornerLayerWindow->setMargins(QMargins(0, 0, 0, 0));
    hotCorner.show();
    hotCorner.raise();

    PopupAreaBridge popupAreaBridge(&overlay);
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    constexpr auto PopupAreaService = "org.thxs.KDEMediaFlyout.PopupArea";
    constexpr auto PopupAreaPath = "/org/thxs/KDEMediaFlyout/PopupArea";
    bool popupAreaBridgeRegistered = false;
    if (!testMode) {
        if (sessionBus.registerService(QString::fromLatin1(PopupAreaService))) {
            popupAreaBridgeRegistered = sessionBus.registerObject(
                QString::fromLatin1(PopupAreaPath), &popupAreaBridge,
                QDBusConnection::ExportAllSlots);
            if (!popupAreaBridgeRegistered) {
                sessionBus.unregisterService(QString::fromLatin1(PopupAreaService));
                qWarning() << "Could not export the popup-area D-Bus bridge.";
            }
        } else {
            qWarning() << "Could not register the popup-area D-Bus service;"
                          " is another overlay instance already running?";
        }
    }

    if (testMode) {
        overlay.setPopupAreaBlocked(false);
    } else if (!popupAreaBridgeRegistered) {
        qWarning() << "KWin popup-area updates are unavailable;"
                      " the popup will remain suppressed as a safety fallback.";
    } else {
        qInfo() << "Waiting for KWin to confirm that the popup area is clear.";
    }

    MediaController controller(&overlay, testMode);
    Q_UNUSED(controller);

    return app.exec();
}

#include "main.moc"
