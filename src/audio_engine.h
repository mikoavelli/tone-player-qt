/// @file audio_engine.h
/// @brief Streaming generator of control tones.
///
/// The sound is formed without external audio libraries: the sine comes from a
/// table, the output device is opened via QAudioSink (Qt Multimedia). The push
/// mode is used: a timer on the interface thread slightly ahead of time fills
/// the buffer of the device.
#pragma once

#include "models.h"

#include <QTimer>
#include <QVector>
#include <QAudioDevice>
#include <memory>
#include <optional>

class QAudioSink;
class QIODevice;

/// @brief Audio engine: signal generation and playback on an output device.
namespace tp::audio {

/// @brief Number of channels requested from the device first.
inline constexpr int channels = 1;
/// @brief Stereo fallback if the device cannot open a mono stream.
inline constexpr int fallbackChannels = 2;

/// @brief Size of the sine table — a power of two; the phase transition uses a mask.
inline constexpr int tableSize = 8192;
/// @brief Mask for reducing the phase to the table index range.
inline constexpr int tableMask = tableSize - 1;

/// @brief Descriptor of an output device.
///
/// The index of the device in the system list changes between runs, therefore
/// the settings store the label: the human name of the device.
struct OutputDevice {
    int index = 0; ///< Index in the system device list.
    QString name; ///< Human-readable device name.

    /// @brief Returns the label of the device in the interface and in the settings.
    [[nodiscard]] QString label() const { return name; }

    /// @brief Compares the index and the name of two devices.
    [[nodiscard]] bool operator==(const OutputDevice &other) const = default;
};

/// @brief Returns the output devices available to the system.
QVector<OutputDevice> listOutputDevices();

/// @brief Finds a device by its label.
/// @param devices Devices to search.
/// @param label Label to find; empty means “not requested”.
/// @return The matching device, or std::nullopt if the label is empty or unknown.
std::optional<OutputDevice> findDevice(const QVector<OutputDevice> &devices,
    const std::optional<QString> &label);

/// @brief Sine signal formation: frequency, band level, smooth transitions.
///
/// The tone level in dB from full scale is the overall level plus the band
/// offset.
class ToneGenerator {
public:
    /// @brief Creates a generator positioned at the start of the sequence.
    /// @param config Frequencies, tone duration, sample rate and fade.
    /// @param levels Initial band level offsets, dB.
    /// @param masterDb Overall level, dB from full scale.
    /// @param autoplayEnabled Whether the next frequency may start automatically.
    /// @param levelScale Range and step of the level offsets.
    ToneGenerator(ToneConfig config, QVector<double> levels, double masterDb,
        bool autoplayEnabled = true, LevelScale levelScale = defaultLevelScale);

    /// @brief Returns the playback configuration.
    [[nodiscard]] const ToneConfig &config() const noexcept { return config_; }

    /// @brief Returns the overall level, dB from full scale.
    [[nodiscard]] double masterDb() const noexcept { return masterDb_; }

    /// @brief Returns the tone duration in frames.
    [[nodiscard]] qint64 toneLengthFrames() const;

    /// @brief Returns the level offsets of all bands, dB.
    [[nodiscard]] QVector<double> levels() const;

    /// @brief Returns the range and step of the level offsets.
    [[nodiscard]] LevelScale levelScale() const noexcept { return levelScale_; }

    /// @brief Returns the band level offset by index, dB.
    /// @param index Band index.
    [[nodiscard]] double levelAt(int index) const;

    /// @brief Changes the band level offset; the sounding tone changes immediately.
    /// @param index Band index.
    /// @param levelDb New level offset, dB.
    void setLevelAt(int index, double levelDb);

    /// @brief Returns the final level of the current tone: overall level plus band offset, dB.
    [[nodiscard]] double currentGainDb() const;

    /// @brief Returns the amplitude of the current tone, 0.0..1.0.
    [[nodiscard]] double amplitude() const;

    /// @brief Tells whether the next frequency may start automatically.
    [[nodiscard]] bool advanceAllowed() const noexcept { return autoplayEnabled_; }

    /// @brief Allows or forbids auto-play of the next frequency.
    /// @param enabled New autoplay state.
    void setAutoplay(bool enabled) noexcept { autoplayEnabled_ = enabled; }

    /// @brief Returns the duration of ramp up and ramp down in frames.
    [[nodiscard]] qint64 fadeFrames() const;

    /// @brief Changes the overall level, dB from full scale.
    /// @param masterDb New overall level.
    void setMasterDb(double masterDb);

    /// @brief Returns to the start of the sequence.
    void reset();

    /// @brief Moves to a tone by index without playing it.
    /// @param index Target tone index.
    void seek(int index);

    /// @brief Forms frames of the signal and moves the playback position.
    /// @param frames Number of frames to render.
    /// @return Mono samples in the range −1.0..1.0.
    QVector<float> render(qint64 frames);

    int index = 0; ///< Index of the current tone.
    qint64 framesInTone = 0; ///< Frames already rendered in the current tone.
    bool finished = false; ///< Whether the last tone has been played completely.

private:
    /// @brief Renders a part of the current tone into a block.
    void renderTone(QVector<float> &block, qint64 start, qint64 count);
    /// @brief Moves to the next tone or finishes the sequence.
    void advance() noexcept;

    ToneConfig config_; ///< Playback configuration.
    bool autoplayEnabled_ = true; ///< Whether the sequence advances by itself.
    double masterDb_ = 0.0; ///< Overall level, dB from full scale.
    LevelScale levelScale_; ///< Range and step of the level offsets.
    /// Levels are edited from the interface during sounding, therefore they
    /// live here, not in an immutable ToneConfig.
    QVector<double> levels_;
    double phase_ = 0.0; ///< Current phase in table indices.

    std::array<float, tableSize> sineTable_; ///< Precomputed sine values.
};

/// @brief Playback of a tone sequence with the given frequencies.
///
/// The playback position is counted by the number of frames issued, therefore
/// pause does not shift the timer and does not depend on the accuracy of the
/// system clock. The device buffer is filled on the interface thread by a
/// timer, with a small jump ahead so that GUI pauses do not lead to a dropout.
class TonePlayer {
public:
    /// @brief Creates a player without opening the audio device.
    /// @param config Frequencies, tone duration, sample rate and fade.
    /// @param levels Initial band level offsets, dB.
    /// @param masterDb Overall level, dB from full scale.
    /// @param levelScale Range and step of the level offsets.
    TonePlayer(const ToneConfig &config, const QVector<double> &levels, double masterDb,
        LevelScale levelScale = defaultLevelScale);
    /// @brief Stops playback and releases the device.
    ~TonePlayer();

    TonePlayer(const TonePlayer &) = delete;
    TonePlayer &operator=(const TonePlayer &) = delete;
    TonePlayer(TonePlayer &&) = delete;
    TonePlayer &operator=(TonePlayer &&) = delete;

    /// @brief Returns the playback configuration.
    [[nodiscard]] const ToneConfig &config() const noexcept { return config_; }
    /// @brief Returns the tone generator.
    [[nodiscard]] const ToneGenerator &generator() const noexcept { return *generator_; }
    /// @brief Returns the current player state.
    [[nodiscard]] PlayerState state() const noexcept { return state_; }
    /// @brief Returns the overall level, dB from full scale.
    [[nodiscard]] double masterDb() const;
    /// @brief Returns the index of the selected tone: current during playback, else chosen manually.
    [[nodiscard]] int toneIndex() const;
    /// @brief Returns the frequency of the selected tone, Hz.
    [[nodiscard]] double currentFrequency() const;
    /// @brief Returns the final level of the selected tone, dB.
    [[nodiscard]] double currentGainDb() const;
    /// @brief Returns the level offsets of all bands, dB.
    [[nodiscard]] QVector<double> levels() const;
    /// @brief Returns the band level offset by index, dB.
    [[nodiscard]] double levelAt(int index) const;
    /// @brief Changes the band level offset; the sounding tone changes immediately.
    void setLevelAt(int index, double levelDb);
    /// @brief Changes the overall level, dB from full scale.
    void setMasterDb(double masterDb);
    /// @brief Replaces frequencies, durations and levels; only when there is no sound.
    void reconfigure(const ToneConfig &config, const QVector<double> &levels,
        LevelScale levelScale);
    /// @brief Selects the output device for the next start by its label.
    /// @param deviceLabel Device name; empty selects the default device.
    void setDevice(const std::optional<QString> &deviceLabel);
    /// @brief Returns the label of the selected output device.
    [[nodiscard]] std::optional<QString> deviceLabel() const noexcept { return deviceLabel_; }
    /// @brief Allows or forbids auto-play of the next frequency.
    void setAutoplay(bool enabled);
    /// @brief Returns the last stream message, if there was one.
    [[nodiscard]] const QString &lastStatus() const noexcept { return lastStatus_; }

    /// @brief Starts or re-tunes playback from the tone index.
    /// @param index Tone to start from.
    void start(int index = 0);
    /// @brief Moves to a tone by index without playback.
    void select(int index);
    /// @brief Suspends playback, keeping the position.
    void pause() noexcept;
    /// @brief Resumes playback after a pause.
    void resume() noexcept;
    /// @brief Stops playback and returns to the start.
    void stop() noexcept;
    /// @brief Frees the output device, keeping the final session state.
    void finish() noexcept;

    /// @brief Returns the current playback position.
    [[nodiscard]] PlaybackProgress progress() const;

private:
    /// @brief Opens the audio device and starts the feed timer.
    void openStream();
    /// @brief Stops the feed timer and closes the audio device.
    void closeStream() noexcept;
    /// @brief Pushes the next portion of samples into the device buffer.
    void fillFromTimer();

    ToneConfig config_; ///< Playback configuration.
    std::unique_ptr<ToneGenerator> generator_; ///< Signal source.
    PlayerState state_ = PlayerState::Idle; ///< Current player state.
    std::optional<QString> deviceLabel_; ///< Selected output device label.

    std::unique_ptr<QAudioSink> sink_; ///< Open audio sink, if any.
    QIODevice *sinkPort_ = nullptr; ///< Writable end of the sink.
    int sinkChannels_ = channels; ///< Channels used by the open stream.
    QTimer feedTimer_; ///< Timer that fills the device buffer.
    int feedFrames_ = 0; ///< Frames pushed per timer tick.

    QString lastStatus_; ///< Last error or informational message.
    QList<QAudioDevice> devices_; ///< Output devices present at open time.
};

} // namespace tp::audio
