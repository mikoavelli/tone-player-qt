/// @file band_widgets.h
/// @brief Equalizer widgets: vertical sliders, ruler and band columns.
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

/// @brief Widgets of the equalizer strip.
namespace tp::ui {

/// @name Geometry of a band column
/// @{

/// @brief Width of one band column, px.
inline constexpr int columnWidth = 66;
/// @brief Height of the value label at the top of a column, px.
inline constexpr int columnValueHeight = 20;
/// @brief Height of the frequency label at the bottom of a column, px.
inline constexpr int columnFrequencyHeight = 18;
/// @brief Size of the listening button in a column, px.
inline constexpr int columnPlaySize = 28;
/// @brief Spacing between widgets inside a column, px.
inline constexpr int columnSpacing = 6;
/// @brief Margin from the column edge to the slider, px.
inline constexpr int columnMargin = 4;
/// @brief Slider inside the column: column width minus margins on the sides.
inline constexpr int sliderWidth = columnWidth - 2 * columnMargin;
/// @brief Width of the shared level ruler, px.
inline constexpr int rulerWidth = 40;
/// @brief Gap between the ruler and the column: the same in the band strip and at the master.
inline constexpr int rulerSpacing = 4;
/// @brief Corner radius of the column frame, px.
inline constexpr int frameRadius = 10;

/// @}

/// @brief Converts a wheel or touchpad gesture to a horizontal scroll delta.
/// @param event Wheel event received by the strip.
/// @return Number of pixels to scroll.
[[nodiscard]] int wheelScrollDelta(const QWheelEvent &event);

/// @brief Returns the height on the slider track for a position (maximum at the top).
/// @param position Slider position in steps.
/// @param maximum Highest slider position.
/// @param height Available track height, px.
[[nodiscard]] double trackY(int position, int maximum, int height);

/// @brief Draws the column frame; @p accent selects the bright frame of playback.
/// @param widget Widget whose rect is framed.
/// @param accent Whether to draw the accent color.
void paintColumnFrame(QWidget &widget, bool accent);

/// @brief Vertical slider with a zero line and double-click reset.
class LevelSlider final : public QSlider {
    Q_OBJECT

public:
    /// @brief Creates a slider for a level scale.
    /// @param scale Range and step of the slider.
    /// @param parent Parent widget.
    explicit LevelSlider(LevelScale scale, QWidget *parent = nullptr);

    /// @brief Replaces the range and step of the slider.
    /// @param scale New scale.
    void setScale(const LevelScale &scale);
    /// @brief Returns the slider position corresponding to 0 dB.
    [[nodiscard]] int zeroPosition() const;
    /// @brief Returns the precise height of the handle center for a position, by style geometry.
    /// @param position Slider position in steps.
    [[nodiscard]] double handleCenterY(int position) const;

signals:
    /// @brief The user asked to move the focus to another band.
    /// @param offset Direction and distance in bands.
    void navigateRequested(int offset);
    /// @brief The user scrolled the band strip with the wheel.
    /// @param delta Horizontal scroll delta, px.
    void scrollRequested(int delta);
    /// @brief The user activated the band (Enter or click on the value).
    void activateRequested();
    /// @brief The slider gained or lost the keyboard focus.
    /// @param focused New focus state.
    void focusChanged(bool focused);

protected:
    /// @brief Draws the zero line and the focus ring over the style-drawn slider.
    void paintEvent(QPaintEvent *event) override;
    /// @brief Resets the level to 0 dB on a double click.
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    /// @brief Resets the level to 0 dB on a right click.
    void contextMenuEvent(QContextMenuEvent *event) override;
    /// @brief Left/Right navigate between bands, Enter activates, the rest goes to QSlider.
    void keyPressEvent(QKeyEvent *event) override;
    /// @brief Turns wheel scrolling into a request to scroll the band strip.
    void wheelEvent(QWheelEvent *event) override;
    /// @brief Reports that the slider gained the keyboard focus.
    void focusInEvent(QFocusEvent *event) override;
    /// @brief Reports that the slider lost the keyboard focus.
    void focusOutEvent(QFocusEvent *event) override;

private:
    /// @brief Returns the handle rect for a slider position.
    [[nodiscard]] QRect handleRect(int position) const;
    /// @brief Returns the track height that corresponds to 0 dB.
    [[nodiscard]] double trackZeroY() const;

    LevelScale scale_; ///< Range and step of the slider.
};

/// @brief Toggle switch (“toggle”); the API is like a checkable button.
class ToggleSwitch final : public QAbstractButton {
    Q_OBJECT

public:
    /// @brief Creates a switch in the unchecked state.
    /// @param parent Parent widget.
    explicit ToggleSwitch(QWidget *parent = nullptr);

protected:
    /// @brief Draws the track, the knob and the focus ring.
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr int trackWidth = 48; ///< Width of the switch track, px.
    static constexpr int trackHeight = 26; ///< Height of the switch track, px.
    static constexpr int knob = 20; ///< Diameter of the switch knob, px.
    static constexpr int pad = 3; ///< Padding between the knob and the track, px.
    static constexpr int ring = 2; ///< Width of the focus ring, px.
};

/// @brief Shared level scale to the left of the bands: maximum, zero and minimum.
class LevelRuler final : public QWidget {
    Q_OBJECT

public:
    /// @brief Creates a ruler for a level scale.
    /// @param scale Range and step shown by the ruler.
    /// @param parent Parent widget.
    explicit LevelRuler(LevelScale scale, QWidget *parent = nullptr);

    /// @brief Replaces the scale shown by the ruler.
    /// @param scale New scale.
    void setScale(const LevelScale &scale);
    /// @brief Sets the slider whose handle position the ruler labels follow.
    /// @param slider Slider to track; @c nullptr detaches the ruler.
    void setSlider(const LevelSlider *slider);

protected:
    /// @brief Draws the scale labels aligned with the slider positions.
    void paintEvent(QPaintEvent *event) override;

private:
    LevelScale scale_; ///< Scale shown by the ruler.
    const LevelSlider *slider_ = nullptr; ///< Slider used for vertical alignment.
};

/// @brief Master volume column: the same width and frame as a band column.
class MasterFrame final : public QWidget {
    Q_OBJECT

public:
    /// @brief Creates the frame of the master column.
    /// @param parent Parent widget.
    explicit MasterFrame(QWidget *parent = nullptr);
};

/// @brief Column of one band: level, slider, frequency and the listening button.
class BandColumn final : public QWidget {
    Q_OBJECT

public:
    /// @brief Callback for a changed band level.
    using LevelHandler = std::function<void(int, double)>;
    /// @brief Callback for an action on a band index.
    using IndexHandler = std::function<void(int)>;
    /// @brief Callback without arguments.
    using ActionHandler = std::function<void()>;

    /// @brief Creates the column of one band.
    /// @param index Band index in the shared list.
    /// @param band Frequency and initial level of the band.
    /// @param scale Range and step of the slider.
    /// @param icons Icon set shared by all columns.
    /// @param onLevel Called when the user moves the slider.
    /// @param onPlay Called when the listening button is pressed.
    /// @param onSelect Called when the column is clicked.
    /// @param onActivate Called when the band is activated by keyboard or double click.
    /// @param onNavigate Called for arrow-key navigation between bands.
    /// @param onScroll Called for wheel scrolling of the strip.
    /// @param parent Parent widget.
    BandColumn(int index, const Band &band, const LevelScale &scale,
        const QHash<QString, QIcon> &icons, LevelHandler onLevel, IndexHandler onPlay,
        IndexHandler onSelect, IndexHandler onActivate, IndexHandler onNavigate,
        IndexHandler onScroll, QWidget *parent = nullptr);

    /// @brief Returns the band index.
    [[nodiscard]] int index() const noexcept { return index_; }
    /// @brief Returns the level slider of the column.
    [[nodiscard]] LevelSlider &slider() noexcept { return *slider_; }

    /// @brief Shows the level without generating a change signal.
    /// @param levelDb Level to display, dB.
    void setLevel(double levelDb);
    /// @brief Updates the playback frame and the listening button.
    /// @param current The column is the current band.
    /// @param playing There is active sound.
    void setCurrent(bool current, bool playing);
    /// @brief Dims the column while the master slider is focused.
    /// @param focused Whether the master slider owns the focus.
    void setMasterFocused(bool focused);
    /// @brief Replaces the slider and label scale.
    /// @param scale New scale.
    void setScale(const LevelScale &scale);

protected:
    /// @brief Selects the band and moves the focus to its slider.
    void mousePressEvent(QMouseEvent *event) override;
    /// @brief Resets the band level to 0 dB on a right click.
    void contextMenuEvent(QContextMenuEvent *event) override;
    /// @brief Enables the hover state and refreshes the labels.
    void enterEvent(QEnterEvent *event) override;
    /// @brief Clears the hover and dragging states.
    void leaveEvent(QEvent *event) override;
    /// @brief Draws the frame around the playing band.
    void paintEvent(QPaintEvent *event) override;

private:
    /// @brief Returns the current level shown by the slider, dB.
    [[nodiscard]] double level() const;
    /// @brief Refreshes the value and frequency labels.
    void updateText();
    /// @brief Refreshes the tooltip text.
    void updateTooltip();

    int index_; ///< Band index.
    LevelScale scale_; ///< Range and step of the slider.
    LevelHandler onLevel_; ///< Level-change callback.
    IndexHandler onPlay_; ///< Listening-button callback.
    IndexHandler onSelect_; ///< Selection callback.
    IndexHandler onActivate_; ///< Activation callback.
    IndexHandler onNavigate_; ///< Navigation callback.
    IndexHandler onScroll_; ///< Scroll callback.
    QHash<QString, QIcon> icons_; ///< Shared icon set.
    QLabel valueLabel_; ///< Level label above the slider.
    LevelSlider *slider_ = nullptr; ///< Level slider.
    QLabel *frequencyLabel_ = nullptr; ///< Frequency label below the slider.
    QPushButton *playButton_ = nullptr; ///< Listening button.
    bool current_ = false; ///< Whether this is the current band.
    bool playing_ = false; ///< Whether sound is active.
    bool hover_ = false; ///< Whether the mouse is over the column.
    bool editing_ = false; ///< Whether the slider is being dragged.
    bool masterFocused_ = false; ///< Whether the master slider is focused.
};

/// @brief Strip of equalizer bands with the ruler; placed in a scroll area.
class BandStrip final : public QWidget {
    Q_OBJECT

public:
    /// @brief Callback for a changed band level.
    using LevelHandler = std::function<void(int, double)>;
    /// @brief Callback for an action on a band index.
    using IndexHandler = std::function<void(int)>;

    /// @brief Creates an empty strip.
    /// @param scale Range and step of the level sliders.
    /// @param icons Icon set shared by all columns.
    /// @param onLevel Called when a band level changes.
    /// @param onPlay Called when a listening button is pressed.
    /// @param onSelect Called when a column is selected.
    /// @param onActivate Called when a band is activated.
    /// @param onNavigate Called for keyboard navigation between bands.
    /// @param onScroll Called for wheel scrolling.
    /// @param parent Parent widget.
    BandStrip(const LevelScale &scale, const QHash<QString, QIcon> &icons,
        LevelHandler onLevel, IndexHandler onPlay, IndexHandler onSelect,
        IndexHandler onActivate, IndexHandler onNavigate, IndexHandler onScroll,
        QWidget *parent = nullptr);

    /// @brief Rebuilds the columns for a new set of bands.
    /// @param bands Bands in ascending frequency order.
    /// @param scale Range and step of the level sliders.
    void setBands(const QVector<Band> &bands, const LevelScale &scale);
    /// @brief Highlights the current band.
    /// @param index Index of the current band.
    /// @param playing Whether sound is active.
    void setCurrent(int index, bool playing);
    /// @brief Dims all bands while the master slider is focused.
    /// @param focused Whether the master slider owns the focus.
    void setMasterFocused(bool focused);
    /// @brief Replaces the scale of all sliders and the ruler.
    /// @param scale New scale.
    void setScale(const LevelScale &scale);
    /// @brief Returns the column of a band.
    /// @param index Band index.
    [[nodiscard]] const BandColumn &column(int index) const;
    /// @brief Returns the column of a band.
    /// @param index Band index.
    [[nodiscard]] BandColumn &column(int index);

private:
    /// @brief Deletes all columns and the ruler from the layout.
    void clear();

    LevelScale scale_; ///< Range and step of the level sliders.
    QHash<QString, QIcon> icons_; ///< Shared icon set.
    LevelHandler onLevel_; ///< Level-change callback.
    IndexHandler onPlay_; ///< Listening-button callback.
    IndexHandler onSelect_; ///< Selection callback.
    IndexHandler onActivate_; ///< Activation callback.
    IndexHandler onNavigate_; ///< Navigation callback.
    IndexHandler onScroll_; ///< Scroll callback.
    QHBoxLayout *layout_; ///< Horizontal layout of the strip.
    QVector<BandColumn *> columns_; ///< Columns in band order.
    LevelRuler *ruler_ = nullptr; ///< Shared ruler before the first column.
    bool masterFocused_ = false; ///< Whether the master slider is focused.
};

} // namespace tp::ui
