#ifndef BONDEDUDPRECEIVER_H
#define BONDEDUDPRECEIVER_H

#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct BondedUDPReceiver {
    unsigned int        maxPacketLength;
    unsigned int        maxPayloadLength;
    unsigned int        maxPacketBodyLength;
    unsigned int        maxPacketsPerPayload;
    struct sockaddr_in* localAddresses;
    unsigned int        socketCount;
    int*                fds;
    bool*               flags;
    void*               packet;
    void*               payloadBuffer;
    unsigned int        payloadBufferLength;
    uint64_t            uTimestamp;
} BondedUDPReceiver;

BondedUDPReceiver* BondedUDPReceiverCreate(unsigned int maxPacketLength, unsigned int maxPayloadLength, struct sockaddr_in* localAddresses, unsigned int socketCount);
bool               BondedUDPReceiverReceiveFrame(BondedUDPReceiver* videoUDPReceiver);
void               BondedUDPReceiverFree(BondedUDPReceiver* videoUDPReceiver);

#endif