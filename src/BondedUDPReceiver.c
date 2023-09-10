#include "../include/BondedUDPReceiver.h"
#include "../include/BondedUDPShared.h"
#include <endian.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

BondedUDPReceiver* BondedUDPReceiverCreate(unsigned int maxPacketLength, unsigned int maxPayloadLength, struct sockaddr_in* localAddresses, unsigned int socketCount) {
    BondedUDPReceiver* videoUDPReceiver = malloc(sizeof(BondedUDPReceiver));
    if (videoUDPReceiver == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for BondedUDPReceiver.\n");
        exit(EXIT_FAILURE);
    }
    videoUDPReceiver->maxPacketLength      = maxPacketLength;
    videoUDPReceiver->maxPayloadLength     = maxPayloadLength;
    videoUDPReceiver->maxPacketBodyLength  = maxPacketLength - HEADER_LENGTH;
    videoUDPReceiver->maxPacketsPerPayload = (maxPayloadLength / videoUDPReceiver->maxPacketBodyLength) + 1;
    videoUDPReceiver->localAddresses       = localAddresses;
    videoUDPReceiver->socketCount          = socketCount;
    videoUDPReceiver->fds                  = malloc(socketCount * sizeof(int));
    if (videoUDPReceiver->fds == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for BondedUDPReceiver fds.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoUDPReceiver->fds, 0, socketCount * sizeof(int));
    for (int i = 0; i < socketCount; i++) {
        videoUDPReceiver->fds[i] = -1;
    }
    videoUDPReceiver->flags = malloc(videoUDPReceiver->maxPacketsPerPayload * sizeof(bool));
    if (videoUDPReceiver->flags == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for BondedUDPReceiver flags.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoUDPReceiver->flags, 0, videoUDPReceiver->maxPacketsPerPayload * sizeof(bool));
    videoUDPReceiver->packet = malloc(maxPacketLength);
    if (videoUDPReceiver->packet == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for BondedUDPReceiver packet buffer.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoUDPReceiver->packet, 0, maxPacketLength);
    videoUDPReceiver->payloadBuffer = malloc(maxPayloadLength);
    if (videoUDPReceiver->payloadBuffer == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for BondedUDPReceiver payload buffer.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoUDPReceiver->payloadBuffer, 0, maxPayloadLength);
    for (int i = 0; i < socketCount; i++) {
        videoUDPReceiver->fds[i] = BondedUDPSharedCreateSocket(&localAddresses[i]);
    }
    return videoUDPReceiver;
}

bool BondedUDPReceiverReceiveFrame(BondedUDPReceiver* videoUDPReceiver) {
    uint64_t trackedUTimestamp            = 0;
    bool     trackedUTimestampInitialized = false;
    uint32_t packetsFlagged               = 0;
    for (;;) {
        ssize_t bytesReceived = recvfrom(videoUDPReceiver->fd, videoUDPReceiver->packet, videoUDPReceiver->maxPacketLength, 0, NULL, NULL);
        if (bytesReceived < 0 && errno == EINTR) {
            continue;
        }
        if (bytesReceived < 0 && errno == EBADF) {
            return false;
        }
        if (bytesReceived < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            fprintf(stderr, "Socket was misconfigured non-blocking.\n");
            exit(EXIT_FAILURE);
        }
        if (bytesReceived < 0) {
            perror("Socket error.");
            exit(EXIT_FAILURE);
        }
        if (bytesReceived == 0) {
            fprintf(stderr, "Received 0 length packet.\n");
            exit(EXIT_FAILURE);
        }
        if (bytesReceived < HEADER_LENGTH) {
            fprintf(stderr, "Socket partial receive.\n");
            exit(EXIT_FAILURE);
        }
        uint64_t beUTimestamp;
        uint32_t bePacketIndex;
        uint32_t bePacketCount;
        uint32_t bePacketBodyLength;
        memcpy(&beUTimestamp, videoUDPReceiver->packet + HEADER_UTIMESTAMP_OFFSET, HEADER_UTIMESTAMP_SIZE);
        memcpy(&bePacketIndex, videoUDPReceiver->packet + HEADER_PACKET_INDEX_OFFSET, HEADER_PACKET_INDEX_SIZE);
        memcpy(&bePacketCount, videoUDPReceiver->packet + HEADER_PACKET_COUNT_OFFSET, HEADER_PACKET_COUNT_SIZE);
        memcpy(&bePacketBodyLength, videoUDPReceiver->packet + HEADER_BODY_LENGTH_OFFSET, HEADER_BODY_LENGTH_SIZE);
        uint64_t uTimestamp       = be64toh(beUTimestamp);
        uint32_t packetIndex      = ntohl(bePacketIndex);
        uint32_t packetCount      = ntohl(bePacketCount);
        uint32_t packetBodyLength = ntohl(bePacketBodyLength);
        if (HEADER_LENGTH + packetBodyLength != bytesReceived) {
            fprintf(stderr, "Packet length mismatch\n");
            exit(EXIT_FAILURE);
        }
        memcpy(videoUDPReceiver->payloadBuffer + (packetIndex * videoUDPReceiver->maxPacketBodyLength), videoUDPReceiver->packet + PACKET_BODY_START_OFFSET, packetBodyLength);
        if (!trackedUTimestampInitialized || trackedUTimestamp != uTimestamp) {
            trackedUTimestamp            = uTimestamp;
            trackedUTimestampInitialized = true;
            packetsFlagged               = 0;
            memset(videoUDPReceiver->flags, 0, videoUDPReceiver->maxPacketsPerPayload * sizeof(bool));
        }
        if (videoUDPReceiver->flags[packetIndex]) {
            continue;
        }
        if (packetIndex == packetCount - 1) {
            videoUDPReceiver->payloadBufferLength = (packetCount - 1) * videoUDPReceiver->maxPacketBodyLength + packetBodyLength;
            videoUDPReceiver->uTimestamp          = trackedUTimestamp;
        }
        videoUDPReceiver->flags[packetIndex] = true;
        packetsFlagged++;
        if (packetsFlagged == packetCount) {
            return true;
        }
    }
}

void BondedUDPReceiverFree(BondedUDPReceiver* videoUDPReceiver) {
    if (videoUDPReceiver != NULL) {
        for (int i = 0; i < videoUDPReceiver->socketCount; i++) {
            if (videoUDPReceiver->fds[i] >= 0) {
                close(videoUDPReceiver->fds[i]);
                videoUDPReceiver->fds[i] = -1;
            }
        }
        free(videoUDPReceiver->fds);
        free(videoUDPReceiver->flags);
        free(videoUDPReceiver->packet);
        free(videoUDPReceiver->payloadBuffer);
        free(videoUDPReceiver);
    }
}