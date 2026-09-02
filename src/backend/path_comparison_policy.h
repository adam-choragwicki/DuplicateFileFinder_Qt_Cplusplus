#pragma once

#include <Qt>

/// Defines host-platform rules shared by lexical path comparisons.
namespace PathComparisonPolicy
{
    /// Case-sensitivity policy used for lexical path comparisons on the host platform.
#if defined(Q_OS_WIN)
    inline constexpr Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
#else
    inline constexpr Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
#endif
}
