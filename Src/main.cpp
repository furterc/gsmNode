/*
 * main.cpp
 *
 *  Created on: 30 Nov 2018
 *      Author: christo
 */

#include "caboodle/console.h"
#include "caboodle/utils.h"
#include "mbed.h"
#include <Serial.h>
#include <SIM800L.h>
#include <UARTSerial.h>
#include "MQTTNetwork.h"

#include "MQTT/MQTTmbed.h"
#include "MQTT/MQTTClient.h"


//#define logMessage printf
//
//#define MQTTCLIENT_QOS2 1

/* Console */
Serial pc(SERIAL_TX, SERIAL_RX, 115200);

//SIM900 Modem
DigitalOut gsmPower(D7);
UARTSerial modem(D8, D2, 115200);
static SIM800L *sim = 0;

MQTTNetwork *network = 0;
MQTT::Client<MQTTNetwork, Countdown> *mClient = 0;

bool connected = false;

const char* topic = "mbed";

const char *SWdatetime  =__DATE__ " " __TIME__;

void atSendData(int argc,char *argv[])
{
    printf("GSM send : %s\n", argv[1]);
    sim->send((unsigned char*)argv[1], strlen(argv[1]));
}

void atSendCommand(int argc,char *argv[])
{
    sim->atSend(argv[1]);
}

const Console::cmd_list_t mainCommands[] =
{
      {"MAIN"    ,0,0,0},
      {"gs", "", "GSM send", atSendData},
      {"at", "", "at send command", atSendCommand},
      {0,0,0,0}
};

Console::cmd_list_t *Console::mCmdTable[] =
{
        (cmd_list_t*)shellCommands,
        (cmd_list_t*)mainCommands,
        0
};

void messageIn(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    printInfo("MSG PAYLOAD");
    printf("%.*s\n", message.payloadlen, (char*)message.payload);
}


void cellularConnected(uint8_t id)
{
    printf(GREEN("SIM900 Connected\n"));

    connected = true;

    if(!network->connect((char*)"160.119.253.41", 1883))
    {
        printf(GREEN("OK\n"));
        wait(1);

        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.MQTTVersion = 3;
        data.clientID.cstring = (char*)"mbed-sample";
        //      data.username.cstring = "testuser";
        //      data.password.cstring = "testpassword";

        printInfo("MQTT CLIENT");
        printf("%s\n", data.clientID.cstring);

        printInfo("MQTT CLIENT");
        int rc = mClient->connect(data);
        if(rc)
        {
            printf(RED("CONNECT FAIL : %d\n"), (int)rc);
            return;
        }
        else
        {
            printf(GREEN("CONNECT OK\n"));
        }

        wait(0.5);

        printInfo("MQTT Subscribe: ");
        if(mClient->subscribe("mbed", MQTT::QOS0, messageIn) == 0)
            printf(GREEN("OK"));
        else
            printf(RED("FAIL"));
    }
    else
        printf(RED("FAIL\n"));

}

void cellularDisconnect(uint8_t id)
{
    printf(RED("SIM900 Disconnected\n"));

    connected = false;
}

void runSim()
{
    printf("runSim: 0x%X\n", (int)Thread::gettid());

    while(1)
    {
        wait(0.5);
        sim->run();
    }
}


int main(void)
{
    printf("Hello\n");
    Console::init(&pc, "gsm_try");

    // initialize the Modem
    const char *apn = "internet";
    const char *username = "";
    const char *password = "";
    sim = new SIM800L(&modem, &gsmPower, apn, username, password);
    sim->setConnectCallback(cellularConnected);
    sim->setDisconnectCallback(cellularDisconnect);


    MQTTNetwork mqttNetwork(sim);
    network = &mqttNetwork;
    MQTT::Client<MQTTNetwork, Countdown> client = MQTT::Client<MQTTNetwork, Countdown>(mqttNetwork);
    mClient = &client;

    rtos::Thread simThread(osPriorityLow, OS_STACK_SIZE, NULL, "Console");
        simThread.start(runSim);

    while(1)
    {
        wait(0.1);

        if(connected)
        {
            client.yield(100);
        }
    }

    return -1;
}


