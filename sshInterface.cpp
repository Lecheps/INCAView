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
            qDebug("Failed to create session.");
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
                qDebug("SSH: Failed to connect session");
                qDebug(ssh_get_error(session_));
            }
            else
            {
                //TODO: This should be wrapped in some safety stuff. See libssh tutorial.
                ssh_write_knownhost(session_);

                rc = ssh_userauth_privatekey_file(session_, 0, keyfile, 0);
                if(rc != SSH_AUTH_SUCCESS)
                {
                    qDebug("SSH: Failed to authenticate user");
                    qDebug(ssh_get_error(session_));
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

bool SSHInterface::runCommand(const char *command)
{
    bool success = false;
    if(session_)
    {
        //TODO: Optimize by holding channel open for later use once it is opened?
        ssh_channel channel = ssh_channel_new(session_);
        int rc = ssh_channel_open_session(channel);
        if(rc != SSH_OK)
        {
            qDebug("SSH: Failed to open channel");
            qDebug(ssh_get_error(session_));
        }
        else
        {
            ssh_channel_request_exec(channel, command);
            qDebug("Result of running ssh command:");
            char buffer[256];
            int nbytes = ssh_channel_read(channel, buffer, sizeof(buffer)-1, 0);
            buffer[nbytes] = 0;
            qDebug(buffer);

            success = true; //TODO: This only says that we were able to pass the command. Should we determine if the command was accepted?

            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
        }
        ssh_channel_free(channel);
    }
    else
    {
        qDebug("Tried to run SSH command without having an open session.");
    }

    return success;
}

bool SSHInterface::writeFile(const void *contents, size_t contentssize, const char *remotelocation, const char *remotefilename)
{
    bool success = false;

    if(session_)
    {
        ssh_scp scp = ssh_scp_new(session_, SSH_SCP_WRITE, remotelocation);
        if(scp)
        {
            int rc = ssh_scp_init(scp);
            if(rc != SSH_OK)
            {
                qDebug("Failed to initialize scp session");
                qDebug(ssh_get_error(session_));
            }
            else
            {
                rc = ssh_scp_push_file(scp, remotefilename, contentssize, S_IRUSR | S_IWUSR);
                if(rc != SSH_OK)
                {
                    qDebug("SSH: Failed to push file");
                    qDebug(ssh_get_error(session_));
                }
                else
                {
                    rc = ssh_scp_write(scp, contents, contentssize);
                    if(rc != SSH_OK)
                    {
                        qDebug("SSH: Failed to write to file");
                        qDebug(ssh_get_error(session_));
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
            qDebug("Failed to create scp");
        }
    }
    else
    {
        qDebug("Tried to run SCP write command without having an open session.");
    }

    return success;
}

bool SSHInterface::readFile(void **buffer, size_t* buffersize, const char *remotefilename)
{
    bool success = false;

    if(session_)
    {
        ssh_scp scp = ssh_scp_new(session_, SSH_SCP_READ, remotefilename);
        if(scp)
        {
            int rc = ssh_scp_init(scp);
            if(rc != SSH_OK)
            {
                qDebug("Failed to initialize scp session");
                qDebug(ssh_get_error(session_));
                ssh_scp_free(scp);
                return false;
            }

            rc = ssh_scp_pull_request(scp);
            //TODO: This should to be more robust!
            // The pull request can also return warnings and so on that could be handled.
            if(rc != SSH_SCP_REQUEST_NEWFILE)
            {
                qDebug("SSH: Error receiving information about file.");
                qDebug(ssh_get_error(session_));
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
                    qDebug("SSH: Error receiving file data.");
                    qDebug(ssh_get_error(session_));
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
                qDebug("SSH: Unexpected request.");
                qDebug(ssh_get_error(session_));
            }


            ssh_scp_close(scp);
            ssh_scp_free(scp);
        }
        else
        {
            qDebug("Failed to create scp.");
        }
    }
    else
    {
        qDebug("Tried to run SCP read command without having an open session.");
    }

    return success;
}


bool SSHInterface::runSqlHandler(const char *command, const char *db, const char *tempfile)
{
    if(isSessionConnected())
    {
        char buf[512];
        sprintf(buf, "./testdirectory/sqlhandler %s %s %s", command, db, tempfile);
        runCommand(buf);
    }
    else
    {
        qDebug("Tried to call the sqlhandler over ssh when ssh was not connected.");
    }
}

