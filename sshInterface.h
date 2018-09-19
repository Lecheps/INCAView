#ifndef SSHINTERFACE_H
#define SSHINTERFACE_H

#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#include "sqlhandler/serialization.h"
#include "treemodel.h"
#include <QThread>
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


class SSHInterface : public QObject
{
    Q_OBJECT

public:
    SSHInterface(const char *hubIp, const char *hubUsername, const char *hubKey);
    ~SSHInterface();

    bool createInstance(const char *user, const char *instancename);
    bool destroyInstance();
    bool isInstanceConnected();

    bool getResultsStructure(const char *remoteDB, QVector<TreeData> &structuredata);
    bool getResultSets(const char *remoteDB, const QVector<int>& IDs, QVector<QVector<double>> &valuedata);
    bool uploadEntireFile(const char *localpath, const char *remotelocation, const char *remotefilename);
    bool downloadEntireFile(const char *localpath, const char *remotefilename);

    bool createParameterDatabase(const char *remoteparameterfile, const char *exename);

    const char * getDisconnectionMessage();

    void runModel(const char *exename, const char *remoteDB);

    void sendNoop();


    static void sshLogCallback(int priority, const char *function, const char *buffer, void *data);
    static void sshStatusCallback(void *data, float status);

private:
    ssh_session session_;

    //NOTE: These two bools only reflect whether or not we have logged in and not logged out. We could have been disconnected by error, so one should always test for isSessionConnected().
    bool loggedInToHub_ = false;
    bool loggedInToInstance_ = false;
    bool instanceExists_ = false;

    std::string instanceIp_;
    std::string instanceUser_;
    std::string instanceName_;
    std::string hubIp_;
    std::string hubUsername_;
    std::string hubKey_;

    QTimer *sendNoopTimer;

    bool connectSession(const char *username, const char *serveraddress, const char *keyfile);
    void disconnectSession();
    bool isSessionConnected();

    bool runCommand(const char *command, std::stringstream &out, bool logAsItHappens = false);
    bool writeFile(const void *contents, size_t contentssize, const char *remotelocation, const char *remotefilename);
    bool readFile(void **buffer, size_t *buffersize, const char *remotefilename);
    bool runSqlHandler(const char *command, const char *db, const char *tempfile, const QVector<QString> *extraParam = 0);
    bool getStructureData(const char *remoteDB, const char *command, QVector<TreeData> &outdata);

    void deleteTransactionFile(const char *filename);

signals:
    void log(const QString&);
    void logError(const QString&);
};

#endif // SSHINTERFACE_H
