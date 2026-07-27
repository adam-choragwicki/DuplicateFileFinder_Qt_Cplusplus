#pragma once

#include "model.h"
#include "main_window.h"
#include "scanner.h"

class Controller : public QObject
{
    Q_OBJECT

signals:
    void applicationShutdownRequested();

public:
    Controller(Model& model, MainWindow& view);

private slots:
    void onStartScanButtonClicked();
    void onChooseDirectoryButtonClicked();

private:
    Model& model_;
    MainWindow& view_;

    Scanner scanner_;
};
