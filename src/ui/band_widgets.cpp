// Equalizer widgets: vertical sliders, the ruler and band columns.
#include "band_widgets.h"

#include "theme.h"
#include "text_format.h"

#include "../i18n.h"

#include <QAbstractButton>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QRegion>
#include <QStyleOptionSlider>
#include <QWheelEvent>
#include <qbrush.h>
#include <qfontmetrics.h>

namespace tp::ui {

using tr_ns::tr;
using namespace Qt::Literals::StringLiterals;

using theme::accentColor;
using theme::borderColor;
using theme::borderHoverColor;
using theme::mutedColor;
using theme::surfaceColor;
using theme::surfaceHoverColor;
using theme::textColor;

int wheelScrollDelta(const QWheelEvent &event)
{
    const QPoint pixel = event.pixelDelta();
    if (!pixel.isNull()) {
        return pixel.x() != 0 ? pixel.x() : pixel.y();
    }
    const QPoint angle = event.angleDelta();
    return angle.x() != 0 ? angle.x() : angle.y();
}

double trackY(const int position, const int maximum, const int height)
{
    const double ratio = maximum > 0 ? static_cast<double>(position) / maximum : 0.0;
    return static_cast<double>(height) - ratio * height;
}

void paintColumnFrame(QWidget &widget, const bool accent)
{
    QPainter painter(&widget);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing);
    painter.setBrush(QColor(accent ? surfaceHoverColor : surfaceColor));
    painter.setPen(QPen(QColor(accent ? accentColor : borderColor), 1));
    painter.drawRoundedRect(QRectF(widget.rect()).adjusted(1, 1, -1, -1),
        frameRadius, frameRadius);
    painter.end();
}

// -----------------------------------------------------------------------------
// LevelSlider
// -----------------------------------------------------------------------------

LevelSlider::LevelSlider(const LevelScale scale, QWidget *parent)
    : QSlider(Qt::Orientation::Vertical, parent)
    , scale_(scale)
{
    setRange(0, scale_.positionCount());
    setSingleStep(1);
    setPageStep(std::max(1, static_cast<int>(std::round(1.0 / scale_.stepDb))));
    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
}

void LevelSlider::setScale(const LevelScale &scale)
{
    scale_ = scale;
    setRange(0, scale_.positionCount());
    setSingleStep(1);
    setPageStep(std::max(1, static_cast<int>(std::round(1.0 / scale_.stepDb))));
    update();
}

int LevelSlider::zeroPosition() const
{
    return scale_.position(0.0);
}

QRect LevelSlider::handleRect(const int position) const
{
    QStyleOptionSlider option;
    initStyleOption(&option);
    option.sliderPosition = position;
    option.sliderValue = position;
    return style()->subControlRect(QStyle::ComplexControl::CC_Slider, &option,
        QStyle::SubControl::SC_SliderHandle, this);
}

double LevelSlider::handleCenterY(const int position) const
{
    const QRect rect = handleRect(position);
    if (rect.isNull()) {
        return trackY(position, maximum(), height());
    }
    return rect.center().y();
}

double LevelSlider::trackZeroY() const
{
    const QRect rect = handleRect(zeroPosition());
    if (rect.isNull()) {
        return trackY(zeroPosition(), maximum(), height());
    }
    return rect.center().y();
}

void LevelSlider::paintEvent(QPaintEvent *event)
{
    QSlider::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing);
    const QRect handle = handleRect(value());

    // The zero line must not shine through the handle.
    painter.save();
    if (!handle.isNull()) {
        QRegion region = QRegion(rect());
        region = region.subtracted(QRegion(handle.adjusted(-2, -2, 2, 2)));
        painter.setClipRegion(region);
    }
    const double y = trackZeroY();
    painter.setPen(QPen(QColor(borderHoverColor), 1, Qt::PenStyle::DashLine));
    painter.drawLine(QPointF(8.0, y), QPointF(width() - 8.0, y));
    painter.restore();

    // Keyboard focus: a light ring inside the handle.
    if (hasFocus() && !handle.isNull()) {
        painter.setPen(QPen(QColor(textColor), 2));
        const QRectF ring = QRectF(handle).adjusted(1.5, 1.5, -1.5, -1.5);
        painter.drawRoundedRect(ring, ring.height() / 2, ring.height() / 2);
    }
    painter.end();
}

void LevelSlider::mouseDoubleClickEvent(QMouseEvent *event)
{
    setValue(zeroPosition());
    event->accept();
}

void LevelSlider::contextMenuEvent(QContextMenuEvent *event)
{
    setValue(zeroPosition());
    event->accept();
}

void LevelSlider::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        emit navigateRequested(event->key() == Qt::Key_Left ? -1 : 1);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit activateRequested();
        event->accept();
        return;
    }
    QSlider::keyPressEvent(event);
}

void LevelSlider::wheelEvent(QWheelEvent *event)
{
    emit scrollRequested(wheelScrollDelta(*event));
    event->accept();
}

void LevelSlider::focusInEvent(QFocusEvent *event)
{
    QSlider::focusInEvent(event);
    emit focusChanged(true);
}

void LevelSlider::focusOutEvent(QFocusEvent *event)
{
    QSlider::focusOutEvent(event);
    emit focusChanged(false);
}

// -----------------------------------------------------------------------------
// ToggleSwitch
// -----------------------------------------------------------------------------

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setFocusPolicy(Qt::FocusPolicy::TabFocus);
    setCursor(Qt::CursorShape::PointingHandCursor);
    setFixedSize(trackWidth + 2 * ring, trackHeight + 2 * ring);
}

void ToggleSwitch::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing);
    if (!isEnabled()) {
        painter.setOpacity(0.5);
    }

    const bool checked = isChecked();
    const QRectF track(ring, ring, trackWidth, trackHeight);
    painter.setPen(Qt::PenStyle::NoPen);
    painter.setBrush(QColor(checked ? theme::accentColor : theme::switchOffColor));
    painter.drawRoundedRect(track, trackHeight / 2, trackHeight / 2);

    const double knobX = checked
        ? track.right() - pad - knob
        : track.left() + pad;
    painter.setBrush(QColor(checked ? theme::onAccentColor : theme::mutedColor));
    painter.drawEllipse(QRectF(knobX, track.top() + pad, knob, knob));

    if (hasFocus()) {
        painter.setBrush(Qt::BrushStyle::NoBrush);
        painter.setPen(QPen(QColor(theme::textColor), 2));
        const double radius = height() / 2 - 1;
        painter.drawRoundedRect(QRectF(1, 1, width() - 2, height() - 2), radius, radius);
    }
    painter.end();
    QWidget::paintEvent(event);
}

// -----------------------------------------------------------------------------
// LevelRuler
// -----------------------------------------------------------------------------

LevelRuler::LevelRuler(const LevelScale scale, QWidget *parent)
    : QWidget(parent)
    , scale_(scale)
{
    setFixedWidth(rulerWidth);
    setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Expanding);
}

void LevelRuler::setScale(const LevelScale &scale)
{
    scale_ = scale;
    update();
}

void LevelRuler::setSlider(const LevelSlider *slider)
{
    slider_ = slider;
    update();
}

void LevelRuler::paintEvent(QPaintEvent * /*unused*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing);
    QFont font = this->font();
    font.setPixelSize(11);
    painter.setFont(font);

    const std::array<std::pair<int, QString>, 3> positions = {
        std::pair{scale_.position(scale_.maxDb), text_format::formatScale(scale_.maxDb)},
        std::pair{scale_.position(0.0), QStringLiteral("0")},
        std::pair{scale_.position(scale_.minDb), text_format::formatScale(scale_.minDb)},
    };
    std::array<double, 3> yPositions = {};
    if (slider_ != nullptr) {
        const double top = slider_->mapToGlobal(QPoint(0, 0)).y()
            - mapToGlobal(QPoint(0, 0)).y();
        for (size_t index = 0; index < positions.size(); ++index) {
            yPositions[index] = top + slider_->handleCenterY(positions[index].first);
        }
    } else {
        for (size_t index = 0; index < positions.size(); ++index) {
            yPositions[index] =
                trackY(positions[index].first, scale_.positionCount(), height());
        }
    }

    for (size_t index = 0; index < positions.size(); ++index) {
        const double y = yPositions[index];
        painter.setPen(QPen(QColor(borderHoverColor), 1));
        painter.drawLine(QPointF(width() - 12.0, y), QPointF(width() - 5.0, y));
        painter.setPen(QColor(mutedColor));
        const double labelY =
            std::clamp(y - 8.0, 0.0, std::max(0.0, height() - 16.0));
        painter.drawText(QRectF(0.0, labelY, width() - 15.0, 16.0),
            static_cast<int>(Qt::AlignmentFlag::AlignRight
                | Qt::AlignmentFlag::AlignVCenter),
            positions[index].second);
    }
    painter.end();
}

// -----------------------------------------------------------------------------
// MasterFrame
// -----------------------------------------------------------------------------

MasterFrame::MasterFrame(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(columnWidth);
    setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Expanding);
}

// -----------------------------------------------------------------------------
// BandColumn
// -----------------------------------------------------------------------------

BandColumn::BandColumn(const int index, const Band &band, const LevelScale &scale,
    const QHash<QString, QIcon> &icons, LevelHandler onLevel, IndexHandler onPlay,
    IndexHandler onSelect, IndexHandler onActivate, IndexHandler onNavigate,
    IndexHandler onScroll, QWidget *parent)
    : QWidget(parent)
    , index_(index)
    , scale_(scale)
    , onLevel_(std::move(onLevel))
    , onPlay_(std::move(onPlay))
    , onSelect_(std::move(onSelect))
    , onActivate_(std::move(onActivate))
    , onNavigate_(std::move(onNavigate))
    , onScroll_(std::move(onScroll))
    , icons_(icons)
{
    setFixedWidth(columnWidth);
    setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Expanding);
    setCursor(Qt::CursorShape::PointingHandCursor);

    auto *box = new QVBoxLayout(this);
    box->setContentsMargins(columnMargin, columnMargin, columnMargin, columnMargin);
    box->setSpacing(columnSpacing);

    valueLabel_.setObjectName(u"bandValue"_s);
    valueLabel_.setAlignment(Qt::AlignmentFlag::AlignCenter);
    valueLabel_.setFixedHeight(columnValueHeight);
    box->addWidget(&valueLabel_);

    slider_ = new LevelSlider(scale, this);
    slider_->setAccessibleName(
        tr_ns::tr("band.accessible", text_format::formatFrequencyValue(band.frequencyHz)));
    slider_->setValue(scale_.position(band.levelDb));
    connect(slider_, &LevelSlider::valueChanged, this,
        [this](int) { updateText(); onLevel_(index_, level()); });
    connect(slider_, &LevelSlider::sliderPressed, this, [this] {
        editing_ = true;
        onSelect_(index_);
        updateText();
    });
    connect(slider_, &LevelSlider::sliderReleased, this,
        [this] { editing_ = false; updateText(); });
    connect(slider_, &LevelSlider::navigateRequested, this,
        [this](int offset) { onNavigate_(offset); });
    connect(slider_, &LevelSlider::scrollRequested, this,
        [this](int delta) { onScroll_(delta); });
    connect(slider_, &LevelSlider::activateRequested, this,
        [this] { onActivate_(index_); });
    connect(slider_, &LevelSlider::focusChanged, this, [this](bool) { updateText(); });
    box->addWidget(slider_, 1);

    frequencyLabel_ = new QLabel(text_format::formatFrequencyValue(band.frequencyHz), this);
    frequencyLabel_->setObjectName(u"bandFrequency"_s);
    frequencyLabel_->setAlignment(Qt::AlignmentFlag::AlignCenter);
    frequencyLabel_->setFixedHeight(columnFrequencyHeight);
    box->addWidget(frequencyLabel_);

    playButton_ = new QPushButton(this);
    playButton_->setObjectName(u"bandPlay"_s);
    playButton_->setFixedSize(columnPlaySize, columnPlaySize);
    playButton_->setIcon(icons_[u"bandStart"_s]);
    playButton_->setIconSize(QSize(14, 14));
    playButton_->setCursor(Qt::CursorShape::PointingHandCursor);
    playButton_->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    connect(playButton_, &QPushButton::clicked, this, [this] { onPlay_(index_); });
    box->addWidget(playButton_, 0, Qt::AlignmentFlag::AlignHCenter);

    setLevel(band.levelDb);
    updateTooltip();
    setCurrent(false, false);
}

double BandColumn::level() const
{
    return scale_.atPosition(slider_->value());
}

void BandColumn::setLevel(const double levelDb)
{
    QSignalBlocker blocker(slider_);
    slider_->setValue(scale_.position(levelDb));
    updateText();
}

void BandColumn::setCurrent(const bool current, const bool playing)
{
    if (current == current_ && playing == playing_) {
        return;
    }
    current_ = current;
    playing_ = playing;
    playButton_->setIcon(
        icons_[current_ && playing_ ? u"bandPause"_s : u"bandStart"_s]);
    updateText();
    update();
}

void BandColumn::setMasterFocused(const bool focused)
{
    if (masterFocused_ != focused) {
        masterFocused_ = focused;
        update();
    }
}

void BandColumn::setScale(const LevelScale &scale)
{
    scale_ = scale;
    slider_->setScale(scale);
    updateTooltip();
}

void BandColumn::updateText()
{
    const bool visible = current_ || slider_->hasFocus() || hover_ || editing_;
    valueLabel_.setText(visible ? text_format::formatDb(level(), false) : QString());
    updateTooltip();
}

void BandColumn::updateTooltip()
{
    const QString text = tr_ns::tr("band.tooltip", frequencyLabel_->text(),
        text_format::formatDb(level()));
    setToolTip(text);
    slider_->setToolTip(text);
    frequencyLabel_->setToolTip(text);
}

void BandColumn::mousePressEvent(QMouseEvent *event)
{
    onSelect_(index_);
    slider_->setFocus();
    QWidget::mousePressEvent(event);
}

void BandColumn::contextMenuEvent(QContextMenuEvent *event)
{
    setLevel(0.0);
    onLevel_(index_, 0.0);
    event->accept();
}

void BandColumn::enterEvent(QEnterEvent *event)
{
    hover_ = true;
    updateText();
    QWidget::enterEvent(event);
}

void BandColumn::leaveEvent(QEvent *event)
{
    hover_ = false;
    editing_ = false;
    updateText();
    QWidget::leaveEvent(event);
}

void BandColumn::paintEvent(QPaintEvent *event)
{
    // The frame is drawn exclusively around the playing frequency.
    if (current_) {
        paintColumnFrame(*this, playing_);
    }
    QWidget::paintEvent(event);
}

// -----------------------------------------------------------------------------
// BandStrip
// -----------------------------------------------------------------------------

BandStrip::BandStrip(const LevelScale &scale, const QHash<QString, QIcon> &icons,
    LevelHandler onLevel, IndexHandler onPlay, IndexHandler onSelect,
    IndexHandler onActivate, IndexHandler onNavigate, IndexHandler onScroll,
    QWidget *parent)
    : QWidget(parent)
    , scale_(scale)
    , icons_(icons)
    , onLevel_(std::move(onLevel))
    , onPlay_(std::move(onPlay))
    , onSelect_(std::move(onSelect))
    , onActivate_(std::move(onActivate))
    , onNavigate_(std::move(onNavigate))
    , onScroll_(std::move(onScroll))
{
    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(4, 0, 8, 0);
    layout_->setSpacing(rulerSpacing);
    setSizePolicy(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
}

void BandStrip::setBands(const QVector<Band> &bands, const LevelScale &scale)
{
    clear();
    scale_ = scale;
    ruler_ = new LevelRuler(scale, this);
    layout_->addWidget(ruler_);
    for (int index = 0; index < bands.size(); ++index) {
        auto *column = new BandColumn(index, bands.at(index), scale, icons_, onLevel_,
            onPlay_, onSelect_, onActivate_, onNavigate_, onScroll_, this);
        column->setMasterFocused(masterFocused_);
        columns_.push_back(column);
        layout_->addWidget(column);
    }
    if (!columns_.isEmpty()) {
        ruler_->setSlider(&columns_.front()->slider());
    }
    layout_->addStretch(1);
}

void BandStrip::setCurrent(const int index, const bool playing)
{
    for (int number = 0; number < columns_.size(); ++number) {
        columns_[number]->setCurrent(number == index, playing && number == index);
        columns_[number]->setMasterFocused(masterFocused_);
    }
}

void BandStrip::setMasterFocused(const bool focused)
{
    if (masterFocused_ != focused) {
        masterFocused_ = focused;
        for (BandColumn *column : columns_) {
            column->setMasterFocused(focused);
        }
        update();
    }
}

void BandStrip::setScale(const LevelScale &scale)
{
    scale_ = scale;
    ruler_->setScale(scale);
    for (BandColumn *column : columns_) {
        column->setScale(scale);
    }
    update();
}

const BandColumn &BandStrip::column(const int index) const
{
    return *columns_.at(index);
}

BandColumn &BandStrip::column(const int index)
{
    return *columns_.at(index);
}

void BandStrip::clear()
{
    columns_.clear();
    while (const auto *item = layout_->takeAt(0)) {
        if (auto *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

} // namespace tp::ui
