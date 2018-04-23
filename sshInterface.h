#ifndef SSHTEST_H
#define SSHTEST_H

#include <libssh/libssh.h>


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
    bool runCommand(const char *command);
    bool writeFile(const void *contents, size_t contentssize, const char *remotelocation, const char *remotefilename);
    bool readFile(void **buffer, size_t *buffersize, const char *remotefilename);

    bool runSqlHandler(const char *command, const char *db, const char *tempfile);

private:
    ssh_session session_;
};

#endif // SSHTEST_H
