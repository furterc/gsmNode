/*
 * SIM900.h
 *
 *  Created on: 08 Aug 2018
 *      Author: cfurter
 */

#ifndef SRC_SIM800L_H_
#define SRC_SIM800L_H_

#include "mbed.h"
#include "rtos.h"
#include "NetInterface.h"

#define SIM800L_CMD_FAIL_COUNT   3

class SIM800L : public NetInterface
{
public:
    enum eModemStates_t{
        MODEM_UNKNOWN,
        MODEM_RESET_DEVICE,
        MODEM_POLL,
        MODEM_CHECK_SIM,
        MODEM_CHECK_REGISTRATION,
        MODEM_GPRS_ATTACHED,
        MODEM_CONNECT,
        MODEM_GET_IP,
        MODEM_DISCONNECT,
        MODEM_CONNECTED
    };

    enum Protocol {
        CLOSED = 0,
        TCP    = 1,
        UDP    = 2,
    };

private:
    ATCmdParser mCMDParser;
    DigitalOut *mPWR;
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
    void (*disconnectCallback)(uint8_t id);

    rtos::Mutex simMutex;

public:
    SIM800L(FileHandle *fh, DigitalOut *power, const char *apn, const char *username, const char *password);
    virtual ~SIM800L();

    void setConnectCallback(void (*callback)(uint8_t id));
    void setDisconnectCallback(void (*callback)(uint8_t id));

    int connect(int protocol, const char *host, int port);
    int disconnect();

    int atSend(const char* command);

    void sendUSSD(const char* ussd);
    static void handleUssd(SIM800L *_this);

    int send(unsigned char *data, int len);
    int receive(unsigned char *data, int len);
    int isConnected();

    int retryCommand();
    void retryReset();

    void run();
};

#endif /* SRC_SIM800L_H_ */
