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

//MQTT::Client<MQTTNetwork, Countdown> *mClient = 0;
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

int mqttPendingCommand = 0;

enum eModemStates{
    MQTT_CMD_UNKNOWN = 0,
    MQTT_CMD_CONNECT = 1,
    MQTT_CMD_IDLE = 2,
};

bool connected = false;

void cellularConnected(uint8_t id)
{
    printf(GREEN("SIM900 Cellular Connected\n"));
//    wait(1);
//
//
//    if(network == 0)
//    {
//        printf(RED("network == 0\n"));
//        return;
//    }
//
//    if(network->connect((char*)"160.119.253.41", 3333))
//    {
//        printf(RED("FAIL\n"));
//        return;
//    }
//
//    printf(GREEN("OK\n"));

//    wait(5);
//    //    connected = true;
////
//        printf("MQTT Subscribe: ");
//    if(mClient->subscribe("mbed", MQTT::QOS2, messageIn) == 0)
//        printf(GREEN("OK"));
//    else
//        printf(RED("FAIL"));

//    sim->connect(SIM800L::TCP, "160.119.253.41", 3333);
//    wait(5);

    mqttPendingCommand = MQTT_CMD_CONNECT;
//    led.setState(cLED::LED_FLASH_GREEN);
}

void cellularDisconnect(uint8_t id)
{
//    printf(GREEN("SIM900 Cellular Connected\n"));

//    printf("MQTT Subscribe: ");
//    if(mClient->subscribe("mbed", MQTT::QOS2, messageIn) == 0)
//        printf(GREEN("OK"));
//    else
//        printf(RED("FAIL"));
//    mqttNetwork->connect((char*)"160.119.253.41", 8086);

    printf(RED("SHIT Disconnected!\n"));
    mqttPendingCommand = MQTT_CMD_UNKNOWN;
//    sim->connect(SIM800L::TCP, "160.119.253.41", 3333);
    connected = false;
//    wait(5);

//    mqttPendingCommand = MQTT_CMD_CONNECT;
//    led.setState(cLED::LED_FLASH_GREEN);
}

void runSim()
{
    printf("runSim: 0x%X\n", (int)Thread::gettid());

        while(1)
        {
            wait(0.1);
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
//    mClient = &client;

    rtos::Thread simThread(osPriorityLow, OS_STACK_SIZE, NULL, "Console");
        simThread.start(runSim);

    while(1)
    {


        wait(0.1);

//        int r = sim->receive(data, 64);
//
//        printf("r : %d\n", r);
//        if(r > 0)
//            diag_dump_buf(data, r);

        switch(mqttPendingCommand)
        {
        case MQTT_CMD_CONNECT:
        {

            printf("mqttNetwork connect: ");
            if(!mqttNetwork.connect((char*)"160.119.253.41", 1883))
            {
                printf(GREEN("OK\n"));
                wait(1);

                mqttPendingCommand = MQTT_CMD_IDLE;

                MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
                data.MQTTVersion = 3;
                data.clientID.cstring = (char*)"mbed-sample";
                //      data.username.cstring = "testuser";
                //      data.password.cstring = "testpassword";

                printInfo("MQTT CLIENT");
                printf("%s\n", data.clientID.cstring);

                printInfo("MQTT CLIENT");
                int rc = client.connect(data);
                if(rc)
                {
                    printf(RED("CONNECT FAIL : %d\n"), (int)rc);
                    break;
                }
                else
                {
                    printf(GREEN("CONNECT OK\n"));
                }

                printf("MQTT Subscribe: ");
                wait(1);
//
//                client.connect();
                if(client.subscribe("mbed", MQTT::QOS0, messageIn) == 0)
                    printf(GREEN("OK"));
                else
                    printf(RED("FAIL"));
            }
            else
                printf(RED("FAIL\n"));

            mqttPendingCommand = MQTT_CMD_IDLE;

        }break;
        case MQTT_CMD_IDLE:
        {
            client.yield(100);
        }
        break;
        default:
            break;
        }

////        if(connected)
////        {
//////            static int count = 0;
//////            if(count++ > 100)
//////            {
//////                count = 0;
//////                unsigned char rxData[64];
//////                int len = sim->receive(rxData, 64);
//////                if(len > 0)
//////                {
//////                    printf("data in: ");
//////                    diag_dump_buf(rxData, len);
//////                }
//////            }
////
//////            char *data = (char*)"Hello daar\n";
////            //    mqttNetwork->write(data, strlen(data), 1);
//////            sim->send((unsigned char*)data, strlen(data));
//////            uint8_t res = rand() & 0xFF;
////
////
//////            sim->send((unsigned char*)&res, 1);
//////            printf("send: %d\n", res);
////        }
    }

    return -1;
}


