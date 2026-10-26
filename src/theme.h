/// @file theme.h
/// @brief Design colors shared by the window and the equalizer widgets.
#pragma once

#include <QtGui/qcolor.h>

/// @brief Palette of the application.
namespace theme {

/// @brief Window background.
inline constexpr auto bgColor = "#12171c";
/// @brief Background of panels, fields and popups.
inline constexpr auto surfaceColor = "#1c242c";
/// @brief Background of a hovered surface.
inline constexpr auto surfaceHoverColor = "#26313b";
/// @brief Default border color.
inline constexpr auto borderColor = "#35414d";
/// @brief Border color of a hovered element.
inline constexpr auto borderHoverColor = "#4c5a68";
/// @brief Primary text color.
inline constexpr auto textColor = "#eef3f8";
/// @brief Secondary text color.
inline constexpr auto mutedColor = "#b8c4d0";
/// @brief Text color of disabled elements.
inline constexpr auto disabledColor = "#7c8997";
/// @brief Slider groove and other inactive tracks.
inline constexpr auto trackColor = "#2c3742";
/// @brief Accent color: current band, focus and selection.
inline constexpr auto accentColor = "#6ec1ff";
/// @brief Accent color of a hovered element.
inline constexpr auto accentHoverColor = "#8acdff";
/// @brief Text and icons drawn on the accent color.
inline constexpr auto onAccentColor = "#0b1117";
/// @brief Default color of monochrome icons.
inline constexpr auto iconColor = "#d4dce5";
/// @brief Color of disabled monochrome icons.
inline constexpr auto iconDisabledColor = "#718091";
/// @brief Track of a toggle switch in the off state.
inline constexpr auto switchOffColor = "#4a5561";

} // namespace theme
