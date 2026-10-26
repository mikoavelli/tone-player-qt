/// @file exact_eq.h
/// @brief Exact comparison of doubles without the -Wfloat-equal warning.
///
/// Such a comparison is correct for values obtained from the same computation
/// (snap, clamp) or against exact constants such as 0.0 and floor(x). The
/// semantics coincides with the operator == exactly; the floating-point
/// equality warning (-Wfloat-equal, GCC/Clang) is intentionally suppressed here.
/// MSVC has no equivalent warning, so no pragma is needed there.
#pragma once

namespace tp {

/// @brief Compares two doubles exactly, suppressing -Wfloat-equal.
/// @param a First value.
/// @param b Second value.
/// @return @c true if the values are bit-for-bit equal under the usual == rules.
[[nodiscard]] inline bool exactEq(double a, double b) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    return a == b;
#pragma GCC diagnostic pop
#else
    return a == b;
#endif
}

/// @brief Compares two doubles for exact inequality.
/// @param a First value.
/// @param b Second value.
/// @return @c true if @ref exactEq returns @c false.
[[nodiscard]] inline bool exactNe(double a, double b) noexcept
{
    return !exactEq(a, b);
}

} // namespace tp
