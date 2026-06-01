#include "window.h"

static WinThrup *window = NULL;
static int winSize = 0;
static uint32_t low = 0;
static uint32_t curr = 0;
static uint32_t upp = 0;

void winAlloc(int size)
{
    winSize = size;
    low = 0;
    curr = 0;
    upp = size;  

    window = calloc(winSize, sizeof(WinThrup));
    if (window == NULL) {
        perror("Allocating window buffer failed");
        exit(-1);
    }
}
void winStore(uint32_t seq, uint8_t *pdu, int len)
{
    int idx = seq % winSize;
    memcpy(window[idx].pdu, pdu, len);

    window[idx].len = len;
    window[idx].inUse = 1;
}

void winSlide(uint32_t newlow)
{
    while (low < newlow) {
        window[low % winSize].inUse = 0;
        low++;
    }
    upp = low + winSize;
}

int winGetPDU(uint32_t seq, uint8_t *outwindow)
{
    int idx = seq % winSize;
    if (!window[idx].inUse){
        return -1;
    }

    memcpy(outwindow, window[idx].pdu, window[idx].len);
    return window[idx].len;
}

void winFree()
{
    free(window);
    window = NULL;
}

int winClosed()
{
    return curr == upp;
;
}

uint32_t winGetLower() { 
    return low; 
}
uint32_t winGetCurrent() { 
    return curr; 
}
uint32_t winGetUpper() { 
    return upp; 
}

void winAdvance(){
    curr++;
}


