#ifndef SSHTEST_H
#define SSHTEST_H

#include <libssh/libssh.h>
#include "sqlhandler/parameterserialization.h"
#include "treemodel.h"

//NOTE: we have to define these if we are not on Linux.
#ifndef S_IRWXU
    #define	S_IRWXU	0000700
    #define	S_IRUSR	0000400
    #define	S_IWUSR	0000200
#endif

class SSHInterface
{
public:
    SSHInterface();

    bool connectSession(const char *username, const char *serveraddress, const char *keyfile);
    void disconnectSession();
    bool isSessionConnected();


    void getResultsStructure(const char *remoteDB, QVector<TreeData> &structuredata);
    void getParameterStructure(const char *remoteDB, QVector<TreeData> &structuredata);
    void getParameterValuesMinMax(const char *remoteDB, std::map<uint32_t, parameter_min_max_val_serial_entry>& IDtoParam);
    void writeParameterValues(const char *remoteDB, QVector<parameter_serial_entry>& writedata);
    void getResultSets(const QVector<int>& IDs, QVector<QVector<double>> &valuedata);

    bool runCommand(const char *command);
    bool writeFile(const void *contents, size_t contentssize, const char *remotelocation, const char *remotefilename);
    bool readFile(void **buffer, size_t *buffersize, const char *remotefilename);

    bool runSqlHandler(const char *command, const char *db, const char *tempfile);

private:
    ssh_session session_;

    void getStructureData(const char *remoteDB, const char *command, QVector<TreeData> &outdata);
};

#endif // SSHTEST_H
