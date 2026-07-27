#pragma once

#include "model.h"
#include "controller.h"
#include "main_window.h"

class Application : public QObject
{
    Q_OBJECT

public:
    Application();

private:
    void printAppInfo();

    std::unique_ptr<Controller> controller_;
    std::unique_ptr<Model> model_;
    std::unique_ptr<MainWindow> view_;
};
