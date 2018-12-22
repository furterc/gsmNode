/*
 * NetInterface.h
 *
 *  Created on: 16 Aug 2018
 *      Author: cfurter
 */

#ifndef SRC_NETINTERFACE_H_
#define SRC_NETINTERFACE_H_

class NetInterface
{
public:
    NetInterface(){};
    virtual ~NetInterface() {};

    virtual int connect(int ptl, const char *host, int port) = 0;
    virtual int disconnect() = 0;

    virtual int send(unsigned char *data, int len) = 0;
    virtual int receive(unsigned char *data, int len) = 0;
    virtual int isConnected() = 0;
};

#endif /* SRC_NETINTERFACE_H_ */
