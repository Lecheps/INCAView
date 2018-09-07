#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "sshInterface.h"
#include "treemodel.h"
#include "parametermodel.h"
#include "parametereditdelegate.h"
#include <QMainWindow>
#include "QtCore"
#include "QtGui"
#include "QFileDialog"
#include <QSqlTableModel>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include "plotter.h"
#include "sqlinterface.h"


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:

    void on_pushConnect_clicked();
    void on_pushDisconnect_clicked();
    void on_pushRun_clicked();
    void on_pushLoadProject_clicked();
    void on_pushSaveParameters_clicked();
    void on_pushCreateDatabase_clicked();
    void closeEvent (QCloseEvent *);

    void updateParameterView(const QItemSelection &, const QItemSelection &);
    void updateGraphsAndResultSummary();
    void clearGraphsAndResultSummary();

    //void handleModelSelect(int index);

    void copyToClipboard(bool);
    void undo(bool);
    void updateGraphToolTip(QMouseEvent *event);
    void parameterWasEdited(ParameterEditAction);
    void handleInvoluntarySSHDisconnect();
    void onRunINCAFinished();
    void handleRunINCAError(const QString&);

    void log(const QString &);
    void logError(const QString &);
    void logSSHError(const QString&);

private:
    void toggleParametersHaveBeenEditedSinceLastSave(bool);
    void runModel();
    void setWeExpectToBeConnected(bool);

    void loadParameterDatabase(QString fileName);

    void loadParameterData();
    void loadResultStructure(const char *remotedbpath);

    void resetWindowTitle();

    Ui::MainWindow *ui;

    TreeModel *treeParameters_, *treeResults_;

    ParameterModel *parameterModel_;
    ParameterEditDelegate *lineEditDelegate;

    Plotter *plotter_;

    bool parametersHaveBeenEditedSinceLastSave_ = false;
    bool weExpectToBeConnected_ = false;

    QVector<ParameterEditAction> editUndoStack_;

    QString selectedProjectDbPath_;

    SSHInterface *sshInterface_;
    SQLInterface projectDb_;
};


#endif // MAINWINDOW_H
