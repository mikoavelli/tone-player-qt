/// @file main_window.cpp
/// @brief Main window: equalizer, transport and settings.
///
/// Implementation of the window declared in main_window.h.
#include "main_window.h"

#include "audio_engine.h"
#include "band_widgets.h"
#include "errors.h"
#include "exact_eq.h"
#include "i18n.h"
#include "models.h"
#include "presets.h"
#include "settings_dialog.h"
#include "settings_store.h"
#include "theme.h"
#include "text_format.h"

#include <algorithm>
#include <stdexcept>

#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileIconProvider>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QPolygonF>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

#include <array>
#include <cmath>
#include <utility>

namespace tp::ui {

using audio::OutputDevice;
using audio::ToneGenerator;
using audio::TonePlayer;
using tr_ns::tr;
using Qt::Literals::StringLiterals::operator""_s;

// -----------------------------------------------------------------------------
// Interface appearance
// -----------------------------------------------------------------------------

namespace {

/// @brief Builds the interface stylesheet with the theme colors substituted.
QString stylesheet()
{
    return QStringLiteral(R"(
QMainWindow, QDialog {
    background-color: %bg%;
}
QWidget {
    color: %text%;
}
QToolTip {
    background-color: %surface%;
    color: %text%;
    border: 1px solid %border%;
    padding: 4px 6px;
}
QLabel {
    background: transparent;
}
QLabel#toneLabel, QLabel#captionLabel, QLabel#subtitleLabel,
QLabel#statCaption, QLabel#scaleLabel, QLabel#statusLabel,
QLabel#bandValue, QLabel#bandFrequency, QLabel#sectionLabel,
QLabel#fieldLabel {
    color: %muted%;
}
QLabel#subtitleLabel, QLabel#statusLabel {
    font-size: 13px;
}
QLabel#captionLabel, QLabel#fieldLabel {
    font-size: 13px;
}
QLabel#sectionLabel {
    color: %text%;
    font-size: 16px;
    font-weight: 600;
    padding-top: 6px;
}
QLabel#bandValue {
    font-size: 12px;
}
QLabel#bandFrequency {
    font-size: 11px;
}
QFrame#card, QFrame#statCard {
    background-color: %surface%;
    border: 1px solid %border%;
    border-radius: 12px;
}
QFrame#divider {
    background-color: %border%;
}
QScrollArea {
    background: transparent;
    border: none;
}
BandStrip {
    background: transparent;
}
QScrollArea > QWidget > QWidget {
    background: transparent;
}
QPushButton {
    background-color: transparent;
    border: 1px solid %border%;
    border-radius: 12px;
    padding: 0 12px;
    font-size: 16px;
    font-weight: 500;
}
QPushButton:hover:enabled {
    background-color: %surface%;
    border-color: %border_hover%;
}
QPushButton:pressed:enabled {
    background-color: %track%;
}
QPushButton:focus {
    border-color: %accent%;
}
QPushButton:disabled {
    color: %disabled%;
}
QPushButton#navButton {
    background-color: %surface%;
    padding: 0;
}
QPushButton#navButton:hover:enabled {
    background-color: %surface_hover%;
}
QPushButton#navButton:pressed:enabled {
    background-color: %track%;
}
QPushButton#navButton:disabled {
    background-color: transparent;
}
QPushButton#bandPlay {
    background-color: transparent;
    border: none;
    border-radius: 14px;
    padding: 0;
}
QPushButton#bandPlay:hover:enabled {
    background-color: %track%;
}
QPushButton#playButton {
    background-color: %accent%;
    color: %on_accent%;
    border: 1px solid %accent%;
    font-size: 17px;
    font-weight: 600;
}
QPushButton#playButton:hover:enabled {
    background-color: %accent_hover%;
    border-color: %accent_hover%;
}
QPushButton#playButton:pressed:enabled {
    background-color: %accent%;
}
QPushButton#playButton:focus {
    border: 2px solid %text%;
}
QPushButton#playButton:disabled {
    background-color: %surface%;
    color: %disabled%;
    border: 1px solid %border%;
}
QProgressBar {
    border: none;
    border-radius: 3px;
    background-color: %track%;
}
QProgressBar::chunk {
    background-color: %accent%;
    border-radius: 3px;
}
QSlider::groove:vertical {
    width: 6px;
    background-color: %track%;
    border-radius: 3px;
}
QSlider::handle:vertical {
    background-color: %accent%;
    height: 16px;
    margin: 0 -6px;
    border-radius: 8px;
}
QSlider::handle:vertical:hover {
    background-color: %accent_hover%;
}
QComboBox {
    background-color: %surface%;
    border: 1px solid %border%;
    border-radius: 10px;
    padding: 0 12px;
    font-size: 14px;
}
QComboBox:hover:enabled {
    border-color: %border_hover%;
}
QComboBox:focus {
    border-color: %accent%;
}
QComboBox:disabled {
    color: %disabled%;
}
QComboBox::drop-down {
    border: none;
    width: 30px;
}
QComboBox::down-arrow {
    image: none;
}
QComboBox QAbstractItemView {
    background-color: %surface%;
    border: 1px solid %border%;
    selection-background-color: %border%;
    selection-color: %text%;
    outline: 0;
    padding: 4px;
}
QDoubleSpinBox, QSpinBox {
    background-color: %surface%;
    border: 1px solid %border%;
    border-radius: 8px;
    padding: 5px 8px;
    min-height: 24px;
}
QDoubleSpinBox:focus, QSpinBox:focus {
    border-color: %accent%;
}
/* Qt 6.12: NoButtons + styled frame collapses the internal editor to 1px,
   so the buttons are hidden by the stylesheet instead of setButtonSymbols. */
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button,
QSpinBox::up-button, QSpinBox::down-button {
    width: 0px;
    height: 0px;
    margin: 0px;
    padding: 0px;
    border: none;
    background: transparent;
}
QPlainTextEdit {
    background-color: %surface%;
    border: 1px solid %border%;
    border-radius: 8px;
    padding: 6px;
}
QLineEdit {
    background-color: %surface%;
    color: %text%;
    border: 1px solid %border%;
    border-radius: 8px;
    padding: 7px 10px;
    min-height: 22px;
    selection-background-color: %accent%;
    selection-color: %on_accent%;
}
QLineEdit:focus {
    border-color: %accent%;
}
QScrollBar:horizontal {
    background: transparent;
    height: 8px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: %border_hover%;
    border-radius: 4px;
    min-width: 40px;
}
QScrollBar::handle:horizontal:hover {
    background: %muted%;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: transparent;
}
)")
        .replace(u"%bg%"_s, theme::bgColor)
        .replace(u"%surface%"_s, theme::surfaceColor)
        .replace(u"%surface_hover%"_s, theme::surfaceHoverColor)
        .replace(u"%border%"_s, theme::borderColor)
        .replace(u"%border_hover%"_s, theme::borderHoverColor)
        .replace(u"%text%"_s, theme::textColor)
        .replace(u"%muted%"_s, theme::mutedColor)
        .replace(u"%disabled%"_s, theme::disabledColor)
        .replace(u"%track%"_s, theme::trackColor)
        .replace(u"%accent%"_s, theme::accentColor)
        .replace(u"%accent_hover%"_s, theme::accentHoverColor)
        .replace(u"%on_accent%"_s, theme::onAccentColor)
        .replace(u"%icon%"_s, theme::iconColor)
        .replace(u"%icon_disabled%"_s, theme::iconDisabledColor);
}

/// @brief Returns the translated label of the main button for a mode.
/// @param mode Current button mode.
QString playButtonLabel(PlayButtonMode mode)
{
    switch (mode) {
    case PlayButtonMode::Pause:
        return tr_ns::tr("play.pause");
    case PlayButtonMode::Resume:
        return tr_ns::tr("play.resume");
    case PlayButtonMode::Start:
        break;
    }
    return tr_ns::tr("play.start");
}

/// @brief Returns the icon name of the main button for a mode.
/// @param mode Current button mode.
QString playButtonIcon(PlayButtonMode mode)
{
    switch (mode) {
    case PlayButtonMode::Pause:
        return u"pause"_s;
    case PlayButtonMode::Resume:
    case PlayButtonMode::Start:
        break;
    }
    return u"start"_s;
}

/// @brief Returns the translated autoplay state text.
/// @param enabled Autoplay state.
QString autoplayText(bool enabled)
{
    return tr_ns::tr(enabled ? "header.autoplay.on" : "header.autoplay.off");
}

/// @brief Returns the first installed font family, otherwise the fallback.
/// @param candidates Preferred families in order.
/// @param fallback Family used when none of the candidates is installed.
QString firstFamily(const std::array<std::string_view, 5> &candidates, const QString &fallback)
{
    QSet<QString> available;
    for (const QString &family : QFontDatabase::families()) {
        available.insert(family);
    }
    for (const std::string_view &candidate : candidates) {
        const QString name = QString::fromUtf8(candidate);
        if (available.contains(name)) {
            return name;
        }
    }
    return fallback;
}

// ---- Icons -------------------------------------------------------------------

/// @brief Pixels per logical point of the pre-rendered glyph pixmaps.
constexpr int iconRatio = 2;

/// @brief Draws a themed vector glyph into a pixmap.
/// @param kind Glyph name: settings, presetSave, delete, start, pause or stop.
/// @param size Logical size of the pixmap, px.
/// @param color Stroke and fill color.
/// @return Pixmap with the glyph, scaled for high-DPI screens.
QPixmap drawGlyph(const QString &kind, int size, const QColor &color)
{
    // Icon kind in a square size × size (24 × 24 grid).
    QPixmap pixmap(size * iconRatio, size * iconRatio);
    pixmap.setDevicePixelRatio(iconRatio);
    pixmap.fill(Qt::GlobalColor::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing);
    painter.scale(size / 24.0, size / 24.0);

    const QPen pen(color, 2.0, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap,
        Qt::PenJoinStyle::RoundJoin);
    if (kind == u"settings"_s) {
        painter.setPen(pen);
        painter.setBrush(Qt::BrushStyle::NoBrush);
        painter.drawEllipse(QPointF(12.0, 12.0), 4.0, 4.0);
        for (int angle = 0; angle < 360; angle += 45) {
            const double radians = qDegreesToRadians(angle);
            painter.drawLine(QPointF(12.0 + 6.5 * std::cos(radians),
                12.0 + 6.5 * std::sin(radians)),
                QPointF(12.0 + 9.5 * std::cos(radians),
                    12.0 + 9.5 * std::sin(radians)));
        }
    } else if (kind == u"presetSave"_s) {
        painter.setPen(QPen(color, 1.9, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap,
            Qt::PenJoinStyle::RoundJoin));
        painter.setBrush(Qt::BrushStyle::NoBrush);
        painter.drawPolygon(QPolygonF({
            QPointF(7.0, 4.0),
            QPointF(17.0, 4.0),
            QPointF(17.0, 20.0),
            QPointF(12.0, 15.5),
            QPointF(7.0, 20.0),
        }));
    } else if (kind == u"delete"_s) {
        painter.setPen(QPen(color, 1.8, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap,
            Qt::PenJoinStyle::RoundJoin));
        painter.setBrush(Qt::BrushStyle::NoBrush);
        painter.drawLine(QPointF(5.0, 7.0), QPointF(19.0, 7.0));
        painter.drawLine(QPointF(9.0, 4.0), QPointF(15.0, 4.0));
        painter.drawRoundedRect(QRectF(7.0, 7.0, 10.0, 13.0), 2.0, 2.0);
        painter.drawLine(QPointF(10.0, 10.0), QPointF(10.0, 17.0));
        painter.drawLine(QPointF(14.0, 10.0), QPointF(14.0, 17.0));
    } else {
        const QString glyph =
            kind == u"bandStart"_s ? u"start"_s : kind == u"bandPause"_s ? u"pause"_s : kind;
        painter.setBrush(color);
        if (glyph == u"start"_s) {
            painter.setPen(QPen(color, 1.5, Qt::PenStyle::SolidLine,
                Qt::PenCapStyle::RoundCap, Qt::PenJoinStyle::RoundJoin));
            painter.drawPolygon(QPolygonF({
                QPointF(7.5, 5.0),
                QPointF(7.5, 19.0),
                QPointF(19.5, 12.0),
            }));
        } else {
            painter.setPen(Qt::PenStyle::NoPen);
            if (glyph == u"pause"_s) {
                painter.drawRoundedRect(QRectF(6.0, 4.0, 4.0, 16.0), 1.0, 1.0);
                painter.drawRoundedRect(QRectF(14.0, 4.0, 4.0, 16.0), 1.0, 1.0);
            } else if (glyph == u"stop"_s) {
                painter.drawRoundedRect(QRectF(5.0, 5.0, 14.0, 14.0), 2.0, 2.0);
            }
        }
    }
    painter.end();
    return pixmap;
}

/// @brief Creates an icon with normal and disabled pixmaps.
/// @param kind Glyph name.
/// @param size Logical icon size, px.
/// @param normal Color in the normal state.
/// @param disabled Color in the disabled state.
QIcon makeIcon(const QString &kind, int size, const QString &normal, const QString &disabled)
{
    QIcon icon;
    icon.addPixmap(drawGlyph(kind, size, QColor(normal)), QIcon::Mode::Normal);
    icon.addPixmap(drawGlyph(kind, size, QColor(disabled)), QIcon::Mode::Disabled);
    return icon;
}

/// @brief Builds the complete icon set used by the window.
QHash<QString, QIcon> makeIcons()
{
    using namespace theme;
    const auto disabledColor = QColor(iconDisabledColor).name();
    const auto normalIconColor = QColor(iconColor).name();
    const auto captionColor = QColor(onAccentColor).name();
    const auto textDisabledColor = QColor(theme::disabledColor).name();
    const auto textColor = QColor(theme::textColor).name();
    QHash<QString, QIcon> icons;
    icons.insert(u"start"_s, makeIcon(u"start"_s, 18, captionColor, textDisabledColor));
    icons.insert(u"pause"_s, makeIcon(u"pause"_s, 18, captionColor, textDisabledColor));
    icons.insert(u"stop"_s, makeIcon(u"stop"_s, 18, textColor, textDisabledColor));
    icons.insert(u"settings"_s, makeIcon(u"settings"_s, 20, normalIconColor, disabledColor));
    icons.insert(u"presetSave"_s,
        makeIcon(u"presetSave"_s, 18, normalIconColor, disabledColor));
    icons.insert(u"delete"_s, makeIcon(u"delete"_s, 18, normalIconColor, disabledColor));
    icons.insert(u"bandStart"_s, makeIcon(u"bandStart"_s, 14, normalIconColor, disabledColor));
    icons.insert(u"bandPause"_s, makeIcon(u"bandPause"_s, 14, normalIconColor, disabledColor));
    return icons;
}

/// @brief Combo box with a chevron drawn in the theme color.
class ChevronCombo final : public QComboBox {
public:
    using QComboBox::QComboBox;

protected:
    /// @brief Draws the chevron on top of the style-drawn combo box.
    void paintEvent(QPaintEvent *event) override
    {
        QComboBox::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::RenderHint::Antialiasing);
        painter.setPen(QPen(isEnabled() ? QColor(theme::mutedColor)
                                        : QColor(theme::iconDisabledColor),
            1.8, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap,
            Qt::PenJoinStyle::RoundJoin));
        const QPointF center(width() - 20.0, height() / 2.0);
        painter.drawPolyline(QVector<QPointF>{
            {center.x() - 4, center.y() - 2},
            {center.x(), center.y() + 2},
            {center.x() + 4, center.y() - 2},
        });
        painter.end();
    }
};

/// @brief Scroll area of the band strip: the wheel moves it horizontally.
class BandScrollArea final : public QScrollArea {
public:
    /// @brief Creates the scroll area without frames or scrollbars.
    /// @param parent Parent widget.
    explicit BandScrollArea(QWidget *parent = nullptr)
        : QScrollArea(parent)
    {
        setWidgetResizable(true);
        setFrameShape(QFrame::Shape::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        setFocusPolicy(Qt::FocusPolicy::NoFocus);
    }

protected:
    /// @brief Translates a vertical wheel gesture into horizontal scrolling.
    void wheelEvent(QWheelEvent *event) override
    {
        if (auto *bar = horizontalScrollBar()) {
            bar->setValue(bar->value() - wheelScrollDelta(*event));
        }
        event->accept();
    }
};

} // namespace

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

MainWindow::MainWindow(const Settings &settings, const std::filesystem::path &settingsPath,
    TonePlayer *player, QWidget *parent)
    : QMainWindow(parent)
    , settings_(settings)
    , settingsPath_(settingsPath)
{
    // Level initialization: the default preset is strictly 0.0 dB.
    // If a user preset is active, its saved levels are loaded.
    {
        for (Band &band : settings_.bands) {
            band.levelDb = 0.0;
        }
    }
    if (settings.activePreset.has_value()) {
        const auto useActive = presets::findPreset(settings, settings.activePreset.value());
        if (useActive.has_value()) {
            const QVector<double> savedLevels =
                presets::alignedLevels(useActive->levels, static_cast<int>(settings.bands.size()));
            for (int index = 0; index < settings_.bands.size(); ++index) {
                settings_.bands[index].levelDb = savedLevels.at(index);
            }
        } else {
            settings_.activePreset = std::nullopt;
        }
    }

    focusedIndex_ = 0;
    settingsFileBytes_ = readSettingsFile();
    config_ = ToneConfig::fromSettings(settings_);

    if (player != nullptr) {
        player_ = player;
    } else {
        ownedPlayer_ = std::make_unique<TonePlayer>(config_, settings_.levels(),
            tp::masterBaseDb + settings_.masterGainDb);
        player_ = ownedPlayer_.get();
    }
    player_->setAutoplay(settings_.autoplay);

    devices_ = audio::listOutputDevices();
    setWindowTitle(tr_ns::tr("window.title"));
    resize(1040, 720);
    setMinimumSize(760, 560);

    sansFamily_ = firstFamily(sansFamilies,
        QFontDatabase::systemFont(QFontDatabase::GeneralFont).family());
    monoFamily_ = firstFamily(monoFamilies,
        QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
    QFont baseFont(sansFamily_);
    baseFont.setPixelSize(baseFontPx);
    setFont(baseFont);
    setStyleSheet(stylesheet());

    icons_ = makeIcons();
    buildUi();
    refreshPresets();

    spaceShortcut_ = new QShortcut(QKeySequence(Qt::Key_Space), this);
    spaceShortcut_->setContext(Qt::ShortcutContext::WindowShortcut);
    spaceShortcut_->setAutoRepeat(false);
    connect(spaceShortcut_, &QShortcut::activated, this, &MainWindow::onPlayPause);

    settingsShortcut_ = new QShortcut(QKeySequence(u"Ctrl+,"_s), this);
    settingsShortcut_->setContext(Qt::ShortcutContext::WindowShortcut);
    connect(settingsShortcut_, &QShortcut::activated, this, &MainWindow::openSettings);

    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    setFocus();

    timer_.setInterval(refreshIntervalMs);
    connect(&timer_, &QTimer::timeout, this, &MainWindow::refresh);
    timer_.start();

    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(saveDelayMs);
    connect(&saveTimer_, &QTimer::timeout, this, &MainWindow::saveSettings);

    noticeTimer_.setSingleShot(true);
    noticeTimer_.setInterval(noticeTimeoutMs);
    connect(&noticeTimer_, &QTimer::timeout, this, &MainWindow::clearNotice);

    settingsWatcher_ = new QFileSystemWatcher(this);
    settingsWatcher_->addPath(QString::fromStdString(settingsPath_.string()));
    connect(settingsWatcher_, &QFileSystemWatcher::fileChanged, this,
        &MainWindow::onSettingsFileChanged);
    reloadTimer_.setSingleShot(true);
    reloadTimer_.setInterval(settingsReloadDelayMs);
    connect(&reloadTimer_, &QTimer::timeout, this, &MainWindow::reloadSettings);

    refresh();
}

MainWindow::~MainWindow() = default;

// -----------------------------------------------------------------------------
// Building the interface
// -----------------------------------------------------------------------------

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    layout->addLayout(buildHeader());
    layout->addLayout(buildPlayback());
    layout->addLayout(buildBands(), 1);

    statusLabel_ = new QLabel(central);
    statusLabel_->setObjectName(u"statusLabel"_s);
    statusLabel_->setAlignment(Qt::AlignmentFlag::AlignCenter);
    layout->addWidget(statusLabel_);
}

QFont MainWindow::monoFont(int pixelSize, QFont::Weight weight) const
{
    QFont font(monoFamily_);
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

QPushButton *MainWindow::iconButton(const QString &kind, const QString &tooltip)
{
    auto *button = new QPushButton(this);
    button->setObjectName(u"navButton"_s);
    button->setFixedSize(38, 38);
    button->setIcon(icons_[kind]);
    button->setIconSize(QSize(19, 19));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    button->setCursor(Qt::CursorShape::PointingHandCursor);
    return button;
}

QLayout *MainWindow::buildHeader()
{
    auto *row = new QHBoxLayout();
    row->setSpacing(8);

    settingsButton_ = iconButton(u"settings"_s, tr_ns::tr("header.settings.tooltip"));
    connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::openSettings);
    row->addWidget(settingsButton_);

    presetSelector_ = new ChevronCombo(this);
    presetSelector_->setFixedHeight(38);
    presetSelector_->setMinimumWidth(150);
    presetSelector_->setMaximumWidth(210);
    presetSelector_->setToolTip(tr_ns::tr("header.preset.tooltip"));
    connect(presetSelector_, &QComboBox::currentIndexChanged, this,
        &MainWindow::onPresetChanged);
    row->addWidget(presetSelector_);

    presetSaveButton_ = iconButton(u"presetSave"_s, tr_ns::tr("header.presetSave.tooltip"));
    connect(presetSaveButton_, &QPushButton::clicked, this, &MainWindow::onSavePreset);
    row->addWidget(presetSaveButton_);

    presetDeleteButton_ = iconButton(u"delete"_s, tr_ns::tr("header.presetDelete.tooltip"));
    connect(presetDeleteButton_, &QPushButton::clicked, this, &MainWindow::onDeletePreset);
    row->addWidget(presetDeleteButton_);

    row->addStretch(1);

    devices_ = audio::listOutputDevices();
    auto *deviceLabel = new QLabel(tr_ns::tr("header.device.label"), this);
    deviceLabel->setObjectName(u"fieldLabel"_s);
    row->addWidget(deviceLabel);

    deviceSelector_ = new ChevronCombo(this);
    deviceSelector_->setFixedHeight(38);
    deviceSelector_->setMinimumWidth(150);
    deviceSelector_->setMaximumWidth(270);
    deviceSelector_->setToolTip(tr_ns::tr("header.device.tooltip"));
    deviceSelector_->addItem(tr_ns::tr("audio.defaultDevice"));
    for (const OutputDevice &device : devices_) {
        deviceSelector_->addItem(device.label());
    }
    const auto configured = audio::findDevice(devices_, settings_.device);
    if (configured.has_value()) {
        deviceSelector_->setCurrentIndex(deviceSelector_->findText(configured->label()));
    } else {
        deviceSelector_->setCurrentIndex(0);
    }
    if (!configured.has_value() && settings_.device.has_value()) {
        showNotice(tr_ns::tr("audio.deviceNotFound", settings_.device.value()));
    }
    player_->setDevice(configured.has_value() ? std::optional{configured->label()}
                                              : std::nullopt);
    connect(deviceSelector_, &QComboBox::currentIndexChanged, this,
        &MainWindow::onDeviceChanged);
    row->addWidget(deviceSelector_);

    autoplayStateLabel_ = new QLabel(tr_ns::tr("header.autoplay.label"), this);
    autoplayStateLabel_->setObjectName(u"captionLabel"_s);
    row->addWidget(autoplayStateLabel_);

    auto *autoplayButton = new ToggleSwitch(this);
    autoplayButton->setChecked(settings_.autoplay);
    autoplayButton->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    connect(autoplayButton, &ToggleSwitch::toggled, this, &MainWindow::onAutoplayToggled);
    row->addWidget(autoplayButton);
    autoplayButton_ = autoplayButton;
    updateAutoplayText(settings_.autoplay);
    return row;
}

QLayout *MainWindow::buildPlayback()
{
    auto *row = new QHBoxLayout();
    row->setSpacing(10);

    playButton_ = new QPushButton(this);
    playButton_->setObjectName(u"playButton"_s);
    playButton_->setIcon(icons_[u"start"_s]);
    playButton_->setIconSize(QSize(18, 18));
    playButton_->setFixedSize(38, 38);
    playButton_->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    playButton_->setCursor(Qt::CursorShape::PointingHandCursor);
    playButton_->setToolTip(tr_ns::tr("play.tooltip", tr_ns::tr("play.start")));
    playButton_->setAccessibleName(tr_ns::tr("play.start"));
    connect(playButton_, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    row->addWidget(playButton_);

    stopButton_ = iconButton(u"stop"_s, tr_ns::tr("play.stop"));
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStop);
    row->addWidget(stopButton_);

    auto *frequencyBox = new QVBoxLayout();
    frequencyBox->setSpacing(0);
    frequencyLabel_ = new QLabel(this);
    frequencyLabel_->setTextFormat(Qt::TextFormat::RichText);
    frequencyLabel_->setFixedWidth(frequencyBlockWidth);
    frequencyLabel_->setAlignment(Qt::AlignmentFlag::AlignLeft
        | Qt::AlignmentFlag::AlignVCenter);
    frequencyBox->addWidget(frequencyLabel_);

    toneCounterLabel_ = new QLabel(this);
    toneCounterLabel_->setObjectName(u"captionLabel"_s);
    toneCounterLabel_->setFixedWidth(frequencyBlockWidth);
    toneCounterLabel_->setAlignment(Qt::AlignmentFlag::AlignLeft);
    frequencyBox->addWidget(toneCounterLabel_);
    row->addLayout(frequencyBox);

    row->addSpacing(8);
    toneProgress_ = new QProgressBar(this);
    toneProgress_->setRange(0, 100);
    toneProgress_->setTextVisible(false);
    toneProgress_->setFixedHeight(8);
    toneProgress_->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
    row->addWidget(toneProgress_, 1);

    remainingLabel_ = new QLabel(this);
    remainingLabel_->setFont(monoFont(14, QFont::Weight::DemiBold));
    remainingLabel_->setAlignment(Qt::AlignmentFlag::AlignRight
        | Qt::AlignmentFlag::AlignVCenter);
    remainingLabel_->setMinimumWidth(116);
    row->addWidget(remainingLabel_);
    return row;
}

QString MainWindow::frequencyHtml(const QString &value) const
{
    // Frequency: the number in an accent mono font and a muted unit.
    const auto unit = tr_ns::tr("unit.hz").trimmed();
    return QStringLiteral(
        "<span style=\"font-family:'%1'; font-size:%2px; font-weight:600; color:%3;\">%4</span>"
        "<span style=\"font-family:'%5'; font-size:%6px; font-weight:500; color:%7;\">&nbsp;%8"
        "</span>")
        .arg(monoFamily_, QString::number(frequencyValuePx), theme::accentColor, value,
            sansFamily_, QString::number(frequencyUnitPx), theme::mutedColor, unit);
}

QLayout *MainWindow::buildBands()
{
    auto *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(4);

    grid->addWidget(buildMaster(), 0, 0);

    auto *divider = new QFrame(this);
    divider->setObjectName(u"divider"_s);
    divider->setFixedWidth(1);
    grid->addWidget(divider, 0, 1, 2, 1);

    bandScroll_ = new BandScrollArea(this);
    grid->addWidget(bandScroll_, 0, 2);

    bandBar_ = new QScrollBar(Qt::Orientation::Horizontal, this);
    bandBar_->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    if (auto *inner = bandScroll_->horizontalScrollBar()) {
        connect(inner, &QScrollBar::rangeChanged, this, [this](int minval, int maxval) {
            bandBar_->setRange(minval, maxval);
        });
        connect(inner, &QScrollBar::valueChanged, bandBar_, &QScrollBar::setValue);
        connect(bandBar_, &QScrollBar::valueChanged, inner, &QScrollBar::setValue);
    }
    grid->addWidget(bandBar_, 1, 2);

    grid->setRowStretch(0, 1);
    grid->setColumnStretch(2, 1);
    rebuildBands();
    syncBandBar();
    return grid;
}

void MainWindow::syncBandBar()
{
    if (const auto *inner = bandScroll_->horizontalScrollBar()) {
        bandBar_->setRange(inner->minimum(), inner->maximum());
        bandBar_->setPageStep(inner->pageStep());
        bandBar_->setSingleStep(inner->singleStep());
        bandBar_->setValue(inner->value());
    }
}

QWidget *MainWindow::buildMaster()
{
    auto *box = new QWidget(this);
    box->setObjectName(u"masterBox"_s);
    auto *row = new QHBoxLayout(box);
    row->setContentsMargins(4, 0, 0, 0);
    row->setSpacing(rulerSpacing);

    masterRuler_ = new LevelRuler(masterGainScale, box);
    row->addWidget(masterRuler_);

    masterFrame_ = new MasterFrame(box);
    auto *layout = new QVBoxLayout(masterFrame_);
    layout->setContentsMargins(columnMargin, columnMargin,
        columnMargin, columnMargin);
    layout->setSpacing(columnSpacing);

    masterValueLabel_ = new QLabel(masterFrame_);
    masterValueLabel_->setObjectName(u"bandValue"_s);
    masterValueLabel_->setAlignment(Qt::AlignmentFlag::AlignCenter);
    masterValueLabel_->setFixedHeight(columnValueHeight);
    layout->addWidget(masterValueLabel_);

    masterSlider_ = new LevelSlider(masterGainScale, masterFrame_);
    masterSlider_->setAccessibleName(tr_ns::tr("playback.masterAccessible"));
    masterSlider_->setToolTip(tr_ns::tr("playback.masterTooltip"));
    connect(masterSlider_, &LevelSlider::valueChanged, this, &MainWindow::onMasterChanged);
    connect(masterSlider_, &LevelSlider::navigateRequested, this, &MainWindow::navigate);
    layout->addWidget(masterSlider_, 1);
    masterRuler_->setSlider(masterSlider_);

    auto *caption = new QLabel(tr_ns::tr("playback.masterCaption"), masterFrame_);
    caption->setObjectName(u"bandFrequency"_s);
    caption->setAlignment(Qt::AlignmentFlag::AlignCenter);
    caption->setFixedHeight(columnFrequencyHeight);
    caption->setToolTip(tr_ns::tr("playback.masterAccessible"));
    layout->addWidget(caption);

    auto *placeholder = new QWidget(masterFrame_);
    placeholder->setFixedHeight(columnPlaySize);
    layout->addWidget(placeholder);

    row->addWidget(masterFrame_);

    syncMaster();
    return box;
}

void MainWindow::rebuildBands()
{
    std::optional<int> focusIndex;
    if (QWidget *focus = QApplication::focusWidget(); focus != nullptr && bands_ != nullptr) {
        for (int index = 0; index < settings_.bands.size(); ++index) {
            if (focus == &bands_->column(index).slider()) {
                focusIndex = index;
                break;
            }
        }
    }
    if (bands_ != nullptr) {
        bands_->deleteLater();
    }
    auto *strip = new BandStrip(settings_.levelScale, icons_,
        [this](int index, double levelDb) { onBandLevel(index, levelDb); },
        [this](int index) { onBandPlay(index); },
        [this](int index) { onBandSelect(index); },
        [this](int index) { onActivateBand(index); },
        [this](int offset) { navigate(offset); },
        [this](int delta) { scrollBands(delta); }, this);
    strip->setBands(settings_.bands, settings_.levelScale);
    bands_ = strip;
    bandScroll_->setWidget(strip);
    lastBandIndex_ = -1;
    const int index =
        focusIndex.has_value()
            ? std::min(focusIndex.value(), static_cast<int>(settings_.bands.size()) - 1)
            : std::min(focusedIndex_, static_cast<int>(settings_.bands.size()) - 1);
    focusedIndex_ = index;
    bands_->column(index).slider().setFocus();
}

// -----------------------------------------------------------------------------
// Handlers
// -----------------------------------------------------------------------------

int MainWindow::focusedBandIndex() const
{
    if (bands_ != nullptr) {
        if (const QWidget *focus = QApplication::focusWidget()) {
            for (int index = 0; index < settings_.bands.size(); ++index) {
                if (focus == &bands_->column(index).slider()) {
                    focusedIndex_ = index;
                    return index;
                }
            }
        }
    }
    return std::clamp(focusedIndex_, 0, static_cast<int>(settings_.bands.size()) - 1);
}

bool MainWindow::bandSliderFocused() const
{
    if (bands_ != nullptr) {
        if (const QWidget *focus = QApplication::focusWidget()) {
            for (int index = 0; index < settings_.bands.size(); ++index) {
                if (focus == &bands_->column(index).slider()) {
                    return true;
                }
            }
        }
    }
    return false;
}

void MainWindow::onPlayPause()
{
    // Main button: start, pause or resume — by the player state.
    const PlayerState state = player_->state();
    if (state == PlayerState::Playing) {
        player_->pause();
        refresh();
    } else if (state == PlayerState::Paused) {
        player_->resume();
        refresh();
    } else {
        const int index =
            state == PlayerState::Finished ? 0 : focusedBandIndex();
        startPlayback(index);
    }
}

void MainWindow::onAutoplayToggled(const bool enabled)
{
    player_->setAutoplay(enabled);
    updateAutoplayText(enabled);
    Settings updated = settings_;
    updated.autoplay = enabled;
    store(updated);
}

void MainWindow::updateAutoplayText(const bool enabled)
{
    autoplayButton_->setAccessibleName(autoplayText(enabled));
    autoplayButton_->setToolTip(
        enabled ? tr_ns::tr("header.autoplay.tooltip.on") : tr_ns::tr("header.autoplay.tooltip.off"));
    autoplayStateLabel_->setText(tr_ns::tr("header.autoplay.label"));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::KeyboardModifier::NoModifier) {
        switch (event->key()) {
        case Qt::Key_Left:
            navigate(-1);
            event->accept();
            return;
        case Qt::Key_Right:
            navigate(1);
            event->accept();
            return;
        case Qt::Key_Up:
            nudgeActiveBand(1);
            event->accept();
            return;
        case Qt::Key_Down:
            nudgeActiveBand(-1);
            event->accept();
            return;
        case Qt::Key_Space:
            onPlayPause();
            event->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            onActivateCurrent();
            event->accept();
            return;
        default:
            break;
        }
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::navigate(int offset)
{
    // Move the focus (round cap) between the bands without switching the sound.
    if (masterSlider_->hasFocus()) {
        if (offset > 0) {
            focusBand(focusedIndex_);
        }
        return;
    }

    const int current = focusedBandIndex();
    if (offset < 0 && current == 0) {
        const QWidget *focus = QApplication::focusWidget();
        const auto *firstSlider =
            bands_ != nullptr ? &bands_->column(0).slider() : nullptr;
        if (focus == this || focus == firstSlider) {
            masterSlider_->setFocus();
            return;
        }
    }

    const int newIndex = std::clamp(
        current + offset, 0, static_cast<int>(settings_.bands.size()) - 1);
    focusedIndex_ = newIndex;
    focusBand(newIndex);
    if (bands_ != nullptr) {
        bandScroll_->ensureWidgetVisible(&bands_->column(newIndex), 48, 0);
    }
}

void MainWindow::focusBand(const int index)
{
    if (bands_ != nullptr) {
        focusedIndex_ = index;
        bands_->column(index).slider().setFocus();
    }
}

void MainWindow::scrollBands(const int delta)
{
    if (auto *bar = bandScroll_->horizontalScrollBar()) {
        bar->setValue(bar->value() - delta);
    }
}

void MainWindow::nudgeActiveBand(const int direction)
{
    // Change the level of the band whose handle is in focus.
    const int index = focusedBandIndex();
    const LevelScale scale = settings_.levelScale;
    const double current = player_->levelAt(index);
    const double level = scale.clamp(current + direction * scale.stepDb);
    if (!tp::exactEq(level, current)) {
        onBandLevel(index, level);
        if (bands_ != nullptr) {
            bands_->column(index).setLevel(level);
        }
    }
}

void MainWindow::onActivateCurrent()
{
    // Enter on the current item: direct playback to the focused band.
    if (masterSlider_->hasFocus()) {
        return;
    }
    onActivateBand(focusedBandIndex());
}

void MainWindow::onActivateBand(const int index)
{
    // Activate a band: reset / play from 0:00 without changing the state.
    const PlayerState state = player_->state();
    if (state == PlayerState::Playing) {
        // The tone is on: immediately play the selected band from the start
        // (or restart the current one).
        startPlayback(index);
        return;
    }
    // No sound (Paused, Idle, Finished): the tone is assigned, but not played.
    player_->select(index);
    finishingHandled_ = false;
    refresh();
}

void MainWindow::startPlayback(const int index)
{
    // Start playback from the tone at index, showing the device error.
    try {
        player_->start(index);
    } catch (const AudioError &error) {
        QMessageBox::critical(this, tr_ns::tr("dialog.playFailed.title"), error.message());
        return;
    }
    finishingHandled_ = false;
    refresh();
}

void MainWindow::onStop()
{
    player_->stop();
    finishingHandled_ = false;
    refresh();
}

void MainWindow::onBandLevel(const int index, const double levelDb)
{
    // A level change is applied live to the sound only.
    // It is not automatically written to the config file.
    player_->setLevelAt(index, levelDb);
}

void MainWindow::onBandSelect(const int index)
{
    // Click on a band: put the focus on it without breaking the sound.
    focusedIndex_ = index;
    focusBand(index);
}

void MainWindow::onBandPlay(const int index)
{
    // Click on the play button under the band: direct control of the tone.
    focusBand(index);
    const PlayerState state = player_->state();
    if (index == player_->toneIndex()) {
        if (state == PlayerState::Playing) {
            player_->pause();
            refresh();
            return;
        }
        if (state == PlayerState::Paused) {
            player_->resume();
            refresh();
            return;
        }
    }
    startPlayback(index);
}

void MainWindow::onMasterChanged(const int position)
{
    const double gainDb = masterGainScale.atPosition(position);
    player_->setMasterDb(tp::masterBaseDb + gainDb);
    Settings updated = settings_;
    updated.masterGainDb = gainDb;
    store(updated);
    refresh();
}

void MainWindow::onDeviceChanged(const int index)
{
    // The first item is the default device; the band columns of the device list are the devices in order.
    const auto devicePtr =
        index >= 1 ? std::optional<OutputDevice>{devices_.at(index - 1)} : std::nullopt;
    try {
        player_->setDevice(
            devicePtr.has_value() ? std::optional{devicePtr->label()} : std::nullopt);
    } catch (const AudioError &) {
        return;
    }
    showNotice(QString());
    Settings updated = settings_;
    updated.device = devicePtr.has_value() ? std::optional{devicePtr->label()} : std::nullopt;
    store(updated);
    refresh();
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(settings_, this);
    dialog.setStyleSheet(styleSheet());
    if (dialog.exec() == QDialog::DialogCode::Accepted) {
        applySettings(dialog.settings());
    }
}

void MainWindow::applySettings(const Settings &settings, const QString &notice,
    const bool persist)
{
    // Apply the settings entirely: stop the sound, rebuild the bands and sliders.
    player_->stop();
    settings_ = settings;
    config_ = ToneConfig::fromSettings(settings_);
    player_->reconfigure(config_, settings_.levels(), settings_.levelScale);
    player_->setMasterDb(tp::masterBaseDb + settings_.masterGainDb);
    player_->setAutoplay(settings_.autoplay);
    const auto configured = audio::findDevice(devices_, settings_.device);
    player_->setDevice(
        configured.has_value() ? std::optional{configured->label()} : std::nullopt);
    QString text = notice;
    if (!configured.has_value() && settings_.device.has_value()) {
        text = tr_ns::tr("audio.deviceNotFound", settings_.device.value());
    }
    showNotice(text);
    syncDeviceSelector();
    syncAutoplay();
    syncMaster();
    rebuildBands();
    refreshPresets();
    if (persist) {
        store(settings_);
    }
    refresh();
}

void MainWindow::syncDeviceSelector()
{
    const std::optional<QString> target = player_->deviceLabel();
    QSignalBlocker blocker(deviceSelector_);
    int index = 0;
    if (target.has_value()) {
        for (int candidate = 0; candidate < deviceSelector_->count(); ++candidate) {
            if (deviceSelector_->itemText(candidate) == target.value()) {
                index = candidate;
                break;
            }
        }
    }
    deviceSelector_->setCurrentIndex(std::max(index, 0));
}

void MainWindow::syncAutoplay()
{
    QSignalBlocker blocker(autoplayButton_);
    autoplayButton_->setChecked(settings_.autoplay);
    updateAutoplayText(settings_.autoplay);
}

void MainWindow::syncMaster()
{
    QSignalBlocker blocker(masterSlider_);
    masterSlider_->setValue(masterGainScale.position(settings_.masterGainDb));
    masterValueLabel_->setText(text_format::formatDb(settings_.masterGainDb, false));
}

// -----------------------------------------------------------------------------
// Presets
// -----------------------------------------------------------------------------

void MainWindow::refreshPresets()
{
    {
        QSignalBlocker blocker(presetSelector_);
        presetSelector_->clear();
        presetSelector_->addItem(tr_ns::tr("preset.default"));
        for (const Preset &preset : settings_.presets) {
            presetSelector_->addItem(preset.name, preset.name);
        }
        if (settings_.activePreset.has_value()) {
            const int index = presetSelector_->findData(settings_.activePreset.value());
            if (index >= 0) {
                presetSelector_->setCurrentIndex(index);
            }
        }
    }
    presetDeleteButton_->setEnabled(settings_.activePreset.has_value());
}

void MainWindow::onPresetChanged(const int index)
{
    const QVariant selected = presetSelector_->itemData(index);
    if (!selected.isValid()) {
        // The default preset: the levels strictly at 0.0 dB.
        Settings updated = settings_;
        for (Band &band : updated.bands) {
            band.levelDb = 0.0;
        }
        updated.activePreset = std::nullopt;
        settings_ = updated;
        presetDeleteButton_->setEnabled(false);
        for (int band = 0; band < settings_.bands.size(); ++band) {
            player_->setLevelAt(band, 0.0);
            if (bands_ != nullptr) {
                bands_->column(band).setLevel(0.0);
            }
        }
        store(settings_);
        refresh();
        return;
    }

    const QSet<QString> available = [&] {
        QSet<QString> labels;
        for (const OutputDevice &device : devices_) {
            labels.insert(device.label());
        }
        return labels;
    }();
    try {
        const auto application =
            presets::applyPreset(settings_, selected.toString(), available);
        QString notice;
        if (application.missingDevice.has_value()) {
            notice = tr_ns::tr("audio.presetDeviceNotFound", application.missingDevice.value());
        }
        applySettings(application.settings, notice);
    } catch (const PresetError &error) {
        QMessageBox::warning(this, tr_ns::tr("preset.notApplied.title"), error.message());
        refreshPresets();
    }
}

std::optional<QString> MainWindow::askPresetName(const QString &fallback)
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(tr_ns::tr("presetdialog.title"));
    dialog->setMinimumWidth(400);
    dialog->setStyleSheet(styleSheet());

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    layout->addWidget(new QLabel(tr_ns::tr("presetdialog.nameLabel"), dialog));

    auto *edit = new QLineEdit(fallback, dialog);
    edit->setFixedHeight(38);
    edit->selectAll();
    layout->addWidget(edit);

    auto *buttons = new QDialogButtonBox(dialog);
    auto *save = buttons->addButton(tr_ns::tr("dialog.save"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr_ns::tr("dialog.cancel"), QDialogButtonBox::RejectRole);
    save->setDefault(true);
    save->setObjectName(u"playButton"_s);
    buttons->setFixedHeight(38);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(edit, &QLineEdit::returnPressed, dialog, &QDialog::accept);
    layout->addWidget(buttons);

    edit->setFocus();
    if (dialog->exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return edit->text();
}

void MainWindow::onSavePreset()
{
    const QString fallback = settings_.activePreset.value_or(tr_ns::tr("preset.defaultName"));
    const auto name = askPresetName(fallback);
    if (!name.has_value()) {
        return;
    }
    try {
        bool overwrite = false;
        const auto existing = presets::findPreset(settings_, name->trimmed());
        if (existing.has_value()) {
            const auto answer = QMessageBox::question(this,
                tr_ns::tr("preset.overwrite.title"),
                tr_ns::tr("preset.overwrite.text", existing->name));
            if (answer != QMessageBox::StandardButton::Yes) {
                return;
            }
            overwrite = true;
        }
        const Settings updated = presets::savePreset(settings_, name->trimmed(),
            player_->levels(), overwrite);
        showNotice(tr_ns::tr("preset.saved", updated.activePreset.value()));
        store(updated);
        refreshPresets();
        refresh();
    } catch (const PresetError &error) {
        QMessageBox::warning(this, tr_ns::tr("preset.notSaved.title"), error.message());
    }
}

void MainWindow::onDeletePreset()
{
    if (!settings_.activePreset.has_value()) {
        return;
    }
    const QString name = settings_.activePreset.value();
    const auto answer = QMessageBox::question(
        this, tr_ns::tr("preset.delete.title"), tr_ns::tr("preset.delete.text", name));
    if (answer != QMessageBox::StandardButton::Yes) {
        return;
    }
    try {
        Settings updated = presets::deletePreset(settings_, name);
        updated.activePreset = std::nullopt;
        for (Band &band : updated.bands) {
            band.levelDb = 0.0;
        }
        settings_ = updated;
        for (int band = 0; band < settings_.bands.size(); ++band) {
            player_->setLevelAt(band, 0.0);
            if (bands_ != nullptr) {
                bands_->column(band).setLevel(0.0);
            }
        }
        store(settings_);
        refreshPresets();
        refresh();
    } catch (const PresetError &error) {
        QMessageBox::warning(this, tr_ns::tr("preset.notDeleted.title"), error.message());
    }
}

// -----------------------------------------------------------------------------
// Settings storage
// -----------------------------------------------------------------------------

void MainWindow::store(Settings settings)
{
    // Accept the new settings; the file is written after a short pause.
    settings_ = std::move(settings);
    saveTimer_.start();
}

void MainWindow::showNotice(const QString &text)
{
    // Show the service message and remove it after a few seconds.
    notice_ = text;
    if (!text.isEmpty()) {
        noticeTimer_.start();
    } else {
        noticeTimer_.stop();
    }
}

void MainWindow::clearNotice()
{
    if (notice_.isEmpty()) {
        return;
    }
    notice_.clear();
    refresh();
}

QByteArray MainWindow::readSettingsFile() const
{
    QFile file(QString::fromStdString(settingsPath_.string()));
    if (!file.open(QIODeviceBase::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void MainWindow::onSettingsFileChanged(const QString &path)
{
    if (path != QString::fromStdString(settingsPath_.string())) {
        return;
    }
    settingsWatcher_->addPath(QString::fromStdString(settingsPath_.string()));
    reloadTimer_.start();
}

void MainWindow::reloadSettings()
{
    settingsWatcher_->addPath(QString::fromStdString(settingsPath_.string()));
    const QByteArray content = readSettingsFile();
    if (content.isEmpty() || content == settingsFileBytes_) {
        return;
    }
    saveTimer_.stop();
    try {
        const Settings loaded = settings_store::loadSettings(settingsPath_);
        if (!(loaded == settings_)) {
            applySettings(loaded, tr_ns::tr("reload.done"), false);
        }
    } catch (const SettingsError &error) {
        showNotice(tr_ns::tr("reload.failed", error.message()));
        refresh();
        return;
    }
}

void MainWindow::saveSettings()
{
    saveTimer_.stop();
    const QByteArray content = readSettingsFile();
    if (!content.isEmpty() && content != settingsFileBytes_) {
        reloadSettings();
        return;
    }
    try {
        settings_store::saveSettings(settingsPath_, settings_);
        saveError_.clear();
        settingsFileBytes_ = readSettingsFile();
        settingsWatcher_->addPath(QString::fromStdString(settingsPath_.string()));
    } catch (const SettingsError &error) {
        saveError_ = error.message();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // On window closing flush the settings, stop the timers and free the device.
    if (saveTimer_.isActive()) {
        saveSettings();
    }
    if (!saveError_.isEmpty()) {
        QMessageBox::warning(this, tr_ns::tr("save.errorTitle"), saveError_);
    }
    noticeTimer_.stop();
    reloadTimer_.stop();
    timer_.stop();
    player_->stop();
    QMainWindow::closeEvent(event);
}

// -----------------------------------------------------------------------------
// State update
// -----------------------------------------------------------------------------

void MainWindow::refresh()
{
    PlayerState state = player_->state();
    PlaybackProgress progress = player_->progress();

    if (state == PlayerState::Finished && !finishingHandled_) {
        finishingHandled_ = true;
        player_->finish();
        state = player_->state();
        progress = player_->progress();
    }

    frequencyLabel_->setText(frequencyHtml(
        text_format::formatFrequencyValue(player_->currentFrequency())));
    toneCounterLabel_->setText(
        tr_ns::tr("playback.toneCounter", progress.toneIndex + 1, config_.toneCount()));
    remainingLabel_->setText(tr_ns::tr("playback.remaining", text_format::formatDuration(
        progress.toneRemaining)));
    toneProgress_->setValue(static_cast<int>(std::lround(progress.toneFraction() * 100)));

    const bool active = state == PlayerState::Playing || state == PlayerState::Paused;
    stopButton_->setEnabled(active);
    deviceSelector_->setEnabled(!active);

    const PlayButtonMode mode = state == PlayerState::Playing ? PlayButtonMode::Pause
        : state == PlayerState::Paused ? PlayButtonMode::Resume
                                       : PlayButtonMode::Start;
    if (mode != playButtonMode_.value_or(PlayButtonMode::Start) || !playButtonMode_.has_value())
    {
        playButtonMode_ = mode;
        playButton_->setIcon(icons_[playButtonIcon(mode)]);
        playButton_->setToolTip(tr_ns::tr("play.tooltip", playButtonLabel(mode)));
        playButton_->setAccessibleName(playButtonLabel(mode));
    }

    if (bands_ != nullptr) {
        bands_->setCurrent(progress.toneIndex, state == PlayerState::Playing);
        if (progress.toneIndex != lastBandIndex_) {
            lastBandIndex_ = progress.toneIndex;
            focusedIndex_ = focusedBandIndex();
            if (focusedIndex_ == progress.toneIndex || !bandSliderFocused()) {
                bandScroll_->ensureWidgetVisible(&bands_->column(progress.toneIndex), 48, 0);
            }
        }
    }

    masterValueLabel_->setText(
        text_format::formatDb(player_->masterDb() - tp::masterBaseDb, false));

    QString status = player_->lastStatus();
    if (status.isEmpty()) {
        status = !saveError_.isEmpty() ? saveError_ : notice_;
    }
    statusLabel_->setText(status);
    statusLabel_->setVisible(!status.isEmpty());
}

/// @brief One-line description of the player for remote clients.
/// Same shape as the Python version: `state=playing tone=1/20 frequency=500Hz
/// remaining=200.0s autoplay=on master=+0.0dB`.
QString MainWindow::handleRemoteCall(const QString &name, const QStringList &args)
{
    const auto summary = [this]() {
        const auto progress = player_->progress();
        const char *stateName = "idle";
        switch (player_->state()) {
        case PlayerState::Playing:
            stateName = "playing";
            break;
        case PlayerState::Paused:
            stateName = "paused";
            break;
        case PlayerState::Finished:
            stateName = "finished";
            break;
        default:
            break;
        }
        return QStringLiteral(
            "state=%1 tone=%2/%3 frequency=%4Hz remaining=%5s autoplay=%6 master=%7dB")
            .arg(QString::fromLatin1(stateName))
            .arg(progress.toneIndex + 1)
            .arg(settings_.bands.size())
            .arg(QString::number(player_->currentFrequency(), 'g', 6))
            .arg(progress.totalRemaining, 0, 'f', 1)
            .arg(settings_.autoplay ? QStringLiteral("on") : QStringLiteral("off"))
            .arg(QString::asprintf("%+.1f", settings_.masterGainDb));
    };
    const auto indexArg = [this, &args]() -> int {
        if (args.isEmpty()) {
            throw std::runtime_error("missing band index");
        }
        bool ok = false;
        const int raw = args.first().toInt(&ok);
        if (!ok) {
            throw std::runtime_error(
                ("not an integer: " + args.first()).toStdString());
        }
        return std::clamp(raw, 0, static_cast<int>(settings_.bands.size()) - 1);
    };
    const auto startOrThrow = [this](const int index) {
        try {
            player_->start(index);
        } catch (const AudioError &error) {
            throw std::runtime_error(error.message().toStdString());
        }
        finishingHandled_ = false;
        refresh();
    };
    const auto stopOrThrow = [this]() {
        player_->stop();
        finishingHandled_ = false;
        refresh();
    };

    const QString call = name.toLower();
    if (call == QStringLiteral("status")) {
        return summary();
    }
    if (call == QStringLiteral("stop")) {
        stopOrThrow();
        return summary();
    }
    if (call == QStringLiteral("toggle")) {
        const PlayerState state = player_->state();
        if (state == PlayerState::Playing || state == PlayerState::Paused) {
            stopOrThrow();
            return summary();
        }
        startOrThrow(state == PlayerState::Finished ? 0 : focusedBandIndex());
        return summary();
    }
    if (call == QStringLiteral("play")) {
        if (!args.isEmpty()) {
            startOrThrow(indexArg());
            return summary();
        }
        const PlayerState state = player_->state();
        if (state == PlayerState::Playing) {
            player_->pause();
            refresh();
            return summary();
        }
        if (state == PlayerState::Paused) {
            startOrThrow(player_->toneIndex());
            return summary();
        }
        startOrThrow(state == PlayerState::Finished ? 0 : focusedBandIndex());
        return summary();
    }
    throw std::runtime_error(
        ("unknown call " + name + " (status|play|stop|toggle)").toStdString());
}

} // namespace tp::ui
