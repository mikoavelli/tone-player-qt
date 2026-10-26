// Streaming generator of control tones. See audio_engine.h for details.
#include "audio_engine.h"

#include "errors.h"
#include "i18n.h"
#include "text_format.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QtGlobal>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

namespace tp::audio {

using ::AudioError;

namespace {

using tr_ns::tr;

double checkedLevel(double levelDb, const LevelScale &scale)
{
    if (!(scale.minDb <= levelDb && levelDb <= scale.maxDb)) {
        throw AudioError(tr("err.levelOutOfRange", text_format::generalNumber(levelDb),
            text_format::generalNumber(scale.minDb), text_format::generalNumber(scale.maxDb)));
    }
    return levelDb;
}

double checkedMaster(double masterDb)
{
    if (!(masterMinDb <= masterDb && masterDb <= masterMaxDb)) {
        throw AudioError(tr("err.masterOutOfRange", text_format::generalNumber(masterDb),
            text_format::generalNumber(masterMinDb), text_format::generalNumber(masterMaxDb)));
    }
    return masterDb;
}

// Fill the internal sine table once.
std::array<float, tableSize> makeSineTable()
{
    std::array<float, tableSize> table = {};
    for (std::size_t index = 0; index < tableSize; ++index) {
        table[index] = static_cast<float>(
            std::sin(2.0 * std::numbers::pi_v<double>
                * static_cast<double>(index) / static_cast<double>(tableSize)));
    }
    return table;
}

QAudioFormat makeFormat(int sampleRate, int count)
{
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(count);
    format.setSampleFormat(QAudioFormat::Float);
    return format;
}

} // namespace

// -----------------------------------------------------------------------------
// Devices
// -----------------------------------------------------------------------------

QVector<OutputDevice> listOutputDevices()
{
    QVector<OutputDevice> devices;
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    int index = 0;
    for (const QAudioDevice &device : outputs) {
        QString name = device.description();
        if (name.isEmpty()) {
            name = tr("audio.deviceNumber", index);
        }
        devices.push_back(OutputDevice{index, name});
        ++index;
    }
    return devices;
}

std::optional<OutputDevice> findDevice(const QVector<OutputDevice> &devices,
    const std::optional<QString> &label)
{
    if (!label.has_value()) {
        return std::nullopt;
    }
    for (const OutputDevice &device : devices) {
        if (device.label() == label.value()) {
            return device;
        }
    }
    return std::nullopt;
}

// -----------------------------------------------------------------------------
// ToneGenerator
// -----------------------------------------------------------------------------

ToneGenerator::ToneGenerator(ToneConfig config, QVector<double> levels, double masterDb,
    bool autoplayEnabled, LevelScale levelScale)
    : config_(std::move(config))
    , autoplayEnabled_(autoplayEnabled)
    , masterDb_(checkedMaster(masterDb))
    , levelScale_(levelScale)
    , levels_(std::move(levels))
    , sineTable_(makeSineTable())
{
    if (levels_.size() != config_.toneCount()) {
        throw AudioError(tr("err.noneLevelsCount"));
    }
    for (double &level : levels_) {
        level = checkedLevel(level, levelScale_);
    }
}

qint64 ToneGenerator::toneLengthFrames() const
{
    return std::max(
        static_cast<qint64>(std::lround(config_.secondsPerTone
            * static_cast<double>(config_.sampleRate))),
        static_cast<qint64>(1));
}

QVector<double> ToneGenerator::levels() const
{
    return levels_;
}

double ToneGenerator::levelAt(int position) const
{
    return levels_.at(position);
}

void ToneGenerator::setLevelAt(int position, double levelDb)
{
    if (position < 0 || position >= levels_.size()) {
        throw AudioError(tr("err.bandNotFound", position + 1));
    }
    levels_[position] = checkedLevel(levelDb, levelScale_);
}

double ToneGenerator::currentGainDb() const
{
    const auto boundIndex = static_cast<int>(std::min<qint64>(index, levels_.size() - 1));
    return masterDb_ + levelAt(boundIndex);
}

double ToneGenerator::amplitude() const
{
    return std::min(std::pow(10.0, currentGainDb() / 20.0), 1.0);
}

qint64 ToneGenerator::fadeFrames() const
{
    const auto fade = static_cast<qint64>(std::lround(
        config_.fadeSeconds() * static_cast<double>(config_.sampleRate)));
    return std::max(std::min(fade, toneLengthFrames() / 2), static_cast<qint64>(1));
}

void ToneGenerator::setMasterDb(double masterDb)
{
    masterDb_ = checkedMaster(masterDb);
}

void ToneGenerator::reset()
{
    seek(0);
}

void ToneGenerator::seek(int newIndex)
{
    index = std::clamp(newIndex, 0, config_.toneCount() - 1);
    framesInTone = 0;
    finished = false;
    phase_ = 0.0;
}

void ToneGenerator::advance() noexcept
{
    ++index;
    framesInTone = 0;
    phase_ = 0.0;
    if (index >= config_.toneCount()) {
        finished = true;
    }
}

QVector<float> ToneGenerator::render(qint64 frames)
{
    QVector<float> block(static_cast<int>(frames), 0.0F);
    qint64 written = 0;
    while (written < frames && !finished) {
        if (framesInTone >= toneLengthFrames()) {
            if (advanceAllowed()) {
                advance();
            } else {
                // Auto-play is off: the next frequency does not begin.
                finished = true;
            }
            continue;
        }
        const qint64 count =
            std::min(frames - written, toneLengthFrames() - framesInTone);
        renderTone(block, written, count);
        written += count;
        framesInTone += count;
    }
    if (!finished && framesInTone >= toneLengthFrames()) {
        if (advanceAllowed()) {
            advance();
        } else {
            finished = true;
        }
    }
    return block;
}

void ToneGenerator::renderTone(QVector<float> &block, qint64 start, qint64 count)
{
    const double rate = static_cast<double>(config_.sampleRate);
    const double step =
        static_cast<double>(tableSize) * config_.frequencyAt(index) / rate;
    const double amplitudeValue = amplitude();
    const qint64 fade = fadeFrames();
    const qint64 total = toneLengthFrames();
    const qint64 first = framesInTone;
    const double pi = std::numbers::pi_v<double>;
    const std::array<float, tableSize> &table = sineTable_;

    double phase = phase_;
    for (qint64 offset = 0; offset < count; ++offset) {
        const qint64 position = first + offset;
        double gain = amplitudeValue;
        if (position < fade) {
            gain = amplitudeValue * 0.5
                * (1.0 - std::cos(pi * static_cast<double>(position)
                    / static_cast<double>(fade)));
        } else if (position > total - fade) {
            gain = amplitudeValue * 0.5
                * (1.0 - std::cos(pi * static_cast<double>(total - position)
                    / static_cast<double>(fade)));
        }
        block[start + offset] = static_cast<float>(
            static_cast<double>(table[static_cast<size_t>(phase) & tableMask]) * gain);
        phase += step;
    }

    phase_ = std::fmod(phase, static_cast<double>(tableSize));
}

// -----------------------------------------------------------------------------
// TonePlayer
// -----------------------------------------------------------------------------

TonePlayer::TonePlayer(const ToneConfig &config, const QVector<double> &levels,
    double masterDb, LevelScale levelScale)
    : config_(config)
    , generator_(std::make_unique<ToneGenerator>(config, levels, masterDb, true, levelScale))
{
    feedTimer_.setTimerType(Qt::TimerType::PreciseTimer);
    feedTimer_.setInterval(20);
    QObject::connect(&feedTimer_, &QTimer::timeout, [this] { fillFromTimer(); });
}

TonePlayer::~TonePlayer()
{
    closeStream();
}

double TonePlayer::masterDb() const
{
    return generator_->masterDb();
}

int TonePlayer::toneIndex() const
{
    return std::min(generator_->index, config_.toneCount() - 1);
}

double TonePlayer::currentFrequency() const
{
    return config_.frequencyAt(toneIndex());
}

double TonePlayer::currentGainDb() const
{
    return generator_->currentGainDb();
}

QVector<double> TonePlayer::levels() const
{
    return generator_->levels();
}

double TonePlayer::levelAt(int index) const
{
    return generator_->levelAt(index);
}

void TonePlayer::setLevelAt(int index, double levelDb)
{
    generator_->setLevelAt(index, levelDb);
}

void TonePlayer::setMasterDb(double masterDb)
{
    generator_->setMasterDb(masterDb);
}

void TonePlayer::reconfigure(const ToneConfig &config, const QVector<double> &levels,
    LevelScale levelScale)
{
    if (state_ == PlayerState::Playing || state_ == PlayerState::Paused) {
        throw AudioError(tr("err.reconfigurePlaying"));
    }
    auto replacement = std::make_unique<ToneGenerator>(
        config, levels, generator_->masterDb(), generator_->advanceAllowed(), levelScale);
    closeStream();
    const int keepIndex = toneIndex();
    config_ = config;
    generator_ = std::move(replacement);
    generator_->seek(keepIndex);
    state_ = PlayerState::Idle;
}

void TonePlayer::setDevice(const std::optional<QString> &label)
{
    if (state_ == PlayerState::Playing || state_ == PlayerState::Paused) {
        throw AudioError(tr("err.deviceChangedPlaying"));
    }
    deviceLabel_ = label;
}

void TonePlayer::setAutoplay(bool enabled)
{
    generator_->setAutoplay(enabled);
}

void TonePlayer::start(int index)
{
    lastStatus_.clear();

    if ((state_ == PlayerState::Playing || state_ == PlayerState::Paused)
        && sink_ != nullptr) {
        // The stream is already open: the generator just moves to the tone.
        generator_->seek(index);
        return;
    }

    closeStream();
    generator_->seek(index);
    openStream();
    state_ = PlayerState::Playing;
    feedTimer_.start();
}

void TonePlayer::select(int index)
{
    if (state_ == PlayerState::Paused) {
        generator_->seek(index);
        return;
    }
    if (state_ == PlayerState::Playing) {
        closeStream();
    }
    generator_->seek(index);
    state_ = PlayerState::Idle;
}

void TonePlayer::pause() noexcept
{
    if (state_ == PlayerState::Playing) {
        state_ = PlayerState::Paused;
        // Stop the device immediately; the queued tail stays for a clean
        // continue on resume.
        if (sink_ != nullptr) {
            sink_->suspend();
        }
    }
}

void TonePlayer::resume() noexcept
{
    if (state_ == PlayerState::Paused) {
        state_ = PlayerState::Playing;
        if (sink_ != nullptr) {
            sink_->resume();
        }
    }
}

void TonePlayer::stop() noexcept
{
    closeStream();
    generator_->reset();
    state_ = PlayerState::Idle;
}

void TonePlayer::finish() noexcept
{
    // Free the output device, keeping the final session state.
    closeStream();
}

PlaybackProgress TonePlayer::progress() const
{
    const int currentIndex = toneIndex();
    double toneElapsed = static_cast<double>(generator_->framesInTone)
        / static_cast<double>(config_.sampleRate);
    if (state_ == PlayerState::Finished) {
        // A finished tone is shown as fully passed.
        toneElapsed = config_.secondsPerTone;
    }
    const double totalElapsed = std::min(
        toneElapsed + currentIndex * config_.secondsPerTone, config_.totalSeconds());
    const double toneRemaining = std::max(config_.secondsPerTone - toneElapsed, 0.0);
    const double totalRemaining = std::max(config_.totalSeconds() - totalElapsed, 0.0);
    return PlaybackProgress{currentIndex, toneElapsed, toneRemaining, totalElapsed,
        totalRemaining};
}

void TonePlayer::openStream()
{
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    std::optional<QAudioDevice> preferred;
    if (deviceLabel_.has_value()) {
        for (const QAudioDevice &device : outputs) {
            if (device.description() == deviceLabel_.value()) {
                preferred = device;
                break;
            }
        }
    }

    // Attempts: the device in mono, the device in stereo, the default device
    // in mono, the default device in stereo.
    struct Attempt {
        std::optional<QAudioDevice> device;
        int count;
    };
    const QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    const std::vector<Attempt> attempts = {
        {preferred, channels},
        {preferred, fallbackChannels},
        {std::nullopt, channels},
        {std::nullopt, fallbackChannels},
    };

    for (const Attempt &attempt : attempts) {
        const QAudioDevice device = attempt.device.value_or(defaultDevice);
        if (device.isNull()) {
            continue;
        }
        const QAudioFormat format = makeFormat(config_.sampleRate, attempt.count);
        if (!device.isFormatSupported(format)) {
            continue;
        }
        auto candidate = std::make_unique<QAudioSink>(device, format);
        // Small look-ahead (100 ms): enough to survive the 20 ms feed timer,
        // while pause/stop take effect immediately instead of seconds later.
        constexpr double bufferSeconds = 0.1;
        const qint64 bufferBytes = static_cast<qint64>(config_.sampleRate * bufferSeconds)
            * attempt.count * qint64(sizeof(float));
        candidate->setBufferSize(static_cast<int>(bufferBytes));
        sinkPort_ = candidate->start();
        if (candidate->error() != QAudio::Error::NoError || sinkPort_ == nullptr) {
            sinkPort_ = nullptr;
            continue;
        }
        sink_ = std::move(candidate);
        sinkChannels_ = attempt.count;
        if (deviceLabel_.has_value() && !attempt.device.has_value()) {
            lastStatus_ = tr("audio.streamUnavailable");
        }
        return;
    }

    throw AudioError(tr("err.audioOpen"));
}

void TonePlayer::closeStream() noexcept
{
    feedTimer_.stop();
    sinkPort_ = nullptr;
    sink_.reset();
}

void TonePlayer::fillFromTimer()
{
    if (sink_ == nullptr) {
        return;
    }
    const qint64 bytesPerFrame = qint64(sizeof(float)) * sinkChannels_;
    // Do not write more than the device has room for.
    const qint64 free = sink_->bytesFree();
    if (free <= 0) {
        return;
    }
    const int capacity = static_cast<int>(free / bytesPerFrame);

    if (state_ != PlayerState::Playing) {
        // The device is suspended while paused: nothing to feed, and the tail
        // already queued in the sink resumes exactly where it stopped.
        return;
    }

    if (sinkPort_ == nullptr) {
        return;
    }
    constexpr int chunk = 2048;
    std::array<float, chunk * fallbackChannels> spread = {};
    int frames = 0;
    while (frames < capacity) {
        const int piece = std::min(chunk, capacity - frames);
        const QVector<float> block = generator_->render(piece);
        const float *source = block.constData();
        if (sinkChannels_ == 1) {
            source = block.constData();
        } else {
            for (int index = 0; index < piece; ++index) {
                const float sample = block.at(index);
                for (int channel = 0; channel < sinkChannels_; ++channel) {
                    spread[static_cast<size_t>(index * sinkChannels_ + channel)] = sample;
                }
            }
            source = spread.data();
        }
        const auto written = sinkPort_->write(reinterpret_cast<const char *>(source),
            static_cast<qint64>(piece) * bytesPerFrame);
        if (written <= 0) {
            break;
        }
        frames += static_cast<int>(written / bytesPerFrame);
        if (generator_->finished) {
            state_ = PlayerState::Finished;
            break;
        }
    }
}

} // namespace tp::audio
