#include "window.h"

static SendWindow sendWin;

void winAlloc(int size)
{
    sendWin.winSize = size;
    sendWin.low = 0;
    sendWin.curr = 0;
    sendWin.upp = size;

    sendWin.window = calloc(sendWin.winSize, sizeof(WinThrup));

    if (sendWin.window == NULL) {
        perror("Allocating window buffer failed");
        exit(-1);
    }
}

void winStore(uint32_t seq, uint8_t *pdu, int len)
{
    int idx = seq % sendWin.winSize;

    memcpy(sendWin.window[idx].pdu, pdu, len);
    sendWin.window[idx].len = len;
    sendWin.window[idx].inUse = 1;
}

void winSlide(uint32_t newLow)
{
    while (sendWin.low < newLow) {
        sendWin.window[sendWin.low % sendWin.winSize].inUse = 0;
        sendWin.low++;
    }

    sendWin.upp = sendWin.low + sendWin.winSize;
}

int winGetPDU(uint32_t seq, uint8_t *outWindow)
{
    int idx = seq % sendWin.winSize;

    if (!sendWin.window[idx].inUse) {
        return -1;
    }

    memcpy(outWindow, sendWin.window[idx].pdu, sendWin.window[idx].len);
    return sendWin.window[idx].len;
}

void winFree()
{
    free(sendWin.window);
    sendWin.window = NULL;
}

int winClosed()
{
    return sendWin.curr == sendWin.upp;
}

uint32_t winGetLower()
{
    return sendWin.low;
}

uint32_t winGetCurrent()
{
    return sendWin.curr;
}

uint32_t winGetUpper()
{
    return sendWin.upp;
}

void winAdvance()
{
    sendWin.curr++;
}