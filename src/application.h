#pragma once

#include "backend/model.h"
#include "controller.h"
#include "frontend/main_window.h"

/// @brief Initializes and owns the application's top-level MVC components.
class Application : public QObject
{
    Q_OBJECT

public:
    /// Creates the model, main window, and controller, then applies application-wide startup configuration.
    Application();

private:
    /// Logs the application version, build type, Qt version, and runtime platform details.
    void printAppInfo();
    /// Applies the application-wide light color palette.
    void applyLightTheme();

    /// Authoritative application state.
    std::unique_ptr<Model> model_;
    /// Top-level application window.
    std::unique_ptr<MainWindow> view_;
    /// Coordinator constructed after both model and view are available.
    std::unique_ptr<Controller> controller_;
};
