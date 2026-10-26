// Streaming generator of control tones.
//
// The sound is formed without external audio libraries: the sine comes from a
// table, the output device is opened via QAudioSink (Qt Multimedia). The push
// mode is used: a timer on the interface thread slightly ahead of time fills
// the buffer of the device.
#pragma once

#include "models.h"

#include <QTimer>
#include <QVector>
#include <QAudioDevice>
#include <memory>
#include <optional>

class QAudioSink;
class QIODevice;

namespace tp::audio {

inline constexpr int channels = 1;
inline constexpr int fallbackChannels = 2;

// Size of the sine table — a power of two; the phase transition uses a mask.
inline constexpr int tableSize = 8192;
inline constexpr int tableMask = tableSize - 1;

// Descriptor of an output device.
//
// The index of the device in the system list changes between runs, therefore
// the settings store the label: the human name of the device.
struct OutputDevice {
    int index = 0;
    QString name;

    // Label of the device in the interface and in the settings.
    [[nodiscard]] QString label() const { return name; }

    [[nodiscard]] bool operator==(const OutputDevice &other) const = default;
};

// List of output devices available to the system.
QVector<OutputDevice> listOutputDevices();
// Device with the label label; std::nullopt, if not requested or not found.
std::optional<OutputDevice> findDevice(const QVector<OutputDevice> &devices,
    const std::optional<QString> &label);

// Sine signal formation: frequency, band level, smooth transitions.
//
// The tone level in dB from full scale is the overall level plus the band
// offset.
class ToneGenerator {
public:
    ToneGenerator(ToneConfig config, QVector<double> levels, double masterDb,
        bool autoplayEnabled = true, LevelScale levelScale = defaultLevelScale);

    [[nodiscard]] const ToneConfig &config() const noexcept { return config_; }

    [[nodiscard]] double masterDb() const noexcept { return masterDb_; }

    // Tone duration in frames.
    [[nodiscard]] qint64 toneLengthFrames() const;

    // Level offsets of all bands, dB.
    [[nodiscard]] QVector<double> levels() const;

    // Range and step of the level offsets.
    [[nodiscard]] LevelScale levelScale() const noexcept { return levelScale_; }

    // Band level offset by index, dB.
    [[nodiscard]] double levelAt(int index) const;

    // Change the band level offset, dB; the sounding tone changes immediately.
    void setLevelAt(int index, double levelDb);

    // Final level of the current tone: overall level plus band offset, dB.
    [[nodiscard]] double currentGainDb() const;

    // Amplitude of the current tone, 0.0..1.0.
    [[nodiscard]] double amplitude() const;

    // Can the next frequency start automatically.
    [[nodiscard]] bool advanceAllowed() const noexcept { return autoplayEnabled_; }

    // Allow (or forbid) auto-play of the next frequency.
    void setAutoplay(bool enabled) noexcept { autoplayEnabled_ = enabled; }

    // Duration of ramp up and ramp down in frames.
    [[nodiscard]] qint64 fadeFrames() const;

    // Change the overall level, dB from full scale.
    void setMasterDb(double masterDb);

    // Return to the start of the sequence.
    void reset();

    // Move to a tone by index without playing it.
    void seek(int index);

    // Form frames of the signal and move the playback position.
    QVector<float> render(qint64 frames);

    int index = 0;
    qint64 framesInTone = 0;
    bool finished = false;

private:
    void renderTone(QVector<float> &block, qint64 start, qint64 count);
    void advance() noexcept;

    ToneConfig config_;
    bool autoplayEnabled_ = true;
    double masterDb_ = 0.0;
    LevelScale levelScale_;
    // Levels are edited from the interface during sounding, therefore they
    // live here, not in an immutable ToneConfig.
    QVector<double> levels_;
    double phase_ = 0.0;

    std::array<float, tableSize> sineTable_;
};

// Playback of a tone sequence with the given frequencies.
//
// The playback position is counted by the number of frames issued, therefore
// pause does not shift the timer and does not depend on the accuracy of the
// system clock. The device buffer is filled on the interface thread by a
// timer, with a small jump ahead so that GUI pauses do not lead to a dropout.
class TonePlayer {
public:
    TonePlayer(const ToneConfig &config, const QVector<double> &levels, double masterDb,
        LevelScale levelScale = defaultLevelScale);
    ~TonePlayer();

    TonePlayer(const TonePlayer &) = delete;
    TonePlayer &operator=(const TonePlayer &) = delete;
    TonePlayer(TonePlayer &&) = delete;
    TonePlayer &operator=(TonePlayer &&) = delete;

    [[nodiscard]] const ToneConfig &config() const noexcept { return config_; }
    [[nodiscard]] const ToneGenerator &generator() const noexcept { return *generator_; }
    [[nodiscard]] PlayerState state() const noexcept { return state_; }
    [[nodiscard]] double masterDb() const;
    // Index of the selected tone: current during playback, else chosen manually.
    [[nodiscard]] int toneIndex() const;
    // Frequency of the selected tone, Hz.
    [[nodiscard]] double currentFrequency() const;
    // Final level of the selected tone, dB.
    [[nodiscard]] double currentGainDb() const;
    // Level offsets of all bands, dB.
    [[nodiscard]] QVector<double> levels() const;
    [[nodiscard]] double levelAt(int index) const;
    // Change the band level offset; the sounding tone changes immediately.
    void setLevelAt(int index, double levelDb);
    // Change the overall level, dB from full scale.
    void setMasterDb(double masterDb);
    // Replace frequencies, durations and levels; only when there is no sound.
    void reconfigure(const ToneConfig &config, const QVector<double> &levels,
        LevelScale levelScale);
    // Select the output device for the next start (by label).
    void setDevice(const std::optional<QString> &deviceLabel);
    // Label of the selected output device.
    [[nodiscard]] std::optional<QString> deviceLabel() const noexcept { return deviceLabel_; }
    // Allow (or forbid) auto-play of the next frequency.
    void setAutoplay(bool enabled);
    // Last stream message, if there was one.
    [[nodiscard]] const QString &lastStatus() const noexcept { return lastStatus_; }

    // Start or re-tune playback from the tone index.
    void start(int index = 0);
    // Move to a tone by index without playback.
    void select(int index);
    // Suspend playback.
    void pause() noexcept;
    // Resume playback after a pause.
    void resume() noexcept;
    // Stop playback.
    void stop() noexcept;
    // Free the output device, keeping the final session state.
    void finish() noexcept;

    // Current playback position.
    [[nodiscard]] PlaybackProgress progress() const;

private:
    void openStream();
    void closeStream() noexcept;
    void fillFromTimer();

    ToneConfig config_;
    std::unique_ptr<ToneGenerator> generator_;
    PlayerState state_ = PlayerState::Idle;
    std::optional<QString> deviceLabel_;

    std::unique_ptr<QAudioSink> sink_;
    QIODevice *sinkPort_ = nullptr;
    int sinkChannels_ = channels;
    QTimer feedTimer_;
    int feedFrames_ = 0;

    QString lastStatus_;
    QList<QAudioDevice> devices_;
};

} // namespace tp::audio
