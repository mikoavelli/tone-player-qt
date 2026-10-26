/// @file settings_dialog.h
/// @brief Settings dialog: sliders, tone and frequencies of the bands.
#pragma once

#include "models.h"

#include <QDoubleSpinBox>
#include <QDialog>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <optional>

class QWidget;

/// @brief Settings dialog and its helper functions.
namespace tp::ui {

/// @brief Width of the labels is the same in all sections: fields line up exactly, px.
inline constexpr int labelWidth = 190;
/// @brief Height of the dialog buttons, px.
inline constexpr int buttonHeight = 34;

/// @brief Modal dialog that edits a copy of the program settings.
///
/// The result is available through @ref settings() only after the dialog has
/// been accepted; cancelling leaves the original settings untouched.
class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    /// @brief Creates the dialog from the current settings.
    /// @param settings Settings shown by the fields.
    /// @param parent Parent widget.
    explicit SettingsDialog(const Settings &settings, QWidget *parent = nullptr);

    /// @brief Returns the validated result.
    /// @throws std::logic_error If the dialog has not been accepted yet.
    [[nodiscard]] const Settings &settings() const;

private:
    /// @brief Builds the dialog layout.
    void build();
    /// @brief Validates the fields and stores the result before closing.
    void accept() final;

    /// @brief Restores the default slider range and step.
    void resetScale();
    /// @brief Resets the band level offsets to zero.
    void resetLevels();
    /// @brief Returns the scale currently entered in the fields.
    [[nodiscard]] LevelScale scale() const;
    /// @brief Writes the edited values into @ref result_.
    void apply();

    Settings dialogSettings_; ///< Settings being edited.
    QVector<Band> bands_; ///< Bands shown by the frequency editor.
    std::optional<Settings> result_; ///< Validated result after acceptance.

    QDoubleSpinBox *minSpin_ = nullptr; ///< Minimum of the level slider range.
    QDoubleSpinBox *maxSpin_ = nullptr; ///< Maximum of the level slider range.
    QDoubleSpinBox *stepSpin_ = nullptr; ///< Step of the level slider range.
    QDoubleSpinBox *secondsSpin_ = nullptr; ///< Tone duration.
    QDoubleSpinBox *fadeSpin_ = nullptr; ///< Fade duration.
    QSpinBox *rateSpin_ = nullptr; ///< Sample rate.
    QPlainTextEdit *frequenciesEdit_ = nullptr; ///< Comma-separated band frequencies.
};

/// @brief Returns the frequency list text: numbers, separated by commas.
/// @param bands Bands whose frequencies are written.
QString frequenciesText(const QVector<Band> &bands);

/// @brief Parses the frequency list text.
/// @param text Comma-separated frequencies.
/// @return Parsed frequencies, or std::nullopt on any error.
std::optional<QVector<double>> parseFrequencies(const QString &text);

} // namespace tp::ui
