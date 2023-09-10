#ifndef BONDEDUDPSENDER_H
#define BONDEDUDPSENDER_H

#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct BondedUDPSender {
    unsigned int        maxPacketLength;
    unsigned int        maxPayloadLength;
    unsigned int        maxPacketBodyLength;
    unsigned int        maxPacketsPerPayload;
    struct sockaddr_in* localAddresses;
    struct sockaddr_in* remoteAddresses;
    unsigned int        socketCount;
    int*                fds;
    void*               packet;
} BondedUDPSender;

BondedUDPSender* BondedUDPSenderCreate(unsigned int maxPacketLength, unsigned int maxPayloadLength, struct sockaddr_in* localAddresses, struct sockaddr_in* remoteAddresses, unsigned int socketCount);
void             BondedUDPSenderSendFrame(BondedUDPSender* videoUDPSender, uint64_t uTimestamp, void* payload, unsigned int payloadLength, unsigned int sendRounds);
void             BondedUDPSenderFree(BondedUDPSender* videoUDPSender);

#endif