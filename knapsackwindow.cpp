#include "knapsackwindow.h"
#include "ui_knapsackwindow.h"

#include <QString>
#include <QFileDialog>

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

void knapsackWindow::on_pushButton_problem_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, // Parent widget (e.g., your main window)
        tr("Open File"), // Dialog title
        QDir::homePath(), // Default directory (e.g., user's home directory)
        tr("Text Files (*.txt);;All Files (*)") // File filters
    );

    if (!filePath.isEmpty()) {
        // User selected a file, now you can use 'filePath'
        // For example, display it in a QLabel:
        // ui->label->setText(filePath);
    }
}

