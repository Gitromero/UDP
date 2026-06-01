#include "buffer.h"

static BuffSlot *buff = NULL;
static int buffSize;
static int fds[2];// fds[0] = socket, fds[1] = output file fd
static struct sockaddr_in6 clientAddr;
static uint32_t expSeq;

//handler helpers
static int handleGood(uint8_t *pdu, int payLen, uint8_t flag);
static void handleAhead(uint32_t seq, uint8_t *payload, int payLen, uint8_t flag);

//packet senders
static void sendRevPack(uint32_t seq, uint8_t flag);

void buffAlloc(int winSize, int sock, int outFd, struct sockaddr_in6 *client)
{
    buffSize = winSize;
    fds[0] = sock;
    fds[1] = outFd;
    memcpy(&clientAddr, client, sizeof(struct sockaddr_in6));
    expSeq = 0;

    buff = calloc(buffSize, sizeof(BuffSlot));
    if (buff == NULL) {
        perror("Allocating Recv buffer failed");
        exit(-1);
    }
}
//state filtering
int buffDataCases(uint8_t *pdu, int pduLen)
{
    uint32_t seq = 0;
    uint8_t flag = 0;

    //time out if bad poacket
    if (in_cksum((unsigned short *)pdu, pduLen) != 0)
        return 0;

    memcpy(&seq, pdu, 4);
    seq = ntohl(seq);
    flag = pdu[6];
    RecvState state = stateDecision(seq);

    switch (state) {
        case SEQ_BEHIND:
            sendRevPack(expSeq, FLAG_RR);
            break;
        case SEQ_GOOD:
            return handleGood(pdu, pduLen - HEADER_SIZE, flag);
        case SEQ_AHEAD:
            handleAhead(seq, pdu + HEADER_SIZE, pduLen - HEADER_SIZE, flag);
            break;
        case SEQ_BAD:
            break;
    }
    return 0;
}

//nothing bad happened no lost packets
static int handleGood(uint8_t *pdu, int len, uint8_t flag)
{
    int done = 0;
    int idx = expSeq % buffSize;

    //gap is now filled
    buff[idx].srejSent = 0;

    write(fds[1], pdu + HEADER_SIZE, len);

    if (flag == FLAG_EOF){
        done = 1;
    }

    expSeq++;

    while (!done && buff[expSeq % buffSize].filled) {
        idx = expSeq % buffSize;
        write(fds[1], buff[idx].data, buff[idx].len);
        if (buff[idx].flag == FLAG_EOF)
            done = 1;
        buff[idx].filled = 0;
        buff[idx].srejSent = 0;
        expSeq++;
    }

    if (done){
        buffSendEOFAck();
    }
    else{
        sendRevPack(expSeq, FLAG_RR);
    }

    return done;
}

// good packet bad order - SREJ and fix the order
static void handleAhead(uint32_t seq, uint8_t *payload, int len, uint8_t flag){

    int idx = seq % buffSize;

    //check filled, if filled leave it be
    if (!buff[idx].filled) {
        memcpy(buff[idx].data, payload, len);
        buff[idx].len = len;
        buff[idx].flag = flag;
        buff[idx].filled = 1;
    }

    //timeout will cover srej
    if (!buff[expSeq % buffSize].srejSent) {

        sendRevPack(expSeq, FLAG_SREJ);
        buff[expSeq % buffSize].srejSent = 1;
    }
}
//packet sendingf function for RR and SREJ
static void sendRevPack(uint32_t seq, uint8_t flag){

    uint8_t pduBuf[HEADER_SIZE + 4];
    uint32_t seqSend = htonl(seq);
    int len = createPDU(pduBuf, 0, flag, (uint8_t *)&seqSend, 4);
    safeSendto(fds[0], pduBuf, len, 0, (struct sockaddr *)&clientAddr, sizeof(struct sockaddr_in6));
}

//would look weird if also part of sendRevPack
void buffSendEOFAck(){

    uint8_t pduBuf[HEADER_SIZE + 4];
    uint8_t a = 0;
    int len = createPDU(pduBuf, 0, FLAG_EOF_ACK, &a, 0);
    safeSendto(fds[0], pduBuf, len, 0, (struct sockaddr *)&clientAddr, sizeof(struct sockaddr_in6));
}

RecvState stateDecision(uint32_t seq){
    if (seq < expSeq)
        return SEQ_BEHIND;
    else if (seq == expSeq)
        return SEQ_GOOD;
    else if (seq < expSeq + (uint32_t)buffSize)
        return SEQ_AHEAD;
    else
        return SEQ_BAD;
}

void buffFree(){
    free(buff);
    buff = NULL;
}

uint32_t buffGetExpected(){
    return expSeq;
}