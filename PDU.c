#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "PDU.h"
#include "cpe464.h"

int createPDU(uint8_t *pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t *payload, int payloadLen){
    uint32_t seq = htonl(sequenceNumber);
    uint16_t cksum = 0;

    int pduLength = HEADER_SIZE + payloadLen;  //should probaly check if less than MAX

    memcpy(pduBuffer, &seq, 4);

    memcpy(pduBuffer + 4, &cksum, 2);

    pduBuffer[6] = flag;

    memcpy(pduBuffer + HEADER_SIZE, payload, payloadLen);

    cksum = in_cksum((unsigned short *)pduBuffer, pduLength);

    memcpy(pduBuffer + 4, &cksum, 2);

    return pduLength;
}

void printPDU(uint8_t *aPDU, int pduLength){
    uint32_t seqNum, Seq;

    uint16_t recvCksum, ourCksum;

    uint8_t flag = aPDU[6];

    int payloadLen = pduLength - HEADER_SIZE;

    memcpy(&seqNum, aPDU, 4);
    Seq = ntohl(seqNum);

    memcpy(&recvCksum, aPDU + 4, 2);

    ourCksum = in_cksum((unsigned short *)aPDU, pduLength);

    if (ourCksum != 0){
        printf("CHECKSUM ERROR: PDU corrupted\n");
    }
    else{
        printf("CHECKSUM OK: PDU uncorrupted\n");
    }

    printf("Sequence Number: %u\n", Seq);
    printf("Flag: %u\n", flag);
    printf("Payload Length: %d\n", payloadLen);
    printf("Payload: ");

    for (int i = 0; i < payloadLen; i++){
        printf("%c", aPDU[HEADER_SIZE + i]);
    }
    printf("\n");
}
