/*
 * MQTTNetwork.cpp
 *
 *  Created on: 09 Dec 2018
 *      Author: christo
 */

#include <MQTTNetwork.h>

MQTTNetwork::MQTTNetwork(NetInterface *netInterface) : network(netInterface)
{

}

MQTTNetwork::~MQTTNetwork()
{

}

int MQTTNetwork::connect(char* hostname, int port)
{
    return network->connect(SIM900::TCP, hostname, port);
}

int MQTTNetwork::disconnect()
{
    network->disconnect();

    return 0;
}

int MQTTNetwork::read(unsigned char* buffer, int len, int timeout)
{
    return network->receive(buffer, len);
}

int MQTTNetwork::write(unsigned char* buffer, int len, int timeout)
{
    return network->send(buffer, len);
}
