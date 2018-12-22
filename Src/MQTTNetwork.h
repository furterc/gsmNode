/*
 * MQTTNetwork.h
 *
 *  Created on: 09 Dec 2018
 *      Author: christo
 */

#ifndef SRC_MQTTNETWORK_H_
#define SRC_MQTTNETWORK_H_

#include "SIM900.h"

class MQTTNetwork
{
private:
    NetInterface *network;
public:
    MQTTNetwork(NetInterface *netInterface);
    virtual ~MQTTNetwork();

    int connect(char* hostname, int port);
    int disconnect();

    int read(unsigned char* buffer, int len, int timeout);
    int write(unsigned char* buffer, int len, int timeout);
};

#endif /* SRC_MQTTNETWORK_H_ */
