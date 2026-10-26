/// @file main_window.h
/// @brief Main window: equalizer, transport and settings.
#pragma once

#include "audio_engine.h"
#include "band_widgets.h"
#include "models.h"
#include "presets.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QTimer>
#include <array>
#include <filesystem>
#include <memory>
#include <optional>

class QComboBox;
class QPushButton;

/// @brief Main window and its widgets.
namespace tp::ui {

/// @brief Screen refresh interval, ms.
inline constexpr int refreshIntervalMs = 50;
/// @brief Setting changes are written not on every slider step, but after this pause, ms.
inline constexpr int saveDelayMs = 600;
/// @brief Service messages (for example “preset saved”) must not hang around forever, ms.
inline constexpr int noticeTimeoutMs = 3500;
/// @brief External edits of settings.json are picked up after this pause, ms.
inline constexpr int settingsReloadDelayMs = 250;

/// @brief Preferred interface font families, ordered by preference.
inline constexpr std::array<std::string_view, 5> sansFamilies = {
    "IBM Plex Sans", "Noto Sans", "Segoe UI", "Ubuntu", "DejaVu Sans"};
/// @brief Preferred monospace font families, ordered by preference.
inline constexpr std::array<std::string_view, 5> monoFamilies = {
    "IBM Plex Mono", "JetBrains Mono", "Noto Sans Mono", "Consolas", "DejaVu Sans Mono"};

/// @brief Base interface font size, px.
inline constexpr int baseFontPx = 14;
/// @brief Font size of the large frequency value, px.
inline constexpr int frequencyValuePx = 36;
/// @brief Font size of the frequency unit, px.
inline constexpr int frequencyUnitPx = 16;
/// @brief Width reserved for the frequency block, px.
inline constexpr int frequencyBlockWidth = 170;

/// @brief Returns the interface stylesheet with theme-colored substitution.
QString buildStylesheet();

/// @brief Mode of the main button: what it does when the button is pressed.
enum class PlayButtonMode {
    Start, ///< No session yet: start from the current band.
    Pause, ///< A session is playing: suspend it.
    Resume, ///< A session is paused: continue it.
};

/// @brief Main window: equalizer, transport, preset and device selectors, settings.
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    /// @brief Creates the window and builds the interface.
    /// @param settings Loaded and validated program settings.
    /// @param settingsPath Path of settings.json used for saving and watching.
    /// @param player Player to control; @c nullptr creates an owned player.
    /// @param parent Parent widget.
    MainWindow(const Settings &settings, const std::filesystem::path &settingsPath,
        audio::TonePlayer *player = nullptr, QWidget *parent = nullptr);
    /// @brief Destroys the window and its owned player.
    ~MainWindow() override;

    /// @brief Returns the player controlled by the window.
    [[nodiscard]] audio::TonePlayer &player() noexcept { return *player_; }

    /// @brief Saves pending changes before the window closes.
    /// @param event Close event.
    void closeEvent(QCloseEvent *event) override;

protected:
    /// @brief Handles global keyboard shortcuts of the window.
    /// @param event Key event.
    void keyPressEvent(QKeyEvent *event) override;

private:
    // ---- Building the interface -------------------------------------------------
    /// @brief Builds the whole window layout.
    void buildUi();
    /// @brief Builds the header with the preset and device selectors.
    QLayout *buildHeader();
    /// @brief Builds the transport controls and status line.
    QLayout *buildPlayback();
    /// @brief Builds the master column and the scrollable band strip.
    QLayout *buildBands();
    /// @brief Builds the master volume column.
    QWidget *buildMaster();
    /// @brief Creates a square icon button.
    /// @param iconKind Resource name of the icon.
    /// @param tooltip Tooltip and accessible name of the button.
    QPushButton *iconButton(const QString &iconKind, const QString &tooltip);
    /// @brief Returns the frequency block markup.
    /// @param value Frequency value text.
    QString frequencyHtml(const QString &value) const;
    /// @brief Returns the first installed monospace font.
    /// @param pixelSize Font size, px.
    /// @param weight Font weight.
    QFont monoFont(int pixelSize, QFont::Weight weight = QFont::Weight::Medium) const;

    // ---- Handlers ---------------------------------------------------------------
    /// @brief Starts, pauses or resumes playback depending on the player state.
    void onPlayPause();
    /// @brief Stops playback.
    void onStop();
    /// @brief Applies the autoplay switch.
    /// @param enabled New autoplay state.
    void onAutoplayToggled(bool enabled);
    /// @brief Applies a new master slider position.
    /// @param position Slider position.
    void onMasterChanged(int position);
    /// @brief Switches the output device.
    /// @param index Index in the device selector.
    void onDeviceChanged(int index);
    /// @brief Applies the selected preset.
    /// @param index Index in the preset selector.
    void onPresetChanged(int index);
    /// @brief Asks for a name and saves the current levels as a preset.
    void onSavePreset();
    /// @brief Deletes the selected preset after confirmation.
    void onDeletePreset();
    /// @brief Applies a band level change.
    /// @param index Band index.
    /// @param levelDb New level offset, dB.
    void onBandLevel(int index, double levelDb);
    /// @brief Selects a band without starting sound.
    /// @param index Band index.
    void onBandSelect(int index);
    /// @brief Starts playback of one band.
    /// @param index Band index.
    void onBandPlay(int index);
    /// @brief Activates the band: selects it and starts sound.
    /// @param index Band index.
    void onActivateBand(int index);
    /// @brief Activates the currently focused band.
    void onActivateCurrent();
    /// @brief Moves the focus between bands and the master slider.
    /// @param offset Direction and distance.
    void navigate(int offset);
    /// @brief Gives the keyboard focus to a band.
    /// @param index Band index.
    void focusBand(int index);
    /// @brief Scrolls the band strip by a pixel delta.
    /// @param delta Horizontal delta, px.
    void scrollBands(int delta);
    /// @brief Changes the level of the focused band from the keyboard.
    /// @param direction −1 down, +1 up.
    void nudgeActiveBand(int direction);
    /// @brief Opens the modal settings dialog and applies its result.
    void openSettings();
    /// @brief Rebuilds the preset selector from the current settings.
    void refreshPresets();

    // ---- Settings storage -------------------------------------------------------
    /// @brief Replaces the settings, updates the interface and schedules saving.
    /// @param settings New settings.
    void store(Settings settings);
    /// @brief Shows a temporary service message in the status line.
    /// @param text Message text.
    void showNotice(const QString &text);
    /// @brief Clears a temporary service message.
    void clearNotice();
    /// @brief Writes settings.json if it changed.
    void saveSettings();
    /// @brief Handles an external change of settings.json.
    /// @param path Path reported by the file watcher.
    void onSettingsFileChanged(const QString &path);
    /// @brief Reloads settings.json changed outside the program.
    void reloadSettings();
    /// @brief Returns the current content of settings.json.
    [[nodiscard]] QByteArray readSettingsFile() const;

    // ---- State update -----------------------------------------------------------
    /// @brief Refreshes transport labels, progress and the current band.
    void refresh();
    /// @brief Applies settings completely and optionally writes them.
    /// @param settings Settings to apply.
    /// @param notice Optional service message.
    /// @param persist Whether to save the settings file.
    void applySettings(const Settings &settings, const QString &notice = {},
        bool persist = true);
    /// @brief Recreates the band strip for the current settings.
    void rebuildBands();
    /// @brief Synchronizes the external scrollbar with the strip scrollbar.
    void syncBandBar();
    /// @brief Synchronizes the master slider and its label with the player.
    void syncMaster();
    /// @brief Synchronizes the autoplay switch and its label.
    void syncAutoplay();
    /// @brief Rebuilds the device selector from the available devices.
    void syncDeviceSelector();
    /// @brief Returns the index of the band that owns the keyboard focus.
    [[nodiscard]] int focusedBandIndex() const;
    /// @brief Tells whether any band slider owns the keyboard focus.
    [[nodiscard]] bool bandSliderFocused() const;
    /// @brief Updates the autoplay state label.
    /// @param enabled Autoplay state.
    void updateAutoplayText(bool enabled);
    /// @brief Starts or re-tunes playback from a band.
    /// @param index Band index.
    void startPlayback(int index);

    /// @brief Asks the user for a preset name.
    /// @param defaultName Name suggested in the dialog.
    /// @return Trimmed name, or std::nullopt if the dialog was cancelled.
    std::optional<QString> askPresetName(const QString &defaultName);

    Settings settings_; ///< Current program settings.
    std::filesystem::path settingsPath_; ///< Path of settings.json.
    QByteArray settingsFileBytes_; ///< Content of settings.json at the last read.

    std::unique_ptr<audio::TonePlayer> ownedPlayer_; ///< Player created by the window, if any.
    audio::TonePlayer *player_; ///< Player controlled by the window.

    tp::ToneConfig config_; ///< Playback configuration derived from the settings.
    QVector<audio::OutputDevice> devices_; ///< Output devices present at startup.

    mutable int focusedIndex_ = 0; ///< Last focused band index.
    int lastBandIndex_ = -1; ///< Band shown as current at the last refresh.
    QString notice_; ///< Temporary service message.
    QString saveError_; ///< Last settings save error.
    bool finishingHandled_ = false; ///< Whether the end of the session was processed.
    std::optional<PlayButtonMode> playButtonMode_; ///< Last applied main-button mode.

    // Interface.
    QHash<QString, QIcon> icons_; ///< Icon set shared by all widgets.
    QPushButton *settingsButton_ = nullptr; ///< Button that opens settings.
    QComboBox *presetSelector_ = nullptr; ///< Preset selector.
    QPushButton *presetSaveButton_ = nullptr; ///< “Save preset” button.
    QPushButton *presetDeleteButton_ = nullptr; ///< “Delete preset” button.
    QComboBox *deviceSelector_ = nullptr; ///< Output device selector.
    QLabel *autoplayStateLabel_ = nullptr; ///< Text next to the autoplay switch.
    LevelSlider *masterSlider_ = nullptr; ///< Master volume slider.
    MasterFrame *masterFrame_ = nullptr; ///< Frame of the master column.
    QLabel *masterValueLabel_ = nullptr; ///< Master level label.
    QHBoxLayout *bandRow_ = nullptr; ///< Row with the master column and the band strip.
    QPushButton *playButton_ = nullptr; ///< Main play/pause button.
    QPushButton *stopButton_ = nullptr; ///< Stop button.
    ToggleSwitch *autoplayButton_ = nullptr; ///< Autoplay switch.
    QLabel *frequencyLabel_ = nullptr; ///< Current frequency display.
    QLabel *toneCounterLabel_ = nullptr; ///< “Tone n of m” label.
    QProgressBar *toneProgress_ = nullptr; ///< Progress of the current tone.
    QLabel *remainingLabel_ = nullptr; ///< Remaining time label.
    QLabel *statusLabel_ = nullptr; ///< Status and service messages.

    BandStrip *bands_ = nullptr; ///< Current band strip.
    QScrollArea *bandScroll_ = nullptr; ///< Scroll area of the band strip.
    QScrollBar *bandBar_ = nullptr; ///< External horizontal scrollbar under the strip.
    LevelRuler *masterRuler_ = nullptr; ///< Ruler of the master column.

    QString sansFamily_; ///< Selected interface font family.
    QString monoFamily_; ///< Selected monospace font family.

    // Timers.
    QTimer timer_; ///< Periodic interface refresh.
    QTimer saveTimer_; ///< Debounce of settings writing.
    QTimer noticeTimer_; ///< Lifetime of a service message.
    QTimer reloadTimer_; ///< Debounce of external settings reloading.
    QFileSystemWatcher *settingsWatcher_ = nullptr; ///< Watcher of settings.json.
    QShortcut *spaceShortcut_ = nullptr; ///< Space: play/pause.
    QShortcut *settingsShortcut_ = nullptr; ///< Ctrl+,: open settings.
};

} // namespace tp::ui
