#include "knapsackwindow.h"
#include "ui_knapsackwindow.h"

knapsackWindow::knapsackWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::knapsackWindow)
{
    ui->setupUi(this);
}

knapsackWindow::~knapsackWindow()
{
    delete ui;
}
