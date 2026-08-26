#pragma once

#include "backend/model.h"
#include "controller.h"
#include "frontend/main_window.h"

class Application : public QObject
{
    Q_OBJECT

public:
    Application();

private:
    void printAppInfo();
    void applyLightTheme();

    std::unique_ptr<Controller> controller_;
    std::unique_ptr<Model> model_;
    std::unique_ptr<MainWindow> view_;
};
