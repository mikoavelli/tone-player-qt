// Main window: equalizer, transport and settings.
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

namespace tp::ui {

// Screen refresh interval.
inline constexpr int refreshIntervalMs = 50;
// Setting changes are written not on every slider step, but after a pause.
inline constexpr int saveDelayMs = 600;
// Service messages (for example “preset saved”) must not hang around forever.
inline constexpr int noticeTimeoutMs = 3500;
// External edits of settings.json are picked up after a short pause.
inline constexpr int settingsReloadDelayMs = 250;

// Font lists are ordered by preference: the first installed one is taken.
inline constexpr std::array<std::string_view, 5> sansFamilies = {
    "IBM Plex Sans", "Noto Sans", "Segoe UI", "Ubuntu", "DejaVu Sans"};
inline constexpr std::array<std::string_view, 5> monoFamilies = {
    "IBM Plex Mono", "JetBrains Mono", "Noto Sans Mono", "Consolas", "DejaVu Sans Mono"};

inline constexpr int baseFontPx = 14;
inline constexpr int frequencyValuePx = 36;
inline constexpr int frequencyUnitPx = 16;
inline constexpr int frequencyBlockWidth = 170;

// The interface stylesheet with theme-colored substitution.
QString buildStylesheet();

// Mode of the main button: what it does when the button is pressed.
enum class PlayButtonMode { Start, Pause, Resume };

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const Settings &settings, const std::filesystem::path &settingsPath,
        audio::TonePlayer *player = nullptr, QWidget *parent = nullptr);
    ~MainWindow() override;

    [[nodiscard]] audio::TonePlayer &player() noexcept { return *player_; }

    void closeEvent(QCloseEvent *event) override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    // ---- Building the interface -------------------------------------------------
    void buildUi();
    QLayout *buildHeader();
    QLayout *buildPlayback();
    QLayout *buildBands();
    QWidget *buildMaster();
    QPushButton *iconButton(const QString &iconKind, const QString &tooltip);
    QString frequencyHtml(const QString &value) const;
    QFont monoFont(int pixelSize, QFont::Weight weight = QFont::Weight::Medium) const;

    // ---- Handlers ---------------------------------------------------------------
    void onPlayPause();
    void onStop();
    void onAutoplayToggled(bool enabled);
    void onMasterChanged(int position);
    void onDeviceChanged(int index);
    void onPresetChanged(int index);
    void onSavePreset();
    void onDeletePreset();
    void onBandLevel(int index, double levelDb);
    void onBandSelect(int index);
    void onBandPlay(int index);
    void onActivateBand(int index);
    void onActivateCurrent();
    void navigate(int offset);
    void focusBand(int index);
    void scrollBands(int delta);
    void nudgeActiveBand(int direction);
    void openSettings();
    void refreshPresets();

    // ---- Settings storage -------------------------------------------------------
    void store(Settings settings);
    void showNotice(const QString &text);
    void clearNotice();
    void saveSettings();
    void onSettingsFileChanged(const QString &path);
    void reloadSettings();
    [[nodiscard]] QByteArray readSettingsFile() const;

    // ---- State update -----------------------------------------------------------
    void refresh();
    void applySettings(const Settings &settings, const QString &notice = {},
        bool persist = true);
    void rebuildBands();
    void syncBandBar();
    void syncMaster();
    void syncAutoplay();
    void syncDeviceSelector();
    [[nodiscard]] int focusedBandIndex() const;
    [[nodiscard]] bool bandSliderFocused() const;
    void updateAutoplayText(bool enabled);
    void startPlayback(int index);

    std::optional<QString> askPresetName(const QString &defaultName);

    Settings settings_;
    std::filesystem::path settingsPath_;
    QByteArray settingsFileBytes_;

    std::unique_ptr<audio::TonePlayer> ownedPlayer_;
    audio::TonePlayer *player_;

    tp::ToneConfig config_;
    QVector<audio::OutputDevice> devices_;

    mutable int focusedIndex_ = 0;
    int lastBandIndex_ = -1;
    QString notice_;
    QString saveError_;
    bool finishingHandled_ = false;
    std::optional<PlayButtonMode> playButtonMode_;

    // Interface.
    QHash<QString, QIcon> icons_;
    QPushButton *settingsButton_ = nullptr;
    QComboBox *presetSelector_ = nullptr;
    QPushButton *presetSaveButton_ = nullptr;
    QPushButton *presetDeleteButton_ = nullptr;
    QComboBox *deviceSelector_ = nullptr;
    QLabel *autoplayStateLabel_ = nullptr;
    LevelSlider *masterSlider_ = nullptr;
    MasterFrame *masterFrame_ = nullptr;
    QLabel *masterValueLabel_ = nullptr;
    QHBoxLayout *bandRow_ = nullptr;
    QPushButton *playButton_ = nullptr;
    QPushButton *stopButton_ = nullptr;
    ToggleSwitch *autoplayButton_ = nullptr;
    QLabel *frequencyLabel_ = nullptr;
    QLabel *toneCounterLabel_ = nullptr;
    QProgressBar *toneProgress_ = nullptr;
    QLabel *remainingLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;

    BandStrip *bands_ = nullptr;
    QScrollArea *bandScroll_ = nullptr;
    QScrollBar *bandBar_ = nullptr;
    LevelRuler *masterRuler_ = nullptr;

    QString sansFamily_;
    QString monoFamily_;

    // Timers.
    QTimer timer_;
    QTimer saveTimer_;
    QTimer noticeTimer_;
    QTimer reloadTimer_;
    QFileSystemWatcher *settingsWatcher_ = nullptr;
    QShortcut *spaceShortcut_ = nullptr;
    QShortcut *settingsShortcut_ = nullptr;
};

} // namespace tp::ui
