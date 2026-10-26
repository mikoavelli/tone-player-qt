// Base application errors. Messages are already translated at throw sites.
#pragma once

#include <QString>
#include <stdexcept>
#include <utility>

#include "i18n.h"

// Base settings error class.
class SettingsError : public std::runtime_error {
public:
    explicit SettingsError(const QString &message)
        : std::runtime_error(message.toStdString())
        , m_message(message)
    {
    }

    [[nodiscard]] const QString &message() const noexcept { return m_message; }

private:
    QString m_message;
};

class SettingsNotFoundError final : public SettingsError {
public:
    explicit SettingsNotFoundError(const QString &path)
        : SettingsError(tr_ns::tr("err.settingsNotFound", path))
        , m_path(path)
    {
    }

    [[nodiscard]] const QString &path() const noexcept { return m_path; }

private:
    QString m_path;
};

// Failed to read the file (permissions, disk, encoding).
class SettingsReadError final : public SettingsError {
    using SettingsError::SettingsError;
};

// The file is corrupted: incorrect syntax or unacceptable data.
class SettingsFormatError final : public SettingsError {
    using SettingsError::SettingsError;
};

// The settings file was created by a different, unsupported version of the program.
class SettingsVersionError final : public SettingsError {
    using SettingsError::SettingsError;
};

// Failed to write the settings file.
class SettingsSaveError final : public SettingsError {
    using SettingsError::SettingsError;
};

// Unacceptable operation with a set of equalizer bands.
class BandError final : public SettingsError {
    using SettingsError::SettingsError;
};

// Unacceptable operation with a preset: empty name, no such preset.
class PresetError final : public SettingsError {
    using SettingsError::SettingsError;
};

// Playback could not be started or resumed.
class AudioError final : public std::runtime_error {
public:
    explicit AudioError(const QString &message)
        : std::runtime_error(message.toStdString())
        , m_message(message)
    {
    }

    [[nodiscard]] const QString &message() const noexcept { return m_message; }

private:
    QString m_message;
};
