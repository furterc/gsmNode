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
#include <UARTSerial.h>
#include <SIM900.h>
#include "MQTTNetwork.h"

#include "MQTT/MQTTmbed.h"
#include "MQTT/MQTTClient.h"

/* Console */
Serial pc(SERIAL_TX, SERIAL_RX, 115200);

//SIM900 Modem
DigitalInOut gsmReset(D7);
UARTSerial modem(D8, D2, 115200);
SIM900 *sim = 0;

MQTT::Client<MQTTNetwork, Countdown> *mClient = 0;
const char* topic = "mbed";

const char *SWdatetime  =__DATE__ " " __TIME__;

const Console::cmd_list_t mainCommands[] =
{
      {"MAIN"    ,0,0,0},
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
    printf(GREEN("SIM900 Cellular Connected\n"));

//    printf("MQTT Subscribe: ");
//    if(mClient->subscribe("mbed", MQTT::QOS2, messageIn) == 0)
//        printf(GREEN("OK"));
//    else
//        printf(RED("FAIL"));
//    mqttNetwork->connect((char*)"160.119.253.41", 8086);

    sim->connect(SIM900::TCP, "160.119.253.41", 8086);
    wait(5);
    char *data = (char*)"Hello daar\n";
//    mqttNetwork->write(data, strlen(data), 1);
    sim->send((unsigned char*)data, strlen(data));
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
    sim = new SIM900(&modem, &gsmReset, apn, username, password);
    sim->setConnectCallback(cellularConnected);


    MQTTNetwork mqttNetwork(sim);
    MQTT::Client<MQTTNetwork, Countdown> client = MQTT::Client<MQTTNetwork, Countdown>(mqttNetwork);
    mClient = &client;

    while(1)
    {
        sim->run();
        wait(5);

//        const char *data = "Hello daar\n";
//        sim->send(data, strlen(data));
    }

    return -1;
}


