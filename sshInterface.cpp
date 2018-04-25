#include "sshInterface.h"
#include <QDebug>

SSHInterface::SSHInterface()
{
    session_ = 0;
}

bool SSHInterface::connectSession(const char *user, const char *address, const char *keyfile)
{
    bool success = false;
    if(!session_)
    {
        session_ = ssh_new();
        if(!session_)
        {
            emit logError("SSH: Failed to create session.");
        }
        else
        {
            //address = "35.197.231.4"
            //user = "magnus"
            //keyfile = "C:\\testkeys\\testkey"
            ssh_options_set(session_, SSH_OPTIONS_HOST, address);
            ssh_options_set(session_, SSH_OPTIONS_USER, user);

            int rc = ssh_connect(session_);
            if(rc != SSH_OK)
            {
                emit logError(QString("SSH: Failed to connect session: %1").arg(ssh_get_error(session_)));
            }
            else
            {
                //TODO: This should probably be wrapped in some safety stuff. See libssh tutorial.
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
    bool result = false;
    if(session_)
    {
        result = ssh_is_connected(session_);
    }
    return result;
}

bool SSHInterface::runCommand(const char *command, char *resultbuffer, int bufferlen)
{
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

            //qDebug("Result of running ssh command:");
            //qDebug(resultbuffer);

            success = true; //NOTE: This only says that the command was executed. It does not say whether or not the program that was called ran successfully.

            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
        }
        ssh_channel_free(channel);
    }
    else
    {
        emit logError("SSH: Tried to run command without having an open ssh session.");
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
        emit logError("SCP: Tried to run write command without having an open ssh session.");
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
        emit logError("SCP: Tried to run write command without having an open ssh session.");
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
    //sprintf(buf, "./testdirectory/sqlhandler %s %s %s", command, db, tempfile);
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
        //NOTE: This log entry should probably be commented out before release:
        emit log(QString("SSH: SQL: Successful request to remote database: %1").arg(command));
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
        readFile(&filedata, &filesize, "~/data.dat");
        uint8_t *at = (uint8_t *)filedata;
        while(at < (uint8_t *)filedata + filesize)
        {
            structure_serial_entry *entry = (structure_serial_entry *)at;
            at += sizeof(structure_serial_entry);

            int parentID = (int)entry->parentID;
            int childID = (int)entry->childID;

            std::string str((char *)at, (char *)at + entry->childNameLen); //Is there a better way to get a QString from a range based char * (not nullterminated)?

            outdata.push_back({QString::fromStdString(str), childID, parentID});

            at += entry->childNameLen;
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
        readFile(&filedata, &filesize, "~/data.dat");

        uint8_t *at = (uint8_t *)filedata;
        while(at < (uint8_t *)filedata + filesize)
        {
            parameter_min_max_val_serial_entry *entry = (parameter_min_max_val_serial_entry *)at;
            IDtoParam[entry->ID] = *entry;
            at += sizeof(parameter_min_max_val_serial_entry);
        }

        if(filedata) free(filedata);
    }
}


void SSHInterface::getResultSets(const char *remoteDB, const QVector<int>& IDs, QVector<QVector<double>> &valuedata)
{
    bool success = runSqlHandler(EXPORT_RESULT_VALUES_COMMAND, remoteDB, "data.dat", &IDs);

    if(success)
    {
        void *filedata;
        size_t filesize;
        readFile(&filedata, &filesize, "~/data.dat");

        uint8_t *data = (uint8_t *)filedata;
        uint64_t numresults = *(uint64_t *)data;
        data += sizeof(uint64_t);

        //TODO: Better error handling
        Q_ASSERT(sizeof(double)==8); //If this is not the case, someone has to write reformatting code for the data.

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

        if(filedata) free(filedata);
    }
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

