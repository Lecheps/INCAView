#ifndef SSHTEST_H
#define SSHTEST_H

#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#include "sqlhandler/parameterserialization.h"
#include "treemodel.h"
#include <QThread>
#include <QProgressBar>
#include <QTimer>
#include <QString>

//NOTE: we have to define these if we are not on Linux.
#ifndef S_IRWXU
    #define	S_IRWXU	0000700
    #define	S_IRUSR	0000400
    #define	S_IWUSR	0000200
#endif



struct ModelSpec
{
    QString name;
    QString exeName; //NOTE: eventually should maybe be a path.
    QString databasePath;
};


//NOTE: SSHRunIncaWorker is used for calling and monitoring the output of INCA
//in a separate thread while the rest of the program continues its business.
class SSHRunModelWorker : public QObject
{
    Q_OBJECT

public:
    void runModel(const char *, const char *, const char *exename, const char *);
    virtual ~SSHRunModelWorker();
signals:
    void tick(int);
    void resultReady();
    void log(const QString&);
    void reportError(const QString&);
private:
    ssh_session inca_run_session;
    ssh_channel inca_run_channel;
};

class SSHInterface : public QObject
{
    Q_OBJECT

public:
    SSHInterface();
    ~SSHInterface();

    bool connectSession(const char *username, const char *serveraddress, const char *keyfile);
    void disconnectSession();
    bool isSessionConnected();

    void getResultsStructure(const char *remoteDB, QVector<TreeData> &structuredata);
    void getParameterStructure(const char *remoteDB, QVector<TreeData> &structuredata);
    void getParameterValuesMinMax(const char *remoteDB, std::map<uint32_t, parameter_min_max_val_serial_entry>& IDtoParam);
    void writeParameterValues(const char *remoteDB, QVector<parameter_serial_entry>& writedata);
    bool getResultSets(const char *remoteDB, const QVector<int>& IDs, QVector<QVector<double>> &valuedata);

    const char * getDisconnectionMessage();

    void runModel(const char *user, const char *address, const char *keyfile, const char *exename, QProgressBar *progressBar);
    void handleIncaFinished();
    void handleIncaTick(int);
    void handleRunINCAError(const QString&);

    void sendNoop();


    static void sshLogCallback(int priority, const char *function, const char *buffer, void *data);
    static void sshStatusCallback(void *data, float status);

private:
    ssh_session session_;
    QThread incaWorkerThread_;
    QProgressBar *progressBar_;
    QTimer *sendNoopTimer;

    bool runCommand(const char *command, char *resultbuffer, int bufferlen);
    bool writeFile(const void *contents, size_t contentssize, const char *remotelocation, const char *remotefilename);
    bool readFile(void **buffer, size_t *buffersize, const char *remotefilename);
    bool runSqlHandler(const char *command, const char *db, const char *tempfile, const QVector<int>* extraParam = 0);
    void getStructureData(const char *remoteDB, const char *command, QVector<TreeData> &outdata);

signals:
    void log(const QString&);
    void logError(const QString&);
    void runINCAFinished();
    void runINCAError(const QString&);
};

#endif // SSHTEST_H
