#include "results_tab.h"
#include "ui_results_tab.h"

ResultsTab::ResultsTab(QWidget* parent)
    : QWidget(parent)
      , ui(new Ui::ResultsTab)
{
    ui->setupUi(this);
}

ResultsTab::~ResultsTab()
{
    delete ui;
}
