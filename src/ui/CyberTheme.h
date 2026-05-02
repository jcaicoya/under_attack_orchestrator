#pragma once

#include <QColor>
#include <QString>

namespace CyberTheme {

// Core palette
inline constexpr const char* BackgroundBase = "#050608";
inline constexpr const char* BackgroundSoft = "#080B10";
inline constexpr const char* BackgroundBlueDepth = "#0A1018";

inline constexpr const char* PanelBackground = "#101318";
inline constexpr const char* PanelBackgroundRaised = "#151922";
inline constexpr const char* PanelBorder = "#293241";
inline constexpr const char* FieldBackground = "#20242B";
inline constexpr const char* FieldBorder = "#3B4654";

inline constexpr const char* TextPrimary = "#F2F5F8";
inline constexpr const char* TextSecondary = "#8D96A3";
inline constexpr const char* TextMuted = "#5F6B78";

inline constexpr const char* AccentPrimary = "#1688E8";
inline constexpr const char* AccentPrimaryHover = "#2EA8FF";
inline constexpr const char* AccentCyan = "#00D1FF";
inline constexpr const char* AccentGreen = "#00FF55";
inline constexpr const char* AccentGreenSoft = "#00C78C";
inline constexpr const char* Warning = "#FFB000";
inline constexpr const char* Error = "#FF3347";
inline constexpr const char* Violet = "#8B5CF6";

QColor color(const char* hex, int alpha = 255);
QString globalStyleSheet();

} // namespace CyberTheme
