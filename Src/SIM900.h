/*
 * SIM900.h
 *
 *  Created on: 08 Aug 2018
 *      Author: cfurter
 */

#ifndef SRC_SIM900_H_
#define SRC_SIM900_H_

#include "mbed.h"
#include "rtos.h"
#include "NetInterface.h"

#define SIM900_CMD_FAIL_COUNT   3

//#define SIM_DEBUG_LVL1

class SIM900 : public NetInterface
{
public:
    enum eModemStates_t{
        SIM900_UNKNOWN,
        SIM900_RESET_DEVICE,
        SIM900_POLL,
        SIM900_CHECK_SIM,
        SIM900_CHECK_REGISTRATION,
        SIM900_GPRS_ATTACHED,
        SIM900_CONNECT,
        SIM900_GET_IP,
        SIM900_CONNECTED
    };

    enum Protocol {
        CLOSED = 0,
        TCP    = 1,
        UDP    = 2,
    };

private:
    ATCmdParser mCMDParser;
    DigitalInOut *mReset;
    eModemStates_t mState;

    int mElapsed;
    Timer mTimer;

    const char* mApn;
    const char* mUsername;
    const char* mPassword;

    int mCmdFailCount;
    int manualCmdBusy;
    int mBufferFlag;
    uint32_t mIdleTick;

    void resetDevice();
    void (*connectedCallback)(uint8_t id);
    void (*textMessageCallback)(char *from, char *message);

    rtos::Mutex simMutex;

public:
    SIM900(FileHandle *fh, DigitalInOut *reset, const char *apn, const char *username, const char *password);
    virtual ~SIM900();

    void setConnectCallback(void (*callback)(uint8_t id));
    void setTextMessageCallback(void (*callback)(char *from, char *message));

    int connect(int ptl, const char *host, int port);
    int disconnect();

    int sendTextMessage(const char *number, const char *message);

    int atSend(const char* command);

    void sendUSSD(const char* ussd);

    static void parseTextMessage(SIM900 *_this);
    static void handleNewTextMessage(SIM900 *_this);
    static void handleUssd(SIM900 *_this);

    int send(unsigned char *data, int len);
    int receive(unsigned char *data, int len);
    int isConnected();

    int retryCommand();
    void retryReset();

    void run();
};

#endif /* SRC_SIM900_H_ */
