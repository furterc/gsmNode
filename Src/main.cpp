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

/* Console */
Serial pc(SERIAL_TX, SERIAL_RX, 115200);

//SIM900 Modem
DigitalOut gsmPower(D7);
UARTSerial modem(D8, D2, 115200);
SIM800L *sim = 0;

MQTT::Client<MQTTNetwork, Countdown> *mClient = 0;
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

bool connected = false;

void cellularConnected(uint8_t id)
{
//    printf(GREEN("SIM900 Cellular Connected\n"));

//    printf("MQTT Subscribe: ");
//    if(mClient->subscribe("mbed", MQTT::QOS2, messageIn) == 0)
//        printf(GREEN("OK"));
//    else
//        printf(RED("FAIL"));
//    mqttNetwork->connect((char*)"160.119.253.41", 8086);

    sim->connect(SIM800L::TCP, "160.119.253.41", 3333);
    connected = true;
//    wait(5);

//    mqttPendingCommand = MQTT_CMD_CONNECT;
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
//    sim->connect(SIM800L::TCP, "160.119.253.41", 3333);
    connected = false;
//    wait(5);

//    mqttPendingCommand = MQTT_CMD_CONNECT;
//    led.setState(cLED::LED_FLASH_GREEN);
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
    MQTT::Client<MQTTNetwork, Countdown> client = MQTT::Client<MQTTNetwork, Countdown>(mqttNetwork);
    mClient = &client;

    while(1)
    {
        sim->run();

        wait(0.1);

        if(connected)
        {
            static int count = 0;
            if(count++ > 100)
            {
                count = 0;
                unsigned char rxData[64];
                int len = sim->receive(rxData, 64);
                if(len > 0)
                {
                    printf("data in: ");
                    diag_dump_buf(rxData, len);
                }
            }

//            char *data = (char*)"Hello daar\n";
            //    mqttNetwork->write(data, strlen(data), 1);
//            sim->send((unsigned char*)data, strlen(data));
//            uint8_t res = rand() & 0xFF;


//            sim->send((unsigned char*)&res, 1);
//            printf("send: %d\n", res);
        }
    }

    return -1;
}


