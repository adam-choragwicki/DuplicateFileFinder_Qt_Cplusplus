#pragma once

#include "model.h"
#include "main_window.h"

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
};
