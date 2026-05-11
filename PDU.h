#ifndef PDU_H
#define PDU_H

#include <stdint.h>

#define HEADER_SIZE 7
#define MAX_PDU_SIZE 1407

int createPDU(uint8_t *pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t *payload, int payloadLen);

void printPDU(uint8_t *aPDU, int pduLength);

#endif