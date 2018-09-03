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
#include <sstream>
#include <regex>

//NOTE: we have to define these if we are not on Linux.
#ifndef S_IRWXU
    #define	S_IRWXU	0000700
    #define	S_IRUSR	0000400
    #define	S_IWUSR	0000200
#endif


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
    SSHInterface(const char *hubIp, const char *hubUsername, const char *hubKey);
    ~SSHInterface();

    bool createInstance(const char *user, const char *instancename);
    bool destroyInstance();
    bool isInstanceConnected();


    //void getProjectList(const char *remoteDB, const char *username, QVector<ProjectSpec> &outdata);
    void getResultsStructure(const char *remoteDB, QVector<TreeData> &structuredata);
    //void getParameterStructure(const char *remoteDB, QVector<TreeData> &structuredata);
    //void getParameterValuesMinMax(const char *remoteDB, std::map<uint32_t, parameter_min_max_val_serial_entry>& IDtoParam);
    //void writeParameterValues(const char *remoteDB, QVector<parameter_serial_entry>& writedata);
    bool getResultSets(const char *remoteDB, const QVector<int>& IDs, QVector<QVector<double>> &valuedata);
    bool uploadEntireFile(const char *localpath, const char *remotelocation, const char *remotefilename);

    const char * getDisconnectionMessage();

    void runModel(const char *exename, const char *remoteDB, QProgressBar *progressBar);
    void handleIncaFinished();
    void handleIncaTick(int);
    void handleRunINCAError(const QString&);

    void sendNoop();


    static void sshLogCallback(int priority, const char *function, const char *buffer, void *data);
    static void sshStatusCallback(void *data, float status);

private:
    ssh_session session_;

    //NOTE: These two bools only reflect whether or not we have logged in and not logged out. We could have been disconnected by other means, so one should always test for isSessionConnected().
    bool loggedInToHub_ = false;
    bool loggedInToInstance_ = false;
    bool instanceExists_ = false;

    std::string instanceIp_;
    std::string instanceUser_;
    std::string instanceName_;
    std::string hubIp_;
    std::string hubUsername_;
    std::string hubKey_;


    QThread incaWorkerThread_;
    QProgressBar *INCARunProgressBar_;
    QTimer *sendNoopTimer;

    bool connectSession(const char *username, const char *serveraddress, const char *keyfile);
    void disconnectSession();
    bool isSessionConnected();

    bool runCommand(const char *command, char *resultbuffer, int bufferlen);
    bool runCommand(const char *command, std::stringstream &out);
    bool writeFile(const void *contents, size_t contentssize, const char *remotelocation, const char *remotefilename);
    bool readFile(void **buffer, size_t *buffersize, const char *remotefilename);
    bool runSqlHandler(const char *command, const char *db, const char *tempfile, const QVector<QString> *extraParam = 0);
    void getStructureData(const char *remoteDB, const char *command, QVector<TreeData> &outdata);

    void generateRandomTransactionFileName(char *outfilename, const char *dbname);
    void deleteTransactionFile(char *filename);

signals:
    void log(const QString&);
    void logError(const QString&);
    void runINCAFinished();
    void runINCAError(const QString&);
};

#endif // SSHTEST_H
