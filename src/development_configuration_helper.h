#pragma once

#include <QString>

/// @brief Provides temporary developer-specific application configuration.
///
/// Values supplied by this class are intended for local development convenience and should not become dependencies of the backend, frontend, or controller layers.
class DevelopmentConfigurationHelper final
{
public:
    DevelopmentConfigurationHelper() = delete;

    /// @brief Returns the scan directory selected for the current development scenario.
    [[nodiscard]] static QString getInitialDirectoryScanPath();
};
