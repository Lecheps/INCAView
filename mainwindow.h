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
    void on_pushUploadInputs_clicked();
    void on_pushExportParameters_clicked();
    void on_pushExportResults_clicked();
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

    void log(const QString &);
    void logError(const QString &);
    void logSSHError(const QString&);

private:
    void setParametersHaveBeenEditedSinceLastSave(bool);
    bool runModelProcessLocally(const QString& program, const QStringList& arguments);
    void runModel();
    void setWeExpectToBeConnected(bool);

    void loadParameterDatabase(QString fileName);

    bool getDataSets(const char *dbname, const QVector<int> &IDs, const char *table, QVector<QVector<double>> &seriesout, QVector<int64_t> &startdatesout);

    void loadParameterData();
    void loadResultAndInputStructure(const char *remoteResultDb, const char *RemoteInputDb);

    void resetWindowTitle();

    void updateRunButtonState();

    Ui::MainWindow *ui;

    TreeModel *treeParameters_, *treeResults_, *treeInputs_;

    ParameterModel *parameterModel_;
    ParameterEditDelegate *lineEditDelegate;

    Plotter *plotter_;

    bool parametersHaveBeenEditedSinceLastSave_ = false;
    bool weExpectToBeConnected_ = false;

    QVector<ParameterEditAction> editUndoStack_;

    QString selectedParameterDbPath_;
    bool parameterDbWasSelected_ = false;
    QDir projectDirectory_;

    SSHInterface *sshInterface_;
    SQLInterface projectDb_;

    int maxresultID_ = 0;

    bool inputFileWasSelected_ = false;
    bool inputFileWasUploaded_ = false;
    QString selectedInputFilePath_;
};


#endif // MAINWINDOW_H
