#pragma once

#include <QWidget>

namespace Ui
{
    class ResultsTab;
}

class ResultsTab : public QWidget
{
    Q_OBJECT

public:
    explicit ResultsTab(QWidget* parent = nullptr);
    ~ResultsTab();

private:
    Ui::ResultsTab* ui;
};
