#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "QtCore"
#include "QtGui"
#include "QFileDialog"
#include <QSqlTableModel>
#include "treemodel.h"
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <boost/variant.hpp>
//#include "qcustomplot.h"

namespace Ui {
class MainWindow;
class TreeItem;
}

class MainWindow : public QMainWindow, virtual protected sqlInterface
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:

    void on_pushLoad_clicked();

    void on_pushResults_clicked();

    void on_pushParameters_clicked();

    void on_treeView_clicked(const QModelIndex &index);
    void on_treeViewResults_clicked(const QModelIndex &index);

private:
    Ui::MainWindow *ui;

    TreeModel *treeParameters_, *treeResults_;
//    QVBoxLayout* tabParameterLayout_;
    QGridLayout* tabParameterLayout_;
    std::vector<int> itemsInGrid_;
};


#endif // MAINWINDOW_H
