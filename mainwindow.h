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

    void parameterTreeSelectionChanged(const QItemSelection &, const QItemSelection &);
    void resultsTreeSelectionChanged(const QItemSelection &, const QItemSelection &);
    void parameterViewCopyRequest(bool);

private:

    void populateParameterModel(ParameterModel*);
    void populateTreeModel(TreeModel*, const QString&, bool);
    void parameterWasEdited(const QString&, int);

    void toggleStuffHasBeenEditedSinceLastSave(bool);

    void runINCA();

    static bool copyAndOverwriteFile(const QString&, const QString&);
    bool saveCheckParameters();
    bool tryToSave(const QString&, const QString&);

    void resetWindowTitle();

    Ui::MainWindow *ui;

    TreeModel *treeParameters_, *treeResults_;

    ParameterModel *parameterModel_;
    LineEditDelegate *lineEditDelegate;

    const QString tempWorkingDBPath = "__temp.db";
    QString loadedDBPath_;

    bool stuffHasBeenEditedSinceLastSave = false;

    QList<QColor> graphColors_;
};


#endif // MAINWINDOW_H
