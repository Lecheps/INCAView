#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "QtCore"
#include "QtGui"
#include "QFileDialog"
#include <QSqlTableModel>
#include "treemodel.h"
#include "parametermodel.h"
#include "lineeditdelegate.h"
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
    void on_pushSave_clicked();
    void on_pushSaveAs_clicked();
    void on_pushRun_clicked();
    void closeEvent (QCloseEvent *);

    void on_treeView_clicked(const QModelIndex &index);
    void on_treeViewResults_clicked(const QModelIndex &index);


private:

    void populateParameterModel();
    void parameterWasEdited(const QString&, int);

    void toggleStuffHasBeenEditedSinceLastSave(bool);

    void runINCA();

    static bool copyAndOverwriteFile(const QString&, const QString&);
    bool saveCheckParameters();
    bool tryToSave(const QString&, const QString&);

    Ui::MainWindow *ui;

    TreeModel *treeParameters_, *treeResults_;

    ParameterModel *parameterModel_;
    LineEditDelegate *lineEditDelegate;

    const QString tempWorkingDBPath = "__temp.db";
    QString loadedDBPath_;

    bool stuffHasBeenEditedSinceLastSave = false;
};


#endif // MAINWINDOW_H
