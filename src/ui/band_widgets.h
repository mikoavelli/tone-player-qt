// Equalizer widgets: vertical sliders, ruler and band columns.
#pragma once

#include "models.h"

#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QHBoxLayout>
#include <QWidget>
#include <functional>
#include <qhash.h>

class QWheelEvent;
class QPaintEvent;
class QEvent;
class QContextMenuEvent;
class QEnterEvent;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;

namespace tp::ui {

// Geometry of a band column.
inline constexpr int columnWidth = 66;
inline constexpr int columnValueHeight = 20;
inline constexpr int columnFrequencyHeight = 18;
inline constexpr int columnPlaySize = 28;
inline constexpr int columnSpacing = 6;
inline constexpr int columnMargin = 4;
// Slider inside the column: column width minus margins on the sides.
inline constexpr int sliderWidth = columnWidth - 2 * columnMargin;
inline constexpr int rulerWidth = 40;
// Gap between the ruler and the column: the same in the band strip and at the master.
inline constexpr int rulerSpacing = 4;
inline constexpr int frameRadius = 10;

// Wheel or touchpad gesture → horizontal scroll delta.
[[nodiscard]] int wheelScrollDelta(const QWheelEvent &event);
// Height on the slider track for a position (maximum at the top).
[[nodiscard]] double trackY(int position, int maximum, int height);
// Draw the column frame; accent — the bright frame of playback.
void paintColumnFrame(QWidget &widget, bool accent);

// Vertical slider with a zero line and double-click reset.
class LevelSlider final : public QSlider {
    Q_OBJECT

public:
    explicit LevelSlider(LevelScale scale, QWidget *parent = nullptr);

    void setScale(const LevelScale &scale);
    // The position corresponding to 0 dB.
    [[nodiscard]] int zeroPosition() const;
    // Precise height of the handle center for a position, by style geometry.
    [[nodiscard]] double handleCenterY(int position) const;

signals:
    void navigateRequested(int offset);
    void scrollRequested(int delta);
    void activateRequested();
    void focusChanged(bool focused);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    [[nodiscard]] QRect handleRect(int position) const;
    [[nodiscard]] double trackZeroY() const;

    LevelScale scale_;
};

// Toggle switch (“toggle”); the API is like a checkable button.
class ToggleSwitch final : public QAbstractButton {
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr int trackWidth = 48;
    static constexpr int trackHeight = 26;
    static constexpr int knob = 20;
    static constexpr int pad = 3;
    static constexpr int ring = 2;
};

// Shared level scale to the left of the bands: maximum, zero and minimum.
class LevelRuler final : public QWidget {
    Q_OBJECT

public:
    explicit LevelRuler(LevelScale scale, QWidget *parent = nullptr);

    void setScale(const LevelScale &scale);
    void setSlider(const LevelSlider *slider);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    LevelScale scale_;
    const LevelSlider *slider_ = nullptr;
};

// Master volume column: the same width and frame as a band column.
class MasterFrame final : public QWidget {
    Q_OBJECT

public:
    explicit MasterFrame(QWidget *parent = nullptr);
};

// Column of one band: level, slider, frequency and the listening button.
class BandColumn final : public QWidget {
    Q_OBJECT

public:
    using LevelHandler = std::function<void(int, double)>;
    using IndexHandler = std::function<void(int)>;
    using ActionHandler = std::function<void()>;

    BandColumn(int index, const Band &band, const LevelScale &scale,
        const QHash<QString, QIcon> &icons, LevelHandler onLevel, IndexHandler onPlay,
        IndexHandler onSelect, IndexHandler onActivate, IndexHandler onNavigate,
        IndexHandler onScroll, QWidget *parent = nullptr);

    [[nodiscard]] int index() const noexcept { return index_; }
    [[nodiscard]] LevelSlider &slider() noexcept { return *slider_; }

    // Show the level without generating a change signal.
    void setLevel(double levelDb);
    // current — the column is playing; playing — there is active sound.
    void setCurrent(bool current, bool playing);
    void setMasterFocused(bool focused);
    void setScale(const LevelScale &scale);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    [[nodiscard]] double level() const;
    void updateText();
    void updateTooltip();

    int index_;
    LevelScale scale_;
    LevelHandler onLevel_;
    IndexHandler onPlay_;
    IndexHandler onSelect_;
    IndexHandler onActivate_;
    IndexHandler onNavigate_;
    IndexHandler onScroll_;
    QHash<QString, QIcon> icons_;
    QLabel valueLabel_;
    LevelSlider *slider_ = nullptr;
    QLabel *frequencyLabel_ = nullptr;
    QPushButton *playButton_ = nullptr;
    bool current_ = false;
    bool playing_ = false;
    bool hover_ = false;
    bool editing_ = false;
    bool masterFocused_ = false;
};

// Strip of equalizer bands with the ruler; placed in a scroll area.
class BandStrip final : public QWidget {
    Q_OBJECT

public:
    using LevelHandler = std::function<void(int, double)>;
    using IndexHandler = std::function<void(int)>;

    BandStrip(const LevelScale &scale, const QHash<QString, QIcon> &icons,
        LevelHandler onLevel, IndexHandler onPlay, IndexHandler onSelect,
        IndexHandler onActivate, IndexHandler onNavigate, IndexHandler onScroll,
        QWidget *parent = nullptr);

    // Rebuild the columns for a new set of bands.
    void setBands(const QVector<Band> &bands, const LevelScale &scale);
    void setCurrent(int index, bool playing);
    void setMasterFocused(bool focused);
    void setScale(const LevelScale &scale);
    [[nodiscard]] const BandColumn &column(int index) const;
    [[nodiscard]] BandColumn &column(int index);

private:
    void clear();

    LevelScale scale_;
    QHash<QString, QIcon> icons_;
    LevelHandler onLevel_;
    IndexHandler onPlay_;
    IndexHandler onSelect_;
    IndexHandler onActivate_;
    IndexHandler onNavigate_;
    IndexHandler onScroll_;
    QHBoxLayout *layout_;
    QVector<BandColumn *> columns_;
    LevelRuler *ruler_ = nullptr;
    bool masterFocused_ = false;
};

} // namespace tp::ui
