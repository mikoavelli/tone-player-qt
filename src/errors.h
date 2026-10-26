/// @file errors.h
/// @brief Base application errors. Messages are already translated at throw sites.
#pragma once

#include <QString>
#include <stdexcept>
#include <utility>

#include "i18n.h"

/// @brief Base settings error class.
///
/// Carries the translated message both as std::runtime_error text and as a
/// QString for direct display in the interface.
class SettingsError : public std::runtime_error {
public:
    /// @brief Creates an error with an already translated message.
    /// @param message Text shown to the user.
    explicit SettingsError(const QString &message)
        : std::runtime_error(message.toStdString())
        , m_message(message)
    {
    }

    /// @brief Returns the translated message.
    [[nodiscard]] const QString &message() const noexcept { return m_message; }

private:
    QString m_message; ///< Translated message.
};

/// @brief The settings file was not found.
class SettingsNotFoundError final : public SettingsError {
public:
    /// @brief Creates the error for a missing file.
    /// @param path Path of the file that could not be found.
    explicit SettingsNotFoundError(const QString &path)
        : SettingsError(tr_ns::tr("err.settingsNotFound", path))
        , m_path(path)
    {
    }

    /// @brief Returns the path of the missing file.
    [[nodiscard]] const QString &path() const noexcept { return m_path; }

private:
    QString m_path; ///< Path of the missing file.
};

/// @brief Failed to read the file (permissions, disk, encoding).
class SettingsReadError final : public SettingsError {
    using SettingsError::SettingsError;
};

/// @brief The file is corrupted: incorrect syntax or unacceptable data.
class SettingsFormatError final : public SettingsError {
    using SettingsError::SettingsError;
};

/// @brief The settings file was created by a different, unsupported version of the program.
class SettingsVersionError final : public SettingsError {
    using SettingsError::SettingsError;
};

/// @brief Failed to write the settings file.
class SettingsSaveError final : public SettingsError {
    using SettingsError::SettingsError;
};

/// @brief Unacceptable operation with a set of equalizer bands.
class BandError final : public SettingsError {
    using SettingsError::SettingsError;
};

/// @brief Unacceptable operation with a preset: empty name, no such preset.
class PresetError final : public SettingsError {
    using SettingsError::SettingsError;
};

/// @brief Playback could not be started or resumed.
class AudioError final : public std::runtime_error {
public:
    /// @brief Creates an error with an already translated message.
    /// @param message Text shown to the user.
    explicit AudioError(const QString &message)
        : std::runtime_error(message.toStdString())
        , m_message(message)
    {
    }

    /// @brief Returns the translated message.
    [[nodiscard]] const QString &message() const noexcept { return m_message; }

private:
    QString m_message; ///< Translated message.
};
