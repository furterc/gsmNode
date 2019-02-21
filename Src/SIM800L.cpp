/*
 * SIM900.cpp
 *
 *  Created on: 08 Aug 2018
 *      Author: cfurter
 */

#include <SIM800L.h>
#include "caboodle/utils.h"

#define MAX_SOCK_NUM 3


/* Enable or disable the debug */
#define SIM_DEBUG_LVL0
#define SIM_DEBUG_LVL1

#ifdef SIM_DEBUG_LVL0
#define TRACE(_x, ...) INFO_TRACE("SIM800L", _x, ##__VA_ARGS__)
#else
#define TRACE(_x, ...)
#endif

SIM800L::SIM800L(FileHandle *fh, DigitalOut *power, const char *apn, const char *username, const char *password) \
                                                                                        : mCMDParser(fh, "\r\n"), \
                                                                                        mPWR(power), \
                                                                                        mState(MODEM_UNKNOWN), \
                                                                                        mApn(apn), \
                                                                                        mUsername(username), \
                                                                                        mPassword(password), \
                                                                                        mCmdFailCount(SIM800L_CMD_FAIL_COUNT)
{
    connectedCallback = 0;
    disconnectCallback = 0;

    mPWR->write(1);

    mTimer.start();
    mElapsed = mTimer.read_ms();

    printf("sim mutex: %p\n", &simMutex);
    mCMDParser.set_timeout(2000);

    manualCmdBusy = 0;
    mCMDParser.oob("+CUSD", callback(SIM800L::handleUssd, this));
    mBufferFlag = 0;
    mCMDParser.debug_on(0);

    mRxHead = 0;
    mRxTail = 0;

#ifdef SIM_DEBUG_LVL1
    mCMDParser.debug_on(1);
#endif
}

SIM800L::~SIM800L()
{

}

void SIM800L::handleUssd(SIM800L *_this)
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

void SIM800L::sendUSSD(const char* ussd)
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

void SIM800L::resetDevice()
{
    TRACE("POWER OFF\n");
    mPWR->write(0);
    wait(10);
    TRACE("POWER ON\n");
    mPWR->write(1);
}

void SIM800L::setConnectCallback(void (*callback)(uint8_t id))
{
    if(callback)
        connectedCallback = callback;
}

void SIM800L::setDisconnectCallback(void (*callback)(uint8_t id))
{
    if(callback)
        disconnectCallback = callback;
}

int SIM800L::connect(int protocol, const char *host, int port)
{
    if(mState < MODEM_READYTOCONNECT)
        return -1;

    if(mState == MODEM_CONNECTED)
        return 0;

    TRACE("Connect to host %s:%d\n", host, port);

    const char *ptcl;
    if(protocol == TCP) {
        ptcl = "TCP";
    } else if(protocol == UDP) {
        ptcl = "UDP";
    } else {
        return -1;
    }

    mCMDParser.send("AT+CIPSTART=\"%s\",\"%s\",%d", ptcl, host, port);

    char response[32];
    if(mCMDParser.recv("CONNECT %s\n", response))
    {
        if(!strcmp(response, "OK"))
        {
            TRACE(GREEN("Connect OK\n"));
            mState = MODEM_CONNECTED;
            return 0;
        }
    }
    TRACE(RED("Connect FAIL\n"));
    return -1;
}

int SIM800L::disconnect()
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

int SIM800L::isConnected()
{
    if(mState < MODEM_CONNECTED)
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

int SIM800L::send(unsigned char *data, int len)
{
    printf("sb4\n");
    if(mState != MODEM_CONNECTED)
        return -1;

    printf("se\n");
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

    mState = MODEM_DISCONNECT;
    simMutex.unlock();
    return -1;
}

int SIM800L::receive(unsigned char *data, int len)
{
    if(mState < MODEM_READYTOCONNECT)
        return -1;

    simMutex.lock();

    uint8_t byteCount = 0;
    for(int i = 0; i < len; i++)
    {
        uint8_t byte = 0;
        int d = getRxByte(&byte);
        if(d == -1)
           break;

        byteCount ++;

        data[i] = byte;
    }
    simMutex.unlock();
    return byteCount;
    return len;
}

int SIM800L::atSend(const char* command)
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

    #ifndef SIM_DEBUG_LVL1
    mCMDParser.debug_on(0);
#endif

    manualCmdBusy = 0;
    simMutex.unlock();
    return 0;
}

void SIM800L::retryReset()
{
    mCmdFailCount = SIM800L_CMD_FAIL_COUNT;
}

int SIM800L::retryCommand()
{
    mCmdFailCount--;
    if(mCmdFailCount <= 0)
    {
        retryReset();
        return -1;
    }
    return 0;
}

void SIM800L::run()
{
    if(manualCmdBusy)
        return;

    //run every 2 seconds
    int tick = mTimer.read_ms();
    if((tick - mElapsed) < 500)
        return;

//

    mElapsed = tick;

    simMutex.lock();
    switch(mState)
    {
        case MODEM_UNKNOWN:
        {
            retryReset();
            mState = MODEM_RESET_DEVICE;
        }
        break;

        case MODEM_RESET_DEVICE:
        {
            TRACE("RESET DEVICE\n");
            resetDevice();

            retryReset();
            mState = MODEM_POLL;
        }
        break;

        case MODEM_POLL:
        {
            mCMDParser.send("AT");
            if(!mCMDParser.recv("OK"))
            {
                if(retryCommand() == -1)
                    mState = MODEM_RESET_DEVICE;

                break;
            }

            wait(0.5);

            mCMDParser.send("ATE0");
            if(!mCMDParser.recv("OK"))
            {
                if(retryCommand() == -1)
                    mState = MODEM_RESET_DEVICE;

                break;
            }

            TRACE(GREEN("found\n"));
            retryReset();
            mState = MODEM_CHECK_SIM;
        }
        break;

        case MODEM_CHECK_SIM:
        {
            mCMDParser.send("AT+CPIN?");
            char simState[16];
            if(!mCMDParser.recv("+CPIN: %s\n", simState))
            {
                if(retryCommand() == -1)
                {
                    TRACE(RED("+CPIN: %s\n"), simState);
                    mState = MODEM_RESET_DEVICE;
                }

                break;
            }

            if(!strcmp(simState, "READY"))
            {
                TRACE(GREEN("+CPIN: READY\n"));
                mCmdFailCount = 10;
                mState = MODEM_CHECK_REGISTRATION;
            }
        }
        break;

        case MODEM_CHECK_REGISTRATION:
        {
            //sim card found
            mCMDParser.send("AT+CREG=0");
            if(!mCMDParser.recv("OK"))
            {
                if(retryCommand() == -1)
                    mState = MODEM_CHECK_SIM;

                break;
            }

            mCMDParser.send("AT+CREG?");
            int urc = 0;
            int stat = 0;
            if(!mCMDParser.recv("+CREG: %d, %d\n", &urc, &stat))
            {
                if(retryCommand() == -1)
                    mState = MODEM_CHECK_SIM;

                break;
            }

            TRACE("+CREG: %d, %d\n", urc, stat);

            if(stat == 1)
            {
                TRACE(GREEN("Registered\n"));

                retryReset();
                mState = MODEM_GPRS_ATTACHED;

                break;
            }

            if(retryCommand() == -1)
                mState = MODEM_CHECK_SIM;

        }
        break;

        case MODEM_GPRS_ATTACHED:
        {
            mCMDParser.send("AT+CGATT?");
            int state = 0;
            if(!mCMDParser.recv("+CGATT: %d\n", &state))
            {
                if(retryCommand() == -1)
                    mState = MODEM_CHECK_REGISTRATION;

                break;
            }

            TRACE("+CGATT: %d\n", state);
            if(state == 1)
            {
                TRACE(GREEN("Attached\n"));

                retryReset();
                mState = MODEM_CONNECT;

                break;
            }
            else
            {
                TRACE(RED("Not Attached\n"));

                if(retryCommand() == -1)
                    mState = MODEM_CHECK_REGISTRATION;

                break;
            }
        }
        break;

        case MODEM_CONNECT:
        {
            //Check Signal
            mCMDParser.send("AT+CSQ");
            int rssi, ber;
            if(!mCMDParser.recv("+CSQ: %d,%d\n", &rssi, &ber))
            {
                if(retryCommand() == -1)
                    mState = MODEM_GPRS_ATTACHED;

                break;
            }
            TRACE("+CSQ: %d,%d\n", rssi, ber);

            wait(1);

            //Select Multiple Connection
            mCMDParser.send("AT+CIPSHUT");
            if(!mCMDParser.recv("SHUT OK\n"))
            {
                if(retryCommand() == -1)
                    mState = MODEM_GPRS_ATTACHED;

                break;
            }

            wait(0.5);

            mCMDParser.send("AT+CIPRXGET=0");
            if(!mCMDParser.recv("OK"))
            {
                if(retryCommand() == -1)
                    mState = MODEM_GPRS_ATTACHED;

                break;
            }

            wait(0.5);

            //Select Multiple Connection
            mCMDParser.send("AT+CIPMUX=0");
            if(!mCMDParser.recv("OK\n"))
            {
                if(retryCommand() == -1)
                    mState = MODEM_GPRS_ATTACHED;

                break;
            }

            wait(0.5);

            //set APN
            mCMDParser.send("AT+CIPCSGP=1,\"%s\",\"%s\",\"%s\"", mApn, mUsername, mPassword);
            if(!mCMDParser.recv("OK\n"))
            {
                if(retryCommand() == -1)
                    mState = MODEM_GPRS_ATTACHED;

                break;
            }

            wait(0.5);

            mCMDParser.send("AT+CSTT");//=\"%s\",\"%s\",\"%s\"", mApn, mUsername, mPassword);
            if(!mCMDParser.recv("OK\n"))
            {
                if(retryCommand() == -1)
                    mState = MODEM_GPRS_ATTACHED;

                break;
            }

            wait(0.5);

            //Bring up wireless connection
            mCMDParser.send("AT+CIICR");
            if(!mCMDParser.recv("OK\n"))
            {
                if(retryCommand() == -1)
                    mState = MODEM_GPRS_ATTACHED;

                break;
            }

            retryReset();
            mState = MODEM_GET_IP;
        }
        break;

        case MODEM_GET_IP:
        {
            mCMDParser.send("AT+CIPCLOSE");
            char response[16];
            if(mCMDParser.recv("CLOSE %s\n", response))
            {
                if(strcmp(response, "OK"))
                {
                    if(retryCommand() == -1)
                        mState = MODEM_CONNECT;
                }
            }

            //Get the local IP Address
            mCMDParser.send("AT+CIFSR");
            char ipAddr[32];
            if(!mCMDParser.recv("%s\n", ipAddr))
            {
                if(retryCommand() == -1)
                    mState = MODEM_CONNECT;

                break;
            }

            if(!strcmp(ipAddr, "ERROR"))
            {
                TRACE(RED("FAIL to get IP address\n"));
                if(retryCommand() == -1)
                    mState = MODEM_CONNECT;

                break;
            }

            TRACE("IP: %s\n", ipAddr);

            retryReset();
            mState = MODEM_READYTOCONNECT;

            TRACE(GREEN("Connected\n"));
            if(connectedCallback)
                connectedCallback(1);
        }
        break;

        case MODEM_DISCONNECT:
        {
            TRACE(RED("Modem Disconnect\n"));
            if(disconnectCallback)
                disconnectCallback(1);
            mState = MODEM_CONNECT;
        }
        break;

        case MODEM_READYTOCONNECT:
        {

        }
        break;

        case MODEM_CONNECTED:
        {

            int a = -1;
            do {
                a = mCMDParser.getc();
                if(a != -1)
                {
                    handleRxByte(a);
                }
            }
            while(a != -1);

        }
        break;
    }

    //unlock the mutex
    simMutex.unlock();

    //reset the counter
    mElapsed = tick;
}

uint8_t connectCloseCount = 0;
static uint8_t connectClose[11] = {0x0D, 0x0A, 0x43, 0x4C, 0x4F, 0x53, 0x45, 0x44,  0x0D, 0x0A, 0x00};

void SIM800L::handleRxByte(uint8_t data)
{
    if(!connectCloseCount && (data == connectClose[0]))
        connectCloseCount++;

    if(connectCloseCount)
    {
        if(data != connectClose[connectCloseCount])
            connectCloseCount = 0;

        connectCloseCount++;
        if(connectCloseCount == 10)
        {
            printf(RED("Connection kakked\n"));
            mRxHead = mRxTail = 0;
            mState = MODEM_DISCONNECT;
        }
    }

    //escape new lines
    if(data == 0x0A)
        return;

    mRxBuffer[mRxTail++] = data;
    mRxTail %= SIM800L_RXBUFFER_SIZE;

    if(mRxHead == mRxTail)
    {
        printf("mRxHead == mRxTail\n");
    }
}

int SIM800L::getRxByte(uint8_t *data)
{
    if(mRxHead == mRxTail)
        return -1;

    *data = mRxBuffer[mRxHead++];
    return 0;
}

