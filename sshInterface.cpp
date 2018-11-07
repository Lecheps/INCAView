#include "sshInterface.h"
#include <QDebug>
#include <QTime>
#include <QRandomGenerator>
#include <fstream>

//NOTE: Useful blog post on using QThread: https://mayaposch.wordpress.com/2011/11/01/how-to-really-truly-use-qthreads-the-full-explanation/

SSHInterface::SSHInterface(const char *hubIp, const char *hubUsername, const char *hubKey)
{
    session_ = nullptr;

    hubIp_ = hubIp;
    hubUsername_ = hubUsername;
    hubKey_ = hubKey;

    //NOTE: This initialization code should only be run once, so we can't
    // allow multiple instances of this class. This should not be a problem,
    // but we could make a static check just to be safe.
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
    if(isSessionConnected() && loggedInToInstance_)
    {
       destroyInstance();
    }

    if(isSessionConnected())
    {
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

    //NOTE: An instance will most of the time not already exist. The main example where it may exist is
    // if we crashed out of INCAView in a previous session so that it did not get to close down the
    // instance properly.

    //TODO: We should probably have a script running on the hub that closes down instances that have
    // been idle for more than e.g 8 hours.

    bool instancealreadyexists = false;

    {
        emit log(QString("Checking to see if instance ") + instancename + " already exists.");

        std::stringstream output;
        bool success = runCommand("gcloud compute instances list", output); //TODO: Check success and handle errors

        //NOTE: Format of what we have to parse to see if the instancename already exists:
        // (NOTE HOWEVER: google state that they don't guarantee the format of output from gcloud to stay constant!!!)
        // If we want to remove the uncertainty of this format maybe changing we have to make our own
        // python script that runs on the hub and calls into the google compute api so that we don't
        // rely on gcloud at all.

        // NAME   ZONE    MACHINE_TYPE    PREEMPTIBLE    INTERNAL_IP  EXTERNAL_IP  STATUS
        // instancename   europe-west2-a    n1-standard2  xx.xx.xx.xx  xx.xx.xx.xx  TERMINATED
        // ...

        //NOTE: split on whitespace:
        std::vector<std::string> words((std::istream_iterator<std::string>(output)),
                                         std::istream_iterator<std::string>());
        bool instanceisrunning = false;
        int foundipcount = 0;
        std::regex ippattern("[[:digit:]]+.[[:digit:]]+.[[:digit:]]+.[[:digit:]]+");
        for(std::string& word : words)
        {
            if(word == instancename)
            {
                instancealreadyexists = true;
            }
            if(instancealreadyexists)
            {
                if(std::regex_match(word, ippattern))
                {
                    if(foundipcount == 0) foundipcount = 1;
                    instanceIp_ = word;
                }
            }
            if(foundipcount == 1)
            {
                if(word == "RUNNING")
                {
                    instanceisrunning = true;
                    break;
                }
                else if(word == "TERMINATED") break;
            }
            if(foundipcount > 1) break;
        }

        if(instancealreadyexists && !instanceisrunning)
        {
            //TODO: Restart it I guess, but this is very unlikely to happen?
            qDebug() << "Found existing compute instance " << instancename << " and it was not running ";
        }
    }

    //NOTE: There is a case to make for it being better to kill an existing instance and recreate it
    // than it is to just reuse it. We have to think about this

    if(instancealreadyexists)
    {
        emit log("Instance found to already exist.");
    }
    else
    {
        emit log("Instance does not exist, attempting to create it. This may take a few seconds ...");

        char command[512];
        sprintf(command, "./createinstance.sh %s %s", instancename, username);

        std::stringstream output;
        bool success = runCommand(command, output); //TODO: Check success and handle errors

        //NOTE: Format of what we want to parse to get the external ip of the newly created instance.
        // (NOTE HOWEVER: google state that they don't guarantee the format of output from gcloud to stay constant!!!)
        // If we want to remove the uncertainty of this format maybe changing we have to make our own
        // python script that runs on the hub and calls into the google compute api so that we don't
        // rely on gcloud at all. (the createinstance.sh script currently calls gcloud)

        // NAME             ZONE            MACHINE_TYPE   PREEMPTIBLE  INTERNAL_IP  EXTERNAL_IP    STATUS
        // incaview-magnus  europe-west3-a  n1-standard-2               xx.xx.xx.xx  xx.xx.xx.xx  RUNNING

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

        emit log(QString("Created compute instance with ip ") + instanceIp_.data());
    }

    instanceName_ = instancename;
    instanceUser_ = username;


    //NOTE: download ssh keys for the instance.
    std::string privkeyfilename = std::string("keys/") + username;
    std::string pubkeyfilename = std::string("keys/") + username + ".pub";

    //TODO: If we use the libssh library in a more smart way, we may probably skip saving the keys to files here and instead provide the key data directly to libssh on the connect.

    success = downloadEntireFile("instancekey", privkeyfilename.data());
    //TODO: Check for success of download, and abort otherwise
    success = downloadEntireFile("instancekey.pub", pubkeyfilename.data());
    //TODO: Check for success of download, and abort otherwise

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

    std::stringstream out;
    runCommand("export PATH=$PATH:/home/magnus:/home/magnus/incaview", out, false); //our exe files are in these locations.

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
            emit logError("SSH: Unable to connect to the hub in order to destroy the compute instance.");
            return false;
        }
        loggedInToHub_ = true;
    }

    emit log(QString("SSH: Sending command to destroy compute instance ") + instanceName_.data() + " This may take a few seconds ...");

    char command[512];
    sprintf(command, "./destroyinstance.sh %s", instanceName_.data());

    std::stringstream output;
    bool success = runCommand(command, output);

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
    if(isSessionConnected())
    {
        return true;
    }

    if(session_)
    {
        ssh_free(session_);
        session_ = nullptr;
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
        session_ = nullptr;
        return false;
    }

    //NOTE: This registers the server as a known host on the local user computer. It should be ok to do this without any further checks since we only connect INCAView to servers we own?
    ssh_write_knownhost(session_);

    rc = ssh_userauth_privatekey_file(session_, nullptr, keyfile, nullptr);
    if(rc != SSH_AUTH_SUCCESS)
    {
        emit logError(QString("SSH: Failed to authenticate user: %1").arg(ssh_get_error(session_)));
        ssh_free(session_);
        session_ = nullptr;
        return false;
    }

    return true;
}

void SSHInterface::disconnectSession()
{
    if(session_)
    {
        ssh_disconnect(session_);
        ssh_free(session_);
        session_ = nullptr;
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

bool SSHInterface::runCommand(const char *command, std::stringstream &out, bool logAsItHappens)
{
    //NOTE: The return value of this function only indicates whether or not the command was executed.
    //  It does not say whether or not the program that was called ran successfully. For that one has
    //  to parse the out strngstream.

    if(!isSessionConnected())
    {
        emit logError(QString("SSH: Tried to run command \"%1\" without having an open ssh session.").arg(command));
        return false;
    }

    ssh_channel channel = ssh_channel_new(session_);
    int rc = ssh_channel_open_session(channel);
    if(rc != SSH_OK)
    {
        emit logError(QString("SSH: Failed to open channel: %1").arg(ssh_get_error(session_)));
        ssh_channel_free(channel);
        return false;
    }

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
           if(logAsItHappens)
           {
               emit log(QString(readData));
           }

           out << readData;
        }

        QThread::msleep(50); //TODO: We should check that this actually does what we want.
    }

    ssh_channel_close(channel);
    ssh_channel_free(channel);

    return true;
}


bool SSHInterface::writeFile(const void *contents, size_t contentssize, const char *remotelocation, const char *remotefilename)
{
    bool success = false;

    if(!isSessionConnected())
    {
        emit logError("SCP: Tried to run file writing command without having an open ssh session.");
        return false;
    }

    ssh_scp scp = ssh_scp_new(session_, SSH_SCP_WRITE, remotelocation);
    if(!scp)
    {
        emit logError("SCP: Failed to create an scp object.");
        return false;
    }

    int rc = ssh_scp_init(scp);
    if(rc != SSH_OK)
    {
        emit logError(QString("SCP: Failed to initialize session: %1").arg(ssh_get_error(session_)));
        ssh_scp_free(scp);
        return false;
    }

    rc = ssh_scp_push_file(scp, remotefilename, contentssize, S_IRUSR | S_IWUSR);
    if(rc != SSH_OK)
    {
        emit logError(QString("SCP: Failed to push file: %1").arg(ssh_get_error(session_)));
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        return false;
    }

    rc = ssh_scp_write(scp, contents, contentssize);
    ssh_scp_close(scp);
    ssh_scp_free(scp);

    if(rc != SSH_OK)
    {
        emit logError(QString("SCP: Failed to write to file: %1").arg(ssh_get_error(session_)));
        return false;
    }

    return true;
}

bool SSHInterface::readFile(void **buffer, size_t* buffersize, const char *remotefilename)
{
    bool success = false;

    if(!isSessionConnected())
    {
        emit logError("SCP: Tried to run file reading command without having an open ssh session.");
        return false;
    }

    ssh_scp scp = ssh_scp_new(session_, SSH_SCP_READ, remotefilename);
    if(!scp)
    {
        emit logError("SCP: Failed to create an scp object.");
        return false;
    }

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



    return success;
}

bool SSHInterface::uploadEntireFile(const char *localpath, const char *remotelocation, const char *remotefilename)
{
    std::ifstream file(localpath, std::ios::binary | std::ios::ate);

    if(file.fail())
    {
        emit logError(QString("Could not open file ") + localpath);
        return false;
    }

    size_t size = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);

    //qDebug() << "size of file was " << size;

    std::vector<char> buffer;
    buffer.resize(size);
    file.read(buffer.data(), size);

    if(file.fail())
    {
        emit logError(QString("Error while reading the file ") + localpath);
        file.close();
        return false;
    }

    bool success = writeFile(buffer.data(), buffer.size(), remotelocation, remotefilename);

    return success;
}

bool SSHInterface::downloadEntireFile(const char *localpath, const char *remotefilename)
{
    void *filebuf = nullptr;
    size_t filebufsize;

    bool success = readFile(&filebuf, &filebufsize, remotefilename);
    if(!success) return false;

    std::ofstream outfile (localpath, std::ofstream::binary);
    if(outfile.fail())
    {
        emit logError(QString("Failed to open local file ") + localpath);
        success = false;
    }
    else
    {
        outfile.write((char *)filebuf, filebufsize);
        if(outfile.fail())
        {
            emit logError(QString("Failed to write to file ") + localpath);
            success = false;
        }
    }

    outfile.close();

    if(filebuf) free(filebuf);

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
    //int len = sprintf(commandbuf, "./incaview/sqlhandler %s %s %s", command, db, tempfile);
    int len = sprintf(commandbuf, "/home/magnus/incaview/sqlhandler %s %s %s", command, db, tempfile);
    if(extraParam)
    {
        for(const QString &par : *extraParam)
        {
            len += sprintf(commandbuf + len, " %s", par.toLatin1().data());
        }
    }

    //qDebug(commandbuf);

    std::stringstream output;
    bool success = runCommand(commandbuf, output);

    //qDebug(resultbuf);

    if(startsWith("ERROR:", output.str().data()))
    {
        emit logError(QString("SSH: SQL: Unsuccessful operation on remote database:</br>&emsp;") + output.str().data());
        success = false;
    }

    return success;
}

void SSHInterface::deleteTransactionFile(const char *filename)
{
    char command[512];
    sprintf(command, "rm %s", filename);
    std::stringstream output;
    runCommand(command, output);
    //NOTE: It should not be necessary to parse the output of this?
}


bool SSHInterface::getStructureData(const char *remoteDB, const char *table, QVector<TreeData> &outdata)
{
    const char *tmpname = "data.dat";

    QVector<QString> extracommand;
    extracommand.push_back(QString(table));

    bool success = runSqlHandler(EXPORT_STRUCTURE_COMMAND, remoteDB, tmpname, &extracommand);

    if(success)
    {
        void *filedata = nullptr;
        size_t filesize;
        success = readFile(&filedata, &filesize, tmpname);
        if(success)
        {
            //qDebug() << "File size: " << filesize;

            //NOTE:
            // We expect to get a binary file on the following format:
            // the entire file is a series of structure entries repeated after each other.
            // each structure entry is a struct of type structure_serial_entry (128 bit) (see serialization.h):
            // structure_serial_entry: (parent_id (32bit uint), child_id (32bit uint), childnamelen (32bit uint), unitlen (32bit uint))
            // followed by
            // childname (childnamelen bytes char string (not 0-terminated))
            // unit      (unitlen      bytes char string (not 0-terminated))

            uint8_t *at = (uint8_t *)filedata;
            while(at < (uint8_t *)filedata + filesize)
            {
                structure_serial_entry *entry = (structure_serial_entry *)at;
                at += sizeof(structure_serial_entry);

                int parentID = (int)entry->parentID;
                int childID = (int)entry->childID;

                std::string namestr((char *)at, (char *)at + entry->childNameLen); //Is there a better way to get a QString from a range based char * (not nullterminated) than going via a std::string?
                at += entry->childNameLen;
                std::string unitstr((char *)at, (char *)at + entry->unitLen);
                at += entry->unitLen;

                //NOTE: Uncomment the following line to see what we got.
                //qDebug() << "parentid: " << parentID << "childid: " << childID << "name: " << namestr.data() << "unit: " << unitstr.data();

                outdata.push_back({childID, parentID, QString::fromStdString(namestr), QString::fromStdString(unitstr)});
            }
        }
        if(filedata) free(filedata);
    }

    deleteTransactionFile(tmpname);

    return success;
}


bool SSHInterface::getDataSets(const char *remoteDB, const QVector<int>& IDs, const char *table, QVector<QVector<double>> &valuedata, QVector<int64_t> &startdates)
{
    const char *tmpname = "data.dat";

    QVector<QString> IDstrs;
    IDstrs.push_back(QString(table));
    for(int ID : IDs)
    {
        IDstrs.push_back(QString::number(ID));
    }

    bool success = runSqlHandler(EXPORT_VALUES_COMMAND, remoteDB, tmpname, &IDstrs);

    if(success)
    {
        void *filedata = nullptr;
        size_t filesize;
        success = readFile(&filedata, &filesize, tmpname);
        if(success)
        {

            //NOTE:
            // We expect a to get a binary file on the following format:
            // numresults (64 bit uint)  - the number of result series that was returned by the request.
            // repeated numresults times:
            //      count (64 bit uint)  - the number of numbers in the current result series.
            //      repeated count times:
            //          double (64 bit float)

            uint8_t *data = (uint8_t *)filedata;

            uint64_t numresults = *(uint64_t *)data;
            data += sizeof(uint64_t);
            int64_t date = *(int64_t *)data;
            data += sizeof(int64_t);

            if((int)numresults == IDs.count())
            {
                valuedata.resize(numresults);
                startdates.resize(numresults);

                //qDebug() << "number of result series returned: " << numresults;

                for(uint i = 0; i < numresults; ++i)
                {
                    startdates[i] = date; //TODO: This is BROKEN!!!!!! We instead have to update the sqlhandler so that it sends one start date per timeseries.

                    //TODO: Check that we never overstep the filesize;
                    uint64_t count = *(uint64_t *)data;
                    data += sizeof(uint64_t);
                    size_t cnt = (size_t)count;

                    //qDebug() << "size of " << i << "th result series: " << cnt;

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
                emit logError(QString("SSH: SQL: Requested %1 data sets, got %2").arg(IDs.count()).arg(numresults));
                success = false;
            }
        }

        if(filedata) free(filedata);
    }

    deleteTransactionFile(tmpname);

    return success;
}


bool SSHInterface::createParameterDatabase(const char *remoteexename, const char *remoteparameterfile, const char *remoteparameterdb)
{
    if(!isInstanceConnected())
    {
        //NOTE: should never happen if interface behaves correctly
        //TODO: log error
        return false;
    }
    char command[512];

    //NOTE: We delete the existing parameter database. This is only necessary because the exe does not delete it when trying to overwrite.
    sprintf(command, "rm %s;/home/magnus/%s create_parameter_database %s %s", remoteparameterdb, remoteexename, remoteparameterfile, remoteparameterdb);

    //qDebug() << command;

    std::stringstream output;
    bool success = runCommand(command, output, true);

    //qDebug() << output.str().data();

    //TODO: Parse output to see if we actually succeeded?

    return success;
}


bool SSHInterface::exportParameters(const char *remoteexename, const char *remoteparameterdb, const char *remoteparameterfile)
{
    if(!isInstanceConnected())
    {
        //NOTE: should never happen if interface behaves correctly
        //TODO: log error
        return false;
    }
    char command[512];

    sprintf(command, "rm %s;/home/magnus/%s export_parameters %s %s", remoteparameterfile, remoteexename, remoteparameterdb, remoteparameterfile);

    std::stringstream output;
    bool success = runCommand(command, output, true);

    return success;
}


void SSHInterface::runModel(const char *exename, const char *remoteInputFile, const char *remotedbname)
{
    if(!isInstanceConnected())
    {
        //NOTE: should never happen if interface behaves correctly
        return;
    }

    //NOTE results.db and inputs.db are the output database files from the model. If we have run correctly once before, the database output behaves strange if the databases are already there,
    // so we delete them for now, but we should find another way to handle this eventually.
    char runcommand[512];

    sprintf(runcommand, "rm results.db; rm inputs.db;/home/magnus/%s run %s %s", exename, remoteInputFile, remotedbname);

    std::stringstream out;
    runCommand(runcommand, out, true);
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
