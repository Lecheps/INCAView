#include "sshInterface.h"
#include <QDebug>
#include <QTime>


SSHInterface::SSHInterface()
{
    session_ = 0;

    //NOTE: This initialization code should only be run once, so we can't
    // allow multiple instances of this class. This should not be a problem,
    // but we could make a static check just to be safe.

    //NOTE: Initialization code is commented out for now since we don't have the libssh_threads.dll, and I don't know how to get/build it for MingW.
    //  It seems to work fine for now, but we should probably eventually get the dll to be safe.
    //ssh_threads_set_callbacks(ssh_threads_get_pthread()); //NOTE: QThread is supposedly based on pthread, so this should hopefully work!
    ssh_init();

    Q_ASSERT(sizeof(double)==8); //NOTE: If sizeof(double) != 8 on a potential user architecture, someone has to write reformatting code for the data. (Very unlikely)
    Q_ASSERT(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__); //NOTE: If we are on a big endian user architecture, we also have to write reformatting code! (unlikely)

    ssh_set_log_userdata(this);
    ssh_set_log_level(SSH_LOG_WARNING);
    //ssh_set_log_level(SSH_LOG_PACKET);
    ssh_set_log_callback(SSHInterface::sshLogCallback);

    //NOTE: We send a no-op to the server every 5 minutes to keep the session alive. This will hopefully stay the firewall from thinking
    // it is dead and close it.
    sendNoopTimer = new QTimer(this);
    QObject::connect(sendNoopTimer, &QTimer::timeout, this, &SSHInterface::sendNoop);
    sendNoopTimer->start(1000*60*5);
}

SSHInterface::~SSHInterface()
{
    //TODO: If we are running an INCA process via SSH, we should probably close that down if possible.
    //Closing the thread may not properly close the SSH connection and stop the remote process.
    //TODO: Find out how this works exactly.

    incaWorkerThread_.quit();
    incaWorkerThread_.wait();

    if(isSessionConnected())
    {
       disconnectSession(); //NOTE: This is for the main session. It will not close the INCARun session.
    }
}

void SSHInterface::sshLogCallback(int priority, const char *function, const char *buffer, void *data)
{
    //SSHInterface *caller = (SSHInterface *)data;

    qDebug() << "SSH (priority " << priority << ") " << buffer;
}

void SSHInterface::sshStatusCallback(void *data, float status)
{
    qDebug() << "SSH status: " << status;
}

bool SSHInterface::connectSession(const char *user, const char *address, const char *keyfile)
{
    bool success = false;
    if(!isSessionConnected())
    {
        if(session_)
        {
            ssh_free(session_);
        }

        session_ = ssh_new();
        if(!session_)
        {
            emit logError("SSH: Failed to create session.");
        }
        else
        {
            ssh_options_set(session_, SSH_OPTIONS_HOST, address);
            ssh_options_set(session_, SSH_OPTIONS_USER, user);
            long timeoutSeconds = 1;
            ssh_options_set(session_, SSH_OPTIONS_TIMEOUT, &timeoutSeconds);
            //ssh_options_set(session_, SSH_OPTIONS_STATUS_CALLBACK, SSHInterface::sshStatusCallback); //HMM: does not seem to exist in the latest mingw binary version. We could get it by compiling the library ourselves.

            int rc = ssh_connect(session_);
            if(rc != SSH_OK)
            {
                emit logError(QString("SSH: Failed to connect session: %1").arg(ssh_get_error(session_)));
            }
            else
            {
                //NOTE: This registers the server as a known host on the local user computer. It should be ok to do this without any further checks since we only connect INCAView to servers we own?
                ssh_write_knownhost(session_);

                rc = ssh_userauth_privatekey_file(session_, 0, keyfile, 0);
                if(rc != SSH_AUTH_SUCCESS)
                {
                    emit logError(QString("SSH: Failed to authenticate user: %1").arg(ssh_get_error(session_)));
                    ssh_free(session_);
                }
                else
                {
                    success = true;
                }
            }
        }
    }
    return success;
}

void SSHInterface::disconnectSession()
{
    if(session_)
    {
        ssh_disconnect(session_);
        ssh_free(session_);
        session_ = 0;
    }
}

bool SSHInterface::isSessionConnected()
{
    bool connected = false;
    if(session_)
    {
        //TODO: If the session is idle for a few minutes there is an error where ssh_is_connected returns true, but when one tries to open a channel it fails with a
        //  Socket exception callback (2) 10053, (See also https://wiki.pscs.co.uk/how_to:10053)
        //  and then the session is disconnected. This can be a problem with the google compute engine servers shutting down the connection, but can also be related to which network the user is on.
        //  I have not been able to trace down the cause.
        //  If there is no way to permanently solve it, we need some way to catch this problem and reconnect without the program stalling for too long, the problem right now is that
        //  we have no way to catch it before we try to open a console or send a command in the console, at which point the program hangs for several seconds.
        connected = ssh_is_connected(session_);
    }
    return connected;
}

const char * SSHInterface::getDisconnectionMessage()
{
    const char *message = ssh_get_disconnect_message(session_);
    if(!message) message = ssh_get_error(session_);
    return message;
}

bool SSHInterface::runCommand(const char *command, char *resultbuffer, int bufferlen)
{
    //NOTE: The return value only indicates whether or not the command was executed. It does not say whether or not the program that was called ran successfully.
    bool success = false;
    if(isSessionConnected())
    {
        //TODO: Optimize by holding channel open for later use once it is opened?
        ssh_channel channel = ssh_channel_new(session_);
        int rc = ssh_channel_open_session(channel);
        if(rc != SSH_OK)
        {
            emit logError(QString("SSH: Failed to open channel: %1").arg(ssh_get_error(session_)));
        }
        else
        {
            ssh_channel_request_exec(channel, command);

            int nbytes = ssh_channel_read(channel, resultbuffer, bufferlen-1, 0);
            resultbuffer[nbytes] = 0;

            success = true;

            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
        }
        ssh_channel_free(channel);
    }
    else
    {
        emit logError(QString("SSH: Tried to run command \"%1\" without having an open ssh session.").arg(command));
    }

    return success;
}

bool SSHInterface::writeFile(const void *contents, size_t contentssize, const char *remotelocation, const char *remotefilename)
{
    bool success = false;

    if(isSessionConnected())
    {
        ssh_scp scp = ssh_scp_new(session_, SSH_SCP_WRITE, remotelocation);
        if(scp)
        {
            int rc = ssh_scp_init(scp);
            if(rc != SSH_OK)
            {
                emit logError(QString("SCP: Failed to initialize session: %1").arg(ssh_get_error(session_)));
            }
            else
            {
                rc = ssh_scp_push_file(scp, remotefilename, contentssize, S_IRUSR | S_IWUSR);
                if(rc != SSH_OK)
                {
                    emit logError(QString("SCP: Failed to push file: %1").arg(ssh_get_error(session_)));
                }
                else
                {
                    rc = ssh_scp_write(scp, contents, contentssize);
                    if(rc != SSH_OK)
                    {
                        emit logError(QString("SCP: Failed to write to file: %1").arg(ssh_get_error(session_)));
                    }
                    else
                    {
                        success = true;
                    }
                }
                ssh_scp_close(scp);
            }
            ssh_scp_free(scp);
        }
        else
        {
            emit logError("SCP: Failed to create an scp object.");
        }
    }
    else
    {
        emit logError("SCP: Tried to run file writing command without having an open ssh session.");
    }

    return success;
}

bool SSHInterface::readFile(void **buffer, size_t* buffersize, const char *remotefilename)
{
    bool success = false;

    if(isSessionConnected())
    {
        ssh_scp scp = ssh_scp_new(session_, SSH_SCP_READ, remotefilename);
        if(scp)
        {
            int rc = ssh_scp_init(scp);
            if(rc != SSH_OK)
            {
                emit logError(QString("SCP: Failed to initialize session: %1").arg(ssh_get_error(session_)));
                ssh_scp_free(scp);
                return false;
            }

            rc = ssh_scp_pull_request(scp);
            //TODO: This should to be more robust!
            // The pull request can also return warnings and other return codes which should probably be handled.
            if(rc != SSH_SCP_REQUEST_NEWFILE)
            {
                emit logError(QString("SCP: Failed to receive file info: %1").arg(ssh_get_error(session_)));
                ssh_scp_close(scp);
                ssh_scp_free(scp);
                return false;
            }

            *buffersize = ssh_scp_request_get_size(scp);

            *buffer = malloc(*buffersize);
            //TODO: check if malloc was successful
            int size_remaining = *buffersize;
            ssh_scp_accept_request(scp);
            uint8_t *writeTo = (uint8_t *)*buffer;
            do
            {
                rc = ssh_scp_read(scp, writeTo, *buffersize);
                if(rc == SSH_ERROR || rc < 0)
                {
                    emit logError(QString("SCP: Failed to receive file data: %1").arg(ssh_get_error(session_)));
                    ssh_scp_close(scp);
                    ssh_scp_free(scp);
                    free(buffer);
                    return false;
                }
                else if(!rc)
                {
                    break;
                }
                size_remaining -= rc;
                writeTo += rc;

            } while(size_remaining);

            success = true;

            rc = ssh_scp_pull_request(scp);
            if(rc != SSH_SCP_REQUEST_EOF)
            {
                emit logError(QString("SCP: Unexpected request: %1").arg(ssh_get_error(session_)));
            }


            ssh_scp_close(scp);
            ssh_scp_free(scp);
        }
        else
        {
            emit logError("SCP: Failed to create an scp object.");
        }
    }
    else
    {
        emit logError("SCP: Tried to run file reading command without having an open ssh session.");
    }

    return success;
}

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

bool SSHInterface::runSqlHandler(const char *command, const char *db, const char *tempfile, const QVector<int> *extraParam)
{
    char commandbuf[512];
    char resultbuf[512];
    int len = sprintf(commandbuf, "./incaview/sqlhandler %s %s %s", command, db, tempfile);
    if(extraParam)
    {
        for(int par : *extraParam)
        {
            len += sprintf(commandbuf + len, " %d", par);
        }
    }

    //qDebug(commandbuf);

    bool success = runCommand(commandbuf, resultbuf, sizeof(resultbuf));

    //qDebug(resultbuf);

    if(startsWith("ERROR:", resultbuf))
    {
        emit logError(QString("SSH: SQL: Unsuccessful operation on remote database:</br>&emsp;%1").arg(resultbuf));
        success = false;
    }
    else if(startsWith("SUCCESS:", resultbuf))
    {
        //emit log(QString("SSH: SQL: Successful request to remote database: %1").arg(command));
    }

    return success;
}

void SSHInterface::getStructureData(const char *remoteDB, const char *command, QVector<TreeData> &outdata)
{
    bool success = runSqlHandler(command, remoteDB, "data.dat");

    if(success)
    {
        void *filedata = 0;
        size_t filesize;
        success = readFile(&filedata, &filesize, "~/data.dat");
        if(success)
        {
            uint8_t *at = (uint8_t *)filedata;
            while(at < (uint8_t *)filedata + filesize)
            {
                structure_serial_entry *entry = (structure_serial_entry *)at;
                at += sizeof(structure_serial_entry);

                int parentID = (int)entry->parentID;
                int childID = (int)entry->childID;

                std::string str((char *)at, (char *)at + entry->childNameLen); //Is there a better way to get a QString from a range based char * (not nullterminated) than going via a std::string?

                outdata.push_back({QString::fromStdString(str), childID, parentID});

                at += entry->childNameLen;
            }
        }
        if(filedata) free(filedata);
    }
}

void SSHInterface::getResultsStructure(const char *remoteDB, QVector<TreeData> &structuredata)
{
    getStructureData(remoteDB, EXPORT_RESULTS_STRUCTURE_COMMAND, structuredata);
}


void SSHInterface::getParameterStructure(const char *remoteDB, QVector<TreeData> &structuredata)
{
    getStructureData(remoteDB, EXPORT_PARAMETER_STRUCTURE_COMMAND, structuredata);
}


void SSHInterface::getParameterValuesMinMax(const char *remoteDB, std::map<uint32_t, parameter_min_max_val_serial_entry>& IDtoParam)
{
    bool success = runSqlHandler(EXPORT_PARAMETER_VALUES_MIN_MAX_COMMAND, remoteDB, "data.dat");

    if(success)
    {
        void *filedata = 0;
        size_t filesize;
        success = readFile(&filedata, &filesize, "~/data.dat");
        if(success)
        {
            uint8_t *at = (uint8_t *)filedata;
            while(at < (uint8_t *)filedata + filesize)
            {
                parameter_min_max_val_serial_entry *entry = (parameter_min_max_val_serial_entry *)at;
                IDtoParam[entry->ID] = *entry;
                at += sizeof(parameter_min_max_val_serial_entry);
            }
        }

        if(filedata) free(filedata);
    }
}


bool SSHInterface::getResultSets(const char *remoteDB, const QVector<int>& IDs, QVector<QVector<double>> &valuedata)
{
    bool success = runSqlHandler(EXPORT_RESULT_VALUES_COMMAND, remoteDB, "data.dat", &IDs);

    if(success)
    {
        void *filedata = 0;
        size_t filesize;
        success = readFile(&filedata, &filesize, "~/data.dat");
        if(success)
        {
            uint8_t *data = (uint8_t *)filedata;
            uint64_t numresults = *(uint64_t *)data;
            data += sizeof(uint64_t);

            if((int)numresults == IDs.count())
            {
                valuedata.resize((int)numresults);

                //qDebug(QString::number((int)numresults).toLatin1().data());

                for(int i = 0; i < numresults; ++i)
                {
                    //TODO: Check that we never overstep the filesize;
                    uint64_t count = *(uint64_t *)data;
                    data += sizeof(uint64_t);
                    int cnt = (int)count;

                    //qDebug(QString::number(cnt).toLatin1().data());

                    QVector<double>& current = valuedata[i];
                    current.resize(cnt);

                    double *data_d = (double *)data;
                    for(int j = 0; j < cnt; ++j)
                    {
                        current[j] = *data_d++;
                    }
                    data += cnt*sizeof(double);
                }
            }
            else
            {
                emit logError(QString("SSH: SQL: Requested %1 result sets, got %2").arg((int)numresults).arg(IDs.count()));
                success = false;
            }
        }

        if(filedata) free(filedata);
    }

    return success;
}


void SSHInterface::writeParameterValues(const char *remoteDB, QVector<parameter_serial_entry>& writedata)
{
    uint64_t count = writedata.count();
    size_t size = sizeof(uint64_t) + count*sizeof(parameter_serial_entry);
    void *result = malloc(size);

    uint8_t *at = (uint8_t *)result;
    *(uint64_t *)at = count;
    at += sizeof(uint64_t);
    memcpy(at, writedata.data(), count*sizeof(parameter_serial_entry));

    bool success = writeFile(result, size, "~/", "data.dat");
    free(result);

    if(success) runSqlHandler(IMPORT_PARAMETER_VALUES_COMMAND, remoteDB, "data.dat");
}


void SSHInterface::runINCA(const char *user, const char *address, const char *keyfile, QProgressBar *progressBar)
{
    //NOTE: We have to create a new SSH session in the new thread. Two different threads can not use the same session in libssh,
    //or there is a risk of state corruption.

    //TODO: Check that we are not already running a remote INCA process from this application. (Should not happen if the UI behaves correctly though).

    SSHRunIncaWorker *worker = new SSHRunIncaWorker;
    worker->moveToThread(&incaWorkerThread_);
    connect(&incaWorkerThread_, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &SSHRunIncaWorker::resultReady, this, &SSHInterface::handleIncaFinished);
    connect(worker, &SSHRunIncaWorker::tick, this, &SSHInterface::handleIncaTick);

    //Relay logging signals
    connect(worker, &SSHRunIncaWorker::log, this, &SSHInterface::log);
    connect(worker, &SSHRunIncaWorker::reportError, this, &SSHInterface::handleRunINCAError);

    progressBar_ = progressBar;
    progressBar_->setVisible(true);
    progressBar_->setMaximum(20); //TODO: Set to the correct amout of timesteps!

    incaWorkerThread_.start();

    worker->runINCA(user, address, keyfile);
}


void SSHInterface::handleIncaFinished()
{
    if(progressBar_)
    {
        progressBar_->setVisible(false);
    }

    emit runINCAFinished();
}

void SSHInterface::handleRunINCAError(const QString& message)
{
    //NOTE: Just relay the signal upwards...
    emit runINCAError(message);
}

void SSHInterface::handleIncaTick(int ticknum)
{
    progressBar_->setValue(ticknum);
}

void SSHInterface::sendNoop()
{
    qDebug() << "No-op";

    //NOTE: This function is supposed to be called in a regular interval so that the session is not idle
    // for too long.
    //TODO: Since this is called by the timer, what happens if we are doing someting with the session at
    // the same time?? Do we need to set a lock while using the session that prevents this from being called?

    if(session_)
    {
        const char *ignorethismessageplease = "No-op";
        int rc = ssh_send_ignore(session_, ignorethismessageplease);
        if(rc == SSH_ERROR)
        {
            //TODO: This is bad, at least if we expected to be connected. Figure out how to handle it
            qDebug() << "No-op caused error";
        }
    }
}


//--------------------------- SSHRunIncaWorker --------------------------------------------------------------------


void SSHRunIncaWorker::runINCA(const char *user, const char *address, const char *keyfile)
{
    //NOTE: We have to create a new SSH session in the new thread. Two threads can not use the same session in libssh,
    //or there is a risk of state corruption.
    ssh_session inca_run_session = ssh_new();
    if(!inca_run_session)
    {
        emit reportError("SSH: Failed to create session to start run of inca.");
        return;
    }

    ssh_options_set(inca_run_session, SSH_OPTIONS_HOST, address);
    ssh_options_set(inca_run_session, SSH_OPTIONS_USER, user);

    int rc = ssh_connect(inca_run_session);
    if(rc != SSH_OK)
    {
        emit reportError(QString("SSH: Failed to connect session: %1").arg(ssh_get_error(inca_run_session)));
        return;
    }

    //NOTE: This registers the server as a known host on the local user computer. Should be ok to do this without any further checks since we only connect INCAView to servers we own?
    ssh_write_knownhost(inca_run_session);

    rc = ssh_userauth_privatekey_file(inca_run_session, 0, keyfile, 0);
    if(rc != SSH_AUTH_SUCCESS)
    {
        emit reportError(QString("SSH: Failed to authenticate user: %1").arg(ssh_get_error(inca_run_session)));
        ssh_free(inca_run_session);
    }

    //TODO: Optimize by holding channel open for later use once it is opened?
    ssh_channel channel = ssh_channel_new(inca_run_session);
    rc = ssh_channel_open_session(channel);
    if(rc != SSH_OK)
    {
        emit reportError(QString("SSH: Failed to open channel: %1").arg(ssh_get_error(inca_run_session)));
        ssh_free(inca_run_session);
        return;
    }

    ssh_channel_request_exec(channel, "cd incaview;./core_hbv");

    char readData[512];

    int poll_rc;
    while((poll_rc = ssh_channel_poll(channel, 0)) != SSH_EOF)
    {
        if(poll_rc == SSH_ERROR)
        {
            emit reportError(QString("SSH: Error while reading from INCA run channel: %1").arg(ssh_get_error(inca_run_session)));
            break;
        }

        //TODO: Check that this sleep works correctly!
        QThread::msleep(50);

        int rc = ssh_channel_read_nonblocking(channel, readData, sizeof(readData)-1, 0);
        readData[rc] = 0;
        if(rc > 0)
        {
            emit log(readData);
            //TODO: When we have an inca model that prints out its timesteps, parse output and emit tick(timestep); to update the progress bar.
        }
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);

    ssh_channel_free(channel);
    ssh_disconnect(inca_run_session);
    ssh_free(inca_run_session);

    emit resultReady();
}
