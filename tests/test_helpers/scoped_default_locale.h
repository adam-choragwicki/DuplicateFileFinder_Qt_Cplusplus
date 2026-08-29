#pragma once

#include <QLocale>

namespace test_helpers
{
    /// Temporarily replaces Qt's default locale and restores it when the current scope ends.
    class ScopedDefaultLocale final
    {
    public:
        explicit ScopedDefaultLocale(const QLocale& locale) : previousDefaultLocale_(QLocale())
        {
            QLocale::setDefault(locale);
        }

        ~ScopedDefaultLocale()
        {
            QLocale::setDefault(previousDefaultLocale_);
        }

        ScopedDefaultLocale(const ScopedDefaultLocale&) = delete;
        ScopedDefaultLocale& operator=(const ScopedDefaultLocale&) = delete;

    private:
        QLocale previousDefaultLocale_;
    };
}
