#ifndef WINDOW_H
#define WINDOW_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "PDU.h"
//local vars packed
typedef struct {
    WinThrup *window;
    int winSize;
    uint32_t low;
    uint32_t curr;
    uint32_t upp;
} SendWindow;

// thrup for cirtcular list
typedef struct {
    uint8_t pdu[MAX_PDU_SIZE];
    int len;
    int inUse;
} WinThrup;

//core
void winAlloc(int size);
void winStore(uint32_t seq, uint8_t *pdu, int len);
void winSlide(uint32_t newLow);


//hewlpful functions
void winFree();
int winClosed();
void winAdvance();

//gets
int winGetPDU(uint32_t seq, uint8_t *outBuff);
uint32_t winGetLower();
uint32_t winGetCurrent();
uint32_t winGetUpper();

#endif