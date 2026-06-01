#ifndef BUFFER_H
#define BUFFER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

#include "safeUtil.h"
#include "cpe464.h"
#include "PDU.h"
#include "globalFlags.h"

// state format with a defaualt added
typedef enum {
    SEQ_BEHIND,
    SEQ_GOOD,
    SEQ_AHEAD,
    SEQ_BAD
} RecvState;

//circular receive buffer
typedef struct {
    uint8_t data[MAX_PDU_SIZE - 7];
    int len;
    int filled;
    uint8_t flag;
    int srejSent;
} BuffSlot;

//local vars packed
typedef struct {
    BuffSlot *buff;
    int buffSize;
    int sock;
    int outFd;
    struct sockaddr_in6 clientAddr;
    uint32_t expSeq;
} RecvBuff;

// Alloc buff and golbal vars
void buffAlloc(int winSize, int sock, int outFd, struct sockaddr_in6 *client);

// process one PDU 
int buffDataCases(uint8_t *pdu, int pduLen);


void buffSendEOFAck();

//helpers and helpful fuincxtions 
RecvState stateDecision(uint32_t seq);
void buffFree();
uint32_t buffGetExpected();

#endif
