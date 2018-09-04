#include "sshInterface.h"
#include <QDebug>
#include <QTime>
#include <QRandomGenerator>

//NOTE: Useful blog post on using QThread: https://mayaposch.wordpress.com/2011/11/01/how-to-really-truly-use-qthreads-the-full-explanation/

SSHInterface::SSHInterface(const char *hubIp, const char *hubUsername, const char *hubKey)
{
    session_ = 0;

    hubIp_ = hubIp;
    hubUsername_ = hubUsername;
    hubKey_ = hubKey;

    //NOTE: This initialization code should only be run once, so we can't
    // allow multiple instances of this class. This should not be a problem,
    // but we could make a static check just to be safe.

    //NOTE: Initialization code is commented out for now since we don't have the libssh_threads.dll, and I don't know how to get/build it for MingW.
    //  It seems to work fine for now, but we should probably eventually get the dll to be safe.
    //ssh_threads_set_callbacks(ssh_threads_get_pthread()); //NOTE: QThread is supposedly based on pthread, so this should hopefully work!
    ssh_init();

    Q_ASSERT(sizeof(double) == 8); //NOTE: If sizeof(double) != 8 on a potential user architecture, someone has to write reformatting code for the data. (Very unlikely)
    Q_ASSERT(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__); //NOTE: If we are on a big endian user architecture, we also have to write reformatting code! (Very unlikely)

    ssh_set_log_userdata(this);
    ssh_set_log_level(SSH_LOG_WARNING);
    //ssh_set_log_level(SSH_LOG_PACKET);
    ssh_set_log_callback(SSHInterface::sshLogCallback);

    //NOTE: We send a no-op to the server every 5 minutes to keep the session alive. This will hopefully stop the firewall from thinking
    // it is dead and close it.
    //NOTE: According to my understanding, the QTimer does not work on a separate thread but rather in the main application's event loop, so this
    // function will never be called while the sshInterface is doing something else, meaning we should not get conflicts when using the session_ in sendNoop()
    // as we would if we used it in a separate thread.
    sendNoopTimer = new QTimer(this);
    QObject::connect(sendNoopTimer, &QTimer::timeout, this, &SSHInterface::sendNoop);
    sendNoopTimer->start(1000*60*5);
}

SSHInterface::~SSHInterface()
{
    incaWorkerThread_.quit();
    incaWorkerThread_.wait();

    if(isSessionConnected() && loggedInToInstance_)
    {
       destroyInstance();
    }

    if(isSessionConnected())
    {
        //NOTE: This is for the main session. It will not close the INCARun session.
        //  However that should be handled by the RunIncaWorker destructor if all other signals are hooked up correctly.
        disconnectSession();
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


bool SSHInterface::createInstance(const char *username, const char *instancename)
{
    bool success = connectSession(hubUsername_.data(), hubIp_.data(), hubKey_.data());

    if(!success)
    {
        return false;
    }

    loggedInToHub_ = true;


    //TODO: Query the hub to see if an instance by the name instancename already exists.

    char command[512];
    sprintf(command, "./createinstance.sh %s %s", instancename, username);

    std::stringstream output;
    runCommand(command, output);
    //std::string gotoutput = output.str();
    //qDebug() << gotoutput.data();

    //NOTE: Format of what we want to parse for the external ip. (NOTE HOWEVER: google state that they don't guarantee the format of output from gcloud to stay constant!!!)
    // NAME             ZONE            MACHINE_TYPE   PREEMPTIBLE  INTERNAL_IP  EXTERNAL_IP    STATUS
    // incaview-magnus  europe-west3-a  n1-standard-2               10.156.0.4   35.234.84.222  RUNNING

    //NOTE: split on whitespace:
    std::vector<std::string> words((std::istream_iterator<std::string>(output)),
                                     std::istream_iterator<std::string>());

    //NOTE: find the second word that looks like an ip address.
    std::regex ippattern("[[:digit:]]+.[[:digit:]]+.[[:digit:]]+.[[:digit:]]+");
    bool foundone = false;
    bool foundtwo = false;
    for(std::string& word : words)
    {
        if(std::regex_match(word, ippattern))
        {
            if(foundone)
            {
                instanceIp_ = word;
                foundtwo = true;
                break;
            }
            foundone = true;
        }
    }

    if(!foundtwo)
    {
        emit logError("Something went wrong with creating an instance. Could not parse an external ip from output of gcloud command. Output was:");
        emit logError(output.str().data());
        return false;
    }

    instanceName_ = instancename;
    instanceUser_ = username;

    emit log(QString("Created compute instance with ip ") + instanceIp_.data());

    //TODO: Error handling!!

    //NOTE: download ssh keys for the instance.
    std::string privkeyfilename = std::string("keys/") + username;
    std::string pubkeyfilename = std::string("keys/") + username + ".pub";

    void *filebuf;
    size_t filebufsize;

    //TODO: If we use the libssh library correctly, we may probably skip saving the keys to files here and instead provide the key data directly to libssh on the connect.
    readFile(&filebuf, &filebufsize, privkeyfilename.data());
    FILE *file = fopen("instancekey", "w");
    fwrite(filebuf, filebufsize, 1, file);
    fclose(file);
    free(filebuf);

    readFile(&filebuf, &filebufsize, pubkeyfilename.data());
    file = fopen("instancekey.pub", "w");
    fwrite(filebuf, filebufsize, 1, file);
    fclose(file);
    free(filebuf);

    emit log("SSH keys to instance downloaded. Attempting to connect to instance...");

    //NOTE: Disconnect from the hub
    disconnectSession();

    loggedInToHub_ = false;

    int maxtries = 10;
    //NOTE: Connect to the new instance
    for(int i = 0; i < maxtries; ++i)
    {
        success = connectSession(username, instanceIp_.data(), "instancekey");
        if(success)
        {
            break;
        }
        else if(i != maxtries-1)
        {
            emit log("Attempting to connect again...");
        }
    }
    if(!success)
    {
        emit logError("Unable to connect");

        destroyInstance();

        return false;
    }

    loggedInToInstance_ = true;
    instanceExists_ = true;

    return true;
}

bool SSHInterface::destroyInstance()
{
    if(!instanceExists_) return true;


    if(loggedInToInstance_ && isSessionConnected())
    {
        disconnectSession();
        loggedInToInstance_ = false;
    }

    if(!loggedInToHub_ || !isSessionConnected())
    {
        bool success = connectSession(hubUsername_.data(), hubIp_.data(), hubKey_.data());
        if(!success)
        {
            emit logError("SSH: Unable to connect to hub in order to destroy compute instance.");
            return false;
        }
        loggedInToHub_ = true;
    }

    emit log(QString("SSH: Sending command to destroy compute instance ") + instanceName_.data());
    emit log(QString("This may take a few seconds ..."));

    char command[512];
    sprintf(command, "./destroyinstance.sh %s", instanceName_.data());

    std::stringstream out;
    bool success = runCommand(command, out);

    if(success)
    {
        //TODO: Parse output to see if gcloud reported a successful deletion?
        emit log(QString("SSH: Successfully destroyed compute instance ") + instanceName_.data());
    }

    instanceExists_ = false;

    return success;
}

bool SSHInterface::isInstanceConnected()
{
    return loggedInToInstance_ && isSessionConnected();
}


bool SSHInterface::connectSession(const char *user, const char *address, const char *keyfile)
{
    bool success = false;
    if(!isSessionConnected())
    {
        if(session_)
        {
            ssh_free(session_);
            session_ = 0;
        }

        session_ = ssh_new();
        if(!session_)
        {
            emit logError("SSH: Failed to create session.");
            return false;
        }

        ssh_options_set(session_, SSH_OPTIONS_HOST, address);
        ssh_options_set(session_, SSH_OPTIONS_USER, user);
        //long timeoutSeconds = 1;
        //ssh_options_set(session_, SSH_OPTIONS_TIMEOUT, &timeoutSeconds);
        //ssh_options_set(session_, SSH_OPTIONS_STATUS_CALLBACK, SSHInterface::sshStatusCallback); //HMM: does not seem to exist in the latest mingw binary version. We could get it by compiling the library ourselves.

        int rc = ssh_connect(session_);
        if(rc != SSH_OK)
        {
            emit logError(QString("SSH: Failed to connect session: %1").arg(ssh_get_error(session_)));
            ssh_free(session_);
            session_ = 0;
            return false;
        }

        //NOTE: This registers the server as a known host on the local user computer. It should be ok to do this without any further checks since we only connect INCAView to servers we own?
        // ALTHOUGH: somebody may mistype the server address. The problem with querying them for this though is that they would maybe not recognize the mistyping?
        // Alternatively, eventually set up a server that provides the right address so that the user does not have to type it in.
        ssh_write_knownhost(session_);

        rc = ssh_userauth_privatekey_file(session_, 0, keyfile, 0);
        if(rc != SSH_AUTH_SUCCESS)
        {
            emit logError(QString("SSH: Failed to authenticate user: %1").arg(ssh_get_error(session_)));
            ssh_free(session_);
            session_ = 0;
            return false;
        }

        success = true;
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

bool SSHInterface::runCommand(const char *command, std::stringstream &out)
{
    //NOTE: The return value of this function only indicates whether or not the command was executed.
    //  It does not say whether or not the program that was called ran successfully. For that one has
    //  to parse the result buffer.
    bool success = false;
    if(isSessionConnected())
    {
        ssh_channel channel = ssh_channel_new(session_);
        int rc = ssh_channel_open_session(channel);
        if(rc != SSH_OK)
        {
            emit logError(QString("SSH: Failed to open channel: %1").arg(ssh_get_error(session_)));
        }
        else
        {
            ssh_channel_request_exec(channel, command);

            int poll_rc;
            char readData[256];

            while((poll_rc = ssh_channel_poll(channel, 0)) != SSH_EOF)
            {
                if(poll_rc == SSH_ERROR)
                {
                    emit logError(QString("SSH: Error while reading from channel: %1").arg(ssh_get_error(session_)));
                    break;
                }
                int rc = ssh_channel_read(channel, readData, sizeof(readData)-1, 0);
                readData[rc] = 0;

                if(rc > 0)
                {
                   out << readData;
                }
            }

            success = true;

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

bool SSHInterface::runCommand(const char *command, char *resultbuffer, int bufferlen)
{
    //NOTE: The return value of this function only indicates whether or not the command was executed.
    //  It does not say whether or not the program that was called ran successfully. For that one has
    //  to parse the result buffer.
    bool success = false;
    if(isSessionConnected())
    {
        ssh_channel channel = ssh_channel_new(session_);
        int rc = ssh_channel_open_session(channel);
        if(rc != SSH_OK)
        {
            emit logError(QString("SSH: Failed to open channel: %1").arg(ssh_get_error(session_)));
        }
        else
        {
            ssh_channel_request_exec(channel, command);

            if(resultbuffer)
            {
                int nbytes = ssh_channel_read(channel, resultbuffer, bufferlen-1, 0);
                resultbuffer[nbytes] = 0;
            }

            success = true;

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

bool SSHInterface::uploadEntireFile(const char *localpath, const char *remotelocation, const char *remotefilename)
{
    uint8_t *filedata = 0;
    FILE *file = fopen(localpath, "r");
    if (!file)
    {
        emit logError(QString("Failed to open the file ") + localpath);
        return false;
    }

    if (fseek(file, 0L, SEEK_END) != 0)
    {
        emit logError(QString("Error while reading the file 1 ") + localpath);
        fclose(file);
        return false;
    }

    long bufsize = ftell(file);
    if (bufsize == -1)
    {
        emit logError(QString("Error while reading the file 2 ") + localpath);
        fclose(file);
        return false;
    }

    filedata = (uint8_t *)malloc((size_t)bufsize);

    if (fseek(file, 0L, SEEK_SET) != 0)
    {
        emit logError(QString("Error while reading the file 3 ") + localpath);
        free(filedata);
        fclose(file);
        return false;
    }

    size_t newLen = fread(filedata, 1, (size_t)bufsize, file);
    if (newLen == 0)
    {
        emit logError(QString("Error while reading the file 4 ") + localpath);
        free(filedata);
        fclose(file);
        return false;
    }

    fclose(file);

    bool success = writeFile(filedata, (size_t)bufsize, remotelocation, remotefilename);

    free(filedata);

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
            //TODO: This could be more robust!
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
    size_t lenpre = strlen(pre);
    size_t lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

bool SSHInterface::runSqlHandler(const char *command, const char *db, const char *tempfile, const QVector<QString> *extraParam)
{
    char commandbuf[512];
    char resultbuf[512];
    int len = sprintf(commandbuf, "./incaview/sqlhandler %s %s %s", command, db, tempfile);
    if(extraParam)
    {
        for(const QString &par : *extraParam)
        {
            len += sprintf(commandbuf + len, " %s", par.toLatin1().data());
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

void SSHInterface::generateRandomTransactionFileName(char *outfilename, const char *dbname)
{
    quint32 number = QRandomGenerator::global()->generate();

    sprintf(outfilename, "tmp%u%s.dat", number, dbname);

    //qDebug() << "Generated transaction file name: " << outfilename;
}

void SSHInterface::deleteTransactionFile(char *filename)
{
    char command[512];
    sprintf(command, "rm %s", filename);
    runCommand(command, 0, 0);
}

/*
void SSHInterface::getProjectList(const char *remoteDB, const char *username, QVector<ProjectSpec> &outdata)
{
    char tmpname[256];
    generateRandomTransactionFileName(tmpname, remoteDB);

    QVector<QString> extraparam;
    extraparam.push_back(username);

    bool success = runSqlHandler(EXPORT_PROJECT_LIST_COMMAND, remoteDB, tmpname, &extraparam);

    if(success)
    {
        void *filedata = 0;
        size_t filesize;
        success = readFile(&filedata, &filesize, tmpname);
        if(success)
        {
            uint8_t *at = (uint8_t *)filedata;
            while(at < (uint8_t *)filedata + filesize)
            {
                project_serial_entry *entry = (project_serial_entry *)at;
                at += sizeof(project_serial_entry);

                std::string namestr((char *)at, (char *)at + entry->namelen); //Is there a better way to get a QString from a range based char * (not nullterminated) than going via a std::string?
                at += entry->namelen;
                std::string dbnamestr((char *)at, (char *)at + entry->dbnamelen);
                at += entry->dbnamelen;
                std::string exenamestr((char *)at, (char *)at + entry->exenamelen);
                at += entry->exenamelen;

                outdata.push_back({QString::fromStdString(namestr), QString::fromStdString(dbnamestr), QString::fromStdString(exenamestr)});
            }
        }
        if(filedata) free(filedata);
    }

    deleteTransactionFile(tmpname);
}
*/


void SSHInterface::getStructureData(const char *remoteDB, const char *command, QVector<TreeData> &outdata)
{
    char tmpname[256];
    generateRandomTransactionFileName(tmpname, remoteDB);

    bool success = runSqlHandler(command, remoteDB, tmpname);

    if(success)
    {
        void *filedata = 0;
        size_t filesize;
        success = readFile(&filedata, &filesize, tmpname);
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

    deleteTransactionFile(tmpname);
}

void SSHInterface::getResultsStructure(const char *remoteDB, QVector<TreeData> &structuredata)
{
    getStructureData(remoteDB, EXPORT_RESULTS_STRUCTURE_COMMAND, structuredata);
}

/*
void SSHInterface::getParameterStructure(const char *remoteDB, QVector<TreeData> &structuredata)
{
    getStructureData(remoteDB, EXPORT_PARAMETER_STRUCTURE_COMMAND, structuredata);
}


void SSHInterface::getParameterValuesMinMax(const char *remoteDB, std::map<uint32_t, parameter_min_max_val_serial_entry>& IDtoParam)
{
    char tmpname[256];
    generateRandomTransactionFileName(tmpname, remoteDB);

    bool success = runSqlHandler(EXPORT_PARAMETER_VALUES_MIN_MAX_COMMAND, remoteDB, tmpname);

    if(success)
    {
        void *filedata = 0;
        size_t filesize;
        success = readFile(&filedata, &filesize, tmpname);
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

    deleteTransactionFile(tmpname);
}
*/

bool SSHInterface::getResultSets(const char *remoteDB, const QVector<int>& IDs, QVector<QVector<double>> &valuedata)
{
    char tmpname[256];
    generateRandomTransactionFileName(tmpname, remoteDB);

    QVector<QString> IDstrs;
    for(int ID : IDs)
    {
        IDstrs.push_back(QString::number(ID));
    }

    bool success = runSqlHandler(EXPORT_RESULT_VALUES_COMMAND, remoteDB, tmpname, &IDstrs);

    if(success)
    {
        void *filedata = 0;
        size_t filesize;
        success = readFile(&filedata, &filesize, tmpname);
        if(success)
        {
            uint8_t *data = (uint8_t *)filedata;
            uint64_t numresults = *(uint64_t *)data;
            data += sizeof(uint64_t);

            if((int)numresults == IDs.count())
            {
                valuedata.resize((int)numresults);

                //qDebug() << numresults;

                for(uint i = 0; i < numresults; ++i)
                {
                    //TODO: Check that we never overstep the filesize;
                    uint64_t count = *(uint64_t *)data;
                    data += sizeof(uint64_t);
                    int cnt = (int)count;

                    //qDebug() << cnt;

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

    deleteTransactionFile(tmpname);

    return success;
}

/*
void SSHInterface::writeParameterValues(const char *remoteDB, QVector<parameter_serial_entry>& writedata)
{
    char tmpname[256];
    generateRandomTransactionFileName(tmpname, remoteDB);

    uint64_t count = writedata.count();
    size_t size = sizeof(uint64_t) + count*sizeof(parameter_serial_entry);
    void *result = malloc(size);

    uint8_t *at = (uint8_t *)result;
    *(uint64_t *)at = count;
    at += sizeof(uint64_t);
    memcpy(at, writedata.data(), count*sizeof(parameter_serial_entry));

    bool success = writeFile(result, size, "~/", tmpname);
    free(result);

    if(success) runSqlHandler(IMPORT_PARAMETER_VALUES_COMMAND, remoteDB, tmpname);

    deleteTransactionFile(tmpname);
}
*/

void SSHInterface::runModel(const char *exename, const char *remoteDB, QProgressBar *progressBar)
{
    if(!isInstanceConnected())
    {
        //NOTE: should never happen if interface behaves correctly
        //TODO: log error
        return;
    }

    //NOTE: We have to create a new SSH session in the new thread. Two different threads can not use the same session in libssh,
    //or there is a risk of state corruption.

    //TODO: Check that we are not already running a remote INCA process from this application. (Should not happen if the UI behaves correctly though).

    SSHRunModelWorker *worker = new SSHRunModelWorker;
    worker->moveToThread(&incaWorkerThread_);

    connect(worker, &SSHRunModelWorker::resultReady, &incaWorkerThread_, &QThread::quit);
    connect(&incaWorkerThread_, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &SSHRunModelWorker::resultReady, this, &SSHInterface::handleIncaFinished);
    connect(worker, &SSHRunModelWorker::tick, this, &SSHInterface::handleIncaTick);

    //Relay logging signals
    connect(worker, &SSHRunModelWorker::log, this, &SSHInterface::log);
    connect(worker, &SSHRunModelWorker::reportError, this, &SSHInterface::handleRunINCAError);

    INCARunProgressBar_ = progressBar;
    INCARunProgressBar_->setVisible(true);

    incaWorkerThread_.start();

    //TODO: This should also take the name of the parameter database to use.
    worker->runModel(instanceUser_.data(), instanceIp_.data(), exename, "instancekey");
}


void SSHInterface::handleIncaFinished()
{
    if(INCARunProgressBar_)
    {
        INCARunProgressBar_->setVisible(false);
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
    INCARunProgressBar_->setValue(ticknum);
}

void SSHInterface::sendNoop()
{
    qDebug() << "SSH - we sent a No-op";

    //NOTE: This function is supposed to be called in a regular interval so that the session is not idle (and so that the firewall does not shut down
    // the connection).

    if(session_)
    {
        const char *ignorethismessageplease = "No-op";
        int rc = ssh_send_ignore(session_, ignorethismessageplease);
        if(rc == SSH_ERROR)
        {
            //TODO: This is bad, at least if we expected to be connected. Figure out how to handle it. (However it has not been triggered during testing, so maybe it is ok).
            qDebug() << "No-op caused error";
        }
    }
}


//--------------------------- SSHRunModelWorker --------------------------------------------------------------------


void SSHRunModelWorker::runModel(const char *user, const char *address, const char *exename, const char *keyfile)
{
    //NOTE: We have to create a new SSH session in the new thread. Two threads can not use the same session in libssh,
    //or there is a risk of state corruption.
    inca_run_session = ssh_new();
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

    inca_run_channel = ssh_channel_new(inca_run_session);
    rc = ssh_channel_open_session(inca_run_channel);
    if(rc != SSH_OK)
    {
        emit reportError(QString("SSH: Failed to open channel: %1").arg(ssh_get_error(inca_run_session)));
        ssh_free(inca_run_session);
        return;
    }

    char runcommand[512];
    //sprintf(runcommand, "cd incaview;./%s", exename);
    sprintf(runcommand, "rm results.db;./%s", exename); //TODO TODO TODO TODO: We should eventually not delete results.db, this should be handled differently
    ssh_channel_request_exec(inca_run_channel, runcommand);

    char readData[512];

    int poll_rc;
    while((poll_rc = ssh_channel_poll(inca_run_channel, 0)) != SSH_EOF)
    {
        if(poll_rc == SSH_ERROR)
        {
            emit reportError(QString("SSH: Error while reading from INCA run channel: %1").arg(ssh_get_error(inca_run_session)));
            break;
        }

        //TODO: Check that this sleep works correctly!
        QThread::msleep(50);

        int rc = ssh_channel_read_nonblocking(inca_run_channel, readData, sizeof(readData)-1, 0);
        readData[rc] = 0;

        if(rc > 0)
        {
            emit log(readData);
            //TODO: When we have an inca model that prints out its timesteps, parse this output and
            //emit tick(timestep); to update the progress bar.

            //NOTE: If the provided exe name is false or something else is wrong with the command we provided, that seems to be printed to
            // stderr, and so we don't catch it. Of course, we should never provide an exe name that is wrong, but it would be nice to log
            // any error in case it happens.
        }
    }

    ssh_channel_close(inca_run_channel);
    ssh_channel_free(inca_run_channel);
    inca_run_channel = 0;
    ssh_disconnect(inca_run_session);
    ssh_free(inca_run_session);
    inca_run_session = 0;

    emit resultReady();
}

SSHRunModelWorker::~SSHRunModelWorker()
{
    if(inca_run_channel)
    {
        //NOTE: This is not entirely thread safe since this could happen while the runINCA function was attempting to close the channel, but
        // that would only be an issue when the program is forcibly closed exactly at the same time as the run is finishing.

        //TODO: We may also want to send a signal to kill the remote process. See https://www.libssh.org/archive/libssh/2011-05/0000005.html
        // however we need a working instance of INCA that has significan run duration to test that properly.
        // Also, we should think about whether that is the right thing to do. It may cause database corruption at the far end if we are unlucky?
        ssh_channel_close(inca_run_channel);
        ssh_channel_free(inca_run_channel);
        inca_run_channel = 0;
    }
    if(inca_run_session)
    {
        ssh_disconnect(inca_run_session);
        ssh_free(inca_run_session);
        inca_run_session = 0;
    }
}
