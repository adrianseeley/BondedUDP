#include "../include/BondedUDPSender.h"
#include "../include/BondedUDPShared.h"
#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

BondedUDPSender* BondedUDPSenderCreate(unsigned int maxPacketLength, unsigned int maxPayloadLength, struct sockaddr_in* localAddresses, struct sockaddr_in* remoteAddresses, unsigned int socketCount) {
    BondedUDPSender* videoUDPSender = malloc(sizeof(BondedUDPSender));
    if (videoUDPSender == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for BondedUDPSender.\n");
        exit(EXIT_FAILURE);
    }
    videoUDPSender->maxPacketLength      = maxPacketLength;
    videoUDPSender->maxPayloadLength     = maxPayloadLength;
    videoUDPSender->maxPacketBodyLength  = maxPacketLength - HEADER_LENGTH;
    videoUDPSender->maxPacketsPerPayload = (maxPayloadLength / videoUDPSender->maxPacketBodyLength) + 1;
    videoUDPSender->localAddresses       = localAddresses;
    videoUDPSender->remoteAddresses      = remoteAddresses;
    videoUDPSender->fds                  = malloc(socketCount * sizeof(int));
    if (videoUDPSender->fds == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for BondedUDPSender fds.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoUDPSender->fds, 0, socketCount * sizeof(int));
    for (int i = 0; i < socketCount; i++) {
        videoUDPSender->fds[i] = -1;
    }
    videoUDPSender->packet = malloc(maxPacketLength);
    if (videoUDPSender->packet == NULL) {
        fprintf(stderr, "Unable to allocate memory for BondedUDPSender packet.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < socketCount; i++) {
        videoUDPSender->fds[i] = BondedUDPSharedCreateSocket(&localAddresses[i]);
    }
    return videoUDPSender;
}

void BondedUDPSenderSendFrame(BondedUDPSender* videoUDPSender, uint64_t uTimestamp, void* payload, unsigned int payloadLength, unsigned int sendRounds) {
    if (payloadLength == 0) {
        fprintf(stderr, "Payload length was zero.\n");
        exit(EXIT_FAILURE);
    }
    if (payloadLength > videoUDPSender->maxPayloadLength) {
        fprintf(stderr, "Payload length was greater than max payload length.\n");
        exit(EXIT_FAILURE);
    }
    uint64_t beUTimestamp  = htobe64(uTimestamp);
    uint32_t packetCount   = (payloadLength + videoUDPSender->maxPacketBodyLength - 1) / videoUDPSender->maxPacketBodyLength;
    uint32_t bePacketCount = htonl(packetCount);
    for (unsigned int sendRoundIndex = 0; sendRoundIndex < sendRounds; sendRoundIndex++) {
        for (uint32_t packetIndex = 0; packetIndex < packetCount; packetIndex++) {
            uint32_t bePacketIndex      = htonl(packetIndex);
            uint32_t packetBodyLength   = (packetIndex == packetCount - 1) ? payloadLength - packetIndex * videoUDPSender->maxPacketBodyLength : videoUDPSender->maxPacketBodyLength;
            uint32_t bePacketBodyLength = htonl(packetBodyLength);
            memcpy(videoUDPSender->packet + HEADER_UTIMESTAMP_OFFSET, &beUTimestamp, HEADER_UTIMESTAMP_SIZE);
            memcpy(videoUDPSender->packet + HEADER_PACKET_INDEX_OFFSET, &bePacketIndex, HEADER_PACKET_INDEX_SIZE);
            memcpy(videoUDPSender->packet + HEADER_PACKET_COUNT_OFFSET, &bePacketCount, HEADER_PACKET_COUNT_SIZE);
            memcpy(videoUDPSender->packet + HEADER_BODY_LENGTH_OFFSET, &bePacketBodyLength, HEADER_BODY_LENGTH_SIZE);
            memcpy(videoUDPSender->packet + PACKET_BODY_START_OFFSET, payload + packetIndex * videoUDPSender->maxPacketBodyLength, packetBodyLength);
            ssize_t bytesSent = sendto(videoUDPSender->fd, videoUDPSender->packet, HEADER_LENGTH + packetBodyLength, 0, (struct sockaddr*)videoUDPSender->remoteAddress, sizeof(struct sockaddr_in));
            if (bytesSent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                fprintf(stderr, "Socket was misconfigured non-blocking.\n");
                exit(EXIT_FAILURE);
            }
            if (bytesSent < 0 && errno == EINTR) {
                packetIndex--;
                continue;
            }
            if (bytesSent < 0) {
                perror("Socket error.");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void BondedUDPSenderFree(BondedUDPSender* videoUDPSender) {
    if (videoUDPSender != NULL) {
        for (int i = 0; i < videoUDPSender->socketCount; i++) {
            if (videoUDPSender->fds[i] >= 0) {
                close(videoUDPSender->fds[i]);
                videoUDPSender->fds[i] = -1;
            }
        }
        free(videoUDPSender->fds);
        free(videoUDPSender->packet);
        free(videoUDPSender);
    }
}