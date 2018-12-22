/*
 * SIM900.cpp
 *
 *  Created on: 08 Aug 2018
 *      Author: cfurter
 */

#include "SIM900.h"
#include "caboodle/utils.h"

#define MAX_SOCK_NUM 3


/* Enable or disable the debug */
#define SIM_DEBUG_LVL0
#define SIM_DEBUG_LVL1

#ifdef SIM_DEBUG_LVL0
#define TRACE(_x, ...) INFO_TRACE("SIM900", _x, ##__VA_ARGS__)
#else
#define TRACE(_x, ...)
#endif

SIM900::SIM900(FileHandle *fh, DigitalInOut *reset, const char *apn, const char *username, const char *password) \
                                                                                        : mCMDParser(fh, "\r\n"), \
                                                                                        mReset(reset), \
                                                                                        mState(SIM900_UNKNOWN), \
                                                                                        mApn(apn), \
                                                                                        mUsername(username), \
                                                                                        mPassword(password), \
                                                                                        mCmdFailCount(SIM900_CMD_FAIL_COUNT)
{
    TRACE("Constructor\n");
    connectedCallback = 0;
    textMessageCallback = 0;

    mReset->output();
    mReset->write(1);
//    mReset->mode(OpenDrainPullUp);

    mTimer.start();
    mElapsed = mTimer.read_ms();

    printf("sim mutex: %p\n", &simMutex);
    mCMDParser.set_timeout(1000);

    manualCmdBusy = 0;
    mCMDParser.oob("+CUSD", callback(SIM900::handleUssd, this));
    mCMDParser.oob("+CMTI", callback(SIM900::handleNewTextMessage, this));
    mCMDParser.oob("+CMGR", callback(SIM900::parseTextMessage, this));
    mBufferFlag = 0;
    mCMDParser.debug_on(0);

#ifdef SIM_DEBUG_LVL1
    mCMDParser.debug_on(1);
#endif
}

SIM900::~SIM900()
{

}


void SIM900::parseTextMessage(SIM900 *_this)
{
    int i = 0;
    char line1Buffer[128];
    char line2Buffer[160];
    char *bufferPointer = line1Buffer;

    int c = _this->mCMDParser.getc();

    int newLineCount = 2;

    while(c != -1 && i < 288)
    {
        // escape the new lines and quotations
        if(c == '\n' || c == '\"')
        {
            c = _this->mCMDParser.getc();
            continue;
        }

        i++;
        if(c == '\r')
        {
            newLineCount--;
            *bufferPointer = 0;

            if(!newLineCount)
                break;

            bufferPointer = line2Buffer;
            c = _this->mCMDParser.getc();
            continue;
        }

        *bufferPointer = c;
        bufferPointer++;
        c = _this->mCMDParser.getc();
    }

    char *argv[5];
    int argc = 5;
    util_parse_params(line1Buffer, argv, &argc, ',');

    if(_this->textMessageCallback)
        _this->textMessageCallback(argv[1], line2Buffer);
}

void SIM900::handleNewTextMessage(SIM900 *_this)
{
    int i = 0;
    char buffer[256];
    int c = _this->mCMDParser.getc();

    while(c != -1 && i < 255)
    {
        if(c == '\r')
            break;
        buffer[i++] = c;
        c = _this->mCMDParser.getc();
    }

    char *argv[4];
    int argc = 4;
    util_parse_params(buffer, argv, &argc, ',');

    wait(1);

    // read the new message
    int msgIndex = atoi(argv[1]);
    _this->mCMDParser.send("AT+CMGR=%d", msgIndex);
    if(!_this->mCMDParser.recv("OK"))
    {
        printf("AT+CMGR=%d: ", msgIndex);
        printf(RED("FAIL\n"));
    }

    wait(1);

    // delete all the messages
    _this->mCMDParser.send("AT+CMGD=%d, 4", msgIndex);
    if(!_this->mCMDParser.recv("OK"))
    {
        printf("AT+CMGD=%d, 4: ", msgIndex);
        printf(RED("FAIL\n"));
    }
}

void SIM900::handleUssd(SIM900 *_this)
{
    printf(YELLOW("OOBs\n"));

    int i = 0;
    char buffer[256];
    int c = _this->mCMDParser.getc();

    while(c != -1 && i < 255)
    {
        buffer[i++] = c;
        c = _this->mCMDParser.getc();
    }
    buffer[i] = 0;
    printf(CYAN("USSD response: %s\n"), buffer);
    _this->mBufferFlag = 1;
}

void SIM900::sendUSSD(const char* ussd)
{
    simMutex.lock();

    printf("USSD: ");
    mCMDParser.send("AT+CUSD=1,\"%s\"", ussd);
    if(!mCMDParser.recv("OK"))
    {
        printf(RED("FAIL\n"));
        simMutex.unlock();
    }
    else
        printf(GREEN("Wait response\n"));


    mBufferFlag = 0;
    int timeout = 20;
    while(!mBufferFlag && timeout--)
    {
        mCMDParser.process_oob();
        wait(1);
    }

    simMutex.unlock();
}

void SIM900::resetDevice()
{
    mReset->write(0);
    wait(1);
    mReset->write(1);
}

void SIM900::setConnectCallback(void (*callback)(uint8_t id))
{
    if(callback)
        connectedCallback = callback;
}

void SIM900::setTextMessageCallback(void (*callback)(char *from, char *message))
{
    if(callback)
        textMessageCallback = callback;
}

int SIM900::connect(int ptl, const char *host, int port)
{
    if(mState < SIM900_CONNECTED)
        return -1;

    TRACE("Connect to host %s:%d\n", host, port);

    const char *protocol;
    if(ptl == TCP) {
        protocol = "TCP";
    } else if(ptl == UDP) {
        protocol = "UDP";
    } else {
        return -1;
    }

    mCMDParser.send("AT+CIPSTART=\"%s\",\"%s\",%d", protocol, host, port);

    char response[32];
    if(mCMDParser.recv("CONNECT %s\n", response))
    {
        if(!strcmp(response, "OK"))
            return 0;
    }

    return -1;
}

int SIM900::disconnect()
{
    TRACE("Disconnect\n");
    mCMDParser.send("AT+CIPCLOSE");
    char response[16];
    if(mCMDParser.recv("CLOSE %s\n", response))
    {
        if(!strcmp(response, "OK"))
            return 0;
    }

    return -1;
}

int SIM900::isConnected()
{
    if(mState < SIM900_CONNECTED)
        return -1;

    return 0;
//    simMutex.lock();
//    mCMDParser.send("AT+CIPSTATUS");
//    if(mCMDParser.recv("OK"))
//    {
//        simMutex.unlock();
//        return true;
//    }
//    simMutex.unlock();
//    return false;
}

int SIM900::send(unsigned char *data, int len)
{
    if(mState != SIM900_CONNECTED)
        return -1;

    simMutex.lock();
    mCMDParser.send("AT+CIPSEND=%d", len);
    if(mCMDParser.recv(">"))
    {
        const char *d = 0;
        d = (const char *)malloc(len);
        memcpy((char *)d, (char *)data, len);
        //write the data and the CTRL+Z escape character
        mCMDParser.write(d, len);
        char esc = 26;
        mCMDParser.write(&esc, 1);

        char response[32];
        if(mCMDParser.recv("SEND %s\n", response))
        {
#ifdef  SIM_DEBUG_LVL1
            printf("SEND %s data: ", response);
            for(int i=0; i < len; i++)
                printf(" %02X", data[i]);
            printf("\n");
#endif
            if(!strcmp(response, "OK"))
            {
                simMutex.unlock();
                return len;
            }
        }
    }

    simMutex.unlock();
    return -1;
}

int SIM900::receive(unsigned char *data, int len)
{
    if(mState != SIM900_CONNECTED)
        return -1;

    simMutex.lock();
    int mode = 2;
    int count = len;
    mCMDParser.send("AT+CIPRXGET=%d,%d", mode, count);

    int bytesInBuffer = 0;
    if(mCMDParser.recv("+CIPRXGET:%d,%d,%d\n", &mode, &count, &bytesInBuffer))
    {
        //take out the leading newline
        mCMDParser.getc();

        for(int i = 0; i < count; i++)
        {
            int a = mCMDParser.getc();
            if(a != -1)
                data[i]= a;
        }

        simMutex.unlock();
        return count;
    }


    simMutex.unlock();
    return -1;
}

int SIM900::sendTextMessage(const char *number, const char *message)
{
    simMutex.lock();
    printf("Sending text Message\n");

    mCMDParser.send("AT+CMGS=\"%s\"", number);
    if(mCMDParser.recv(">"))
    {
        //write the data and the CTRL+Z escape character
        mCMDParser.write(message, strlen(message));
        char esc = 26;
        mCMDParser.write(&esc, 1);

        int response = 0;
        wait(5);
        if(mCMDParser.recv("+CMGS: %d\n", &response))
        {
            printf("SMS Send response: %d\n", response);
            simMutex.unlock();
            return 1;
        }
    }

    simMutex.unlock();
    return -1;
}

int SIM900::atSend(const char* command)
{
    simMutex.lock();
    manualCmdBusy = 1;
    char response[64];

    mCMDParser.debug_on(1);
    //empty the buffer
    while (mCMDParser.recv("%s\n", response));

    mCMDParser.send(command);

    if(mCMDParser.recv("OK"))
    {
        printf("AT Response: %s\n", response);
        mCMDParser.debug_on(0);
        manualCmdBusy = 0;
        simMutex.unlock();
        return 1;
    }
    mCMDParser.debug_on(0);
    manualCmdBusy = 0;
    simMutex.unlock();
    return 0;
}

void SIM900::retryReset()
{
    mCmdFailCount = SIM900_CMD_FAIL_COUNT;
}

int SIM900::retryCommand()
{
    mCmdFailCount--;
    if(mCmdFailCount <= 0)
    {
        retryReset();
        return -1;
    }
    return 0;
}

void SIM900::run()
{
    if(manualCmdBusy)
        return;

    //run every 2 seconds
    int tick = mTimer.read_ms();
    if((tick - mElapsed) < 500)
        return;

    mElapsed = tick;

    simMutex.lock();
    switch(mState)
    {
        case SIM900_UNKNOWN:
        {
            TRACE("unknown\n");
            retryReset();
            mState = SIM900_RESET_DEVICE;
        }
        break;

        case SIM900_RESET_DEVICE:
        {
            TRACE("reset device\n");
            resetDevice();

            retryReset();
            mState = SIM900_POLL;
        }
        break;

        case SIM900_POLL:
        {
            mCMDParser.send("AT");
            if(!mCMDParser.recv("OK"))
            {
                if(retryCommand() == -1)
                    mState = SIM900_RESET_DEVICE;

                break;
            }

            wait(0.5);

            mCMDParser.send("ATE0");
            if(!mCMDParser.recv("OK"))
            {
                if(retryCommand() == -1)
                    mState = SIM900_RESET_DEVICE;

                break;
            }

            wait(0.5);

            // Set SMS Message Format as text
//            mCMDParser.send("AT+CMGF=1");
//            if(!mCMDParser.recv("OK"))
//            {
//                mCmdFailCount--;
//                if(mCmdFailCount <= 0)
//                {
//                    mCmdFailCount = SIM900_CMD_FAIL_COUNT;
//                    mState = SIM900_RESET_DEVICE;
//                }
//                break;
//            }

            TRACE(GREEN("Found\n"));
            retryReset();
            mState = SIM900_CHECK_SIM;
        }
        break;

        case SIM900_CHECK_SIM:
        {
            TRACE("state = SIM900_CHECK_SIM\n");

            mCMDParser.send("AT+CPIN?");
            char simState[16];
            if(!mCMDParser.recv("+CPIN: %s\n", simState))
            {
                if(retryCommand() == -1)
                    mState = SIM900_RESET_DEVICE;

                break;
            }

            if(!strcmp(simState, "READY"))
            {


                mCmdFailCount = 10;
                mState = SIM900_CHECK_REGISTRATION;
            }
        }
        break;

        case SIM900_CHECK_REGISTRATION:
        {
            //sim card found
            TRACE(GREEN("Check Registrations\n"));

            mCMDParser.send("AT+CREG=0");
            if(!mCMDParser.recv("OK"))
            {
                if(retryCommand() == -1)
                    mState = SIM900_CHECK_SIM;

                break;
            }

            //Check Signal
//            mCMDParser.send("AT+CSQ");
//            int rssi, ber;
//            if(!mCMDParser.recv("+CSQ: %d,%d\n", &rssi, &ber))
//            {
//                if(retryCommand() == -1)
//                    mState = SIM900_GPRS_ATTACHED;
//
//                break;
//            }
//            TRACE("+CSQ: %d,%d\n", rssi, ber);
//
//            wait(0.5);

            mCMDParser.send("AT+CREG?");
            int urc = 0;
            int stat = 0;
            if(!mCMDParser.recv("+CREG: %d, %d\n", &urc, &stat))
            {
                if(retryCommand() == -1)
                    mState = SIM900_CHECK_SIM;

                break;
            }

            TRACE(GREEN("+CREG: %d, %d\n"), urc, stat);

            if(stat == 1)
            {
                TRACE(GREEN("Registered\n"));

                retryReset();
                mState = SIM900_GPRS_ATTACHED;

                break;
            }

            if(retryCommand() == -1)
                mState = SIM900_CHECK_SIM;

        }
        break;

        case SIM900_GPRS_ATTACHED:
        {
            mCMDParser.send("AT+CGATT?");
            int state = 0;
            if(!mCMDParser.recv("+CGATT: %d\n", &state))
            {
                if(retryCommand() == -1)
                    mState = SIM900_CHECK_REGISTRATION;

                break;
            }

            if(state == 1)
            {
                TRACE(GREEN("Attached\n"));

                retryReset();
                mState = SIM900_CONNECT;

                break;
            }
            else
            {
                TRACE(RED("Not Attached\n"));

                if(retryCommand() == -1)
                    mState = SIM900_CHECK_REGISTRATION;

                break;
            }
        }
        break;

        case SIM900_CONNECT:
        {
            //Check Signal
            mCMDParser.send("AT+CSQ");
            int rssi, ber;
            if(!mCMDParser.recv("+CSQ: %d,%d\n", &rssi, &ber))
            {
                if(retryCommand() == -1)
                    mState = SIM900_GPRS_ATTACHED;

                break;
            }
            TRACE("+CSQ: %d,%d\n", rssi, ber);

            wait(0.5);

            //Select Multiple Connection
            mCMDParser.send("AT+CIPSHUT");
            if(!mCMDParser.recv("SHUT OK\n"))
            {
                if(retryCommand() == -1)
                    mState = SIM900_GPRS_ATTACHED;

                break;
            }

            wait(0.5);

            TRACE("Manual RX Data\n");
            mCMDParser.send("AT+CIPRXGET=1");
            if(!mCMDParser.recv("OK"))
            {
                if(retryCommand() == -1)
                    mState = SIM900_GPRS_ATTACHED;

                break;
            }

            wait(0.5);

            TRACE("Select single Connection\n");
            //Select Multiple Connection
            mCMDParser.send("AT+CIPMUX=0");
            if(!mCMDParser.recv("OK\n"))
            {
                if(retryCommand() == -1)
                    mState = SIM900_GPRS_ATTACHED;

                break;
            }

            wait(0.5);

            TRACE("Set APN\n");
            //set APN
            mCMDParser.send("AT+CIPCSGP=1,\"%s\",\"%s\",\"%s\"", mApn, mUsername, mPassword);
            if(!mCMDParser.recv("OK\n"))
            {
                if(retryCommand() == -1)
                    mState = SIM900_GPRS_ATTACHED;

                break;
            }

            wait(0.5);

            mCMDParser.send("AT+CSTT");//=\"%s\",\"%s\",\"%s\"", mApn, mUsername, mPassword);
            if(!mCMDParser.recv("OK\n"))
            {
                if(retryCommand() == -1)
                    mState = SIM900_GPRS_ATTACHED;

                break;
            }

            wait(0.5);

            //Bring up wireless connection
            mCMDParser.send("AT+CIICR");
            if(!mCMDParser.recv("OK\n"))
            {
                if(retryCommand() == -1)
                    mState = SIM900_GPRS_ATTACHED;

                break;
            }

            retryReset();
            mState = SIM900_GET_IP;
        }
        break;

        case SIM900_GET_IP:
        {
            //Get the local IP Address
            mCMDParser.send("AT+CIFSR");
            char ipAddr[32];
            if(!mCMDParser.recv("%s\n", ipAddr))
            {
                if(retryCommand() == -1)
                    mState = SIM900_CONNECT;

                break;
            }

            if(!strcmp(ipAddr, "ERROR"))
            {
                TRACE(RED("FAIL to get IP address\n"));
                if(retryCommand() == -1)
                    mState = SIM900_CONNECT;

                break;
            }

            TRACE("IP Address: %s\n", ipAddr);

            retryReset();
            mState = SIM900_CONNECTED;

            TRACE("Connected\n");
            if(connectedCallback)
                connectedCallback(1);
        }
        break;

        case SIM900_CONNECTED:
        {
            mCMDParser.send("AT+CGATT?");
            int state = 0;
            if(!mCMDParser.recv("+CGATT: %d\n", &state))
            {
                if(retryCommand() == -1)
                    mState = SIM900_CHECK_SIM;

                break;
            }

            if(state != 1)
            {
                if(retryCommand() == -1)
                    mState = SIM900_CHECK_SIM;

                break;
            }
        }
        break;
    }

    //unlock the mutex
    simMutex.unlock();

    //reset the counter
    mElapsed = tick;
}
