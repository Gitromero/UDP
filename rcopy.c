// Client side - UDP Code				    
// By Hugh Smith	4/1/2017		

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "pollLib.h"
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"

#include "PDU.h"
#include "globalFlags.h"
#include "window.h"

int checkArgs(int argc, char * argv[]);
int startFileExchange(int socketNum, struct sockaddr_in6 *server, char *toFile, int winSize, int bufSize);
static int buildFilenamePayload(uint8_t *payload, char *toFile, int winSize, int bufSize);
void sendFile(int socketNum, struct sockaddr_in6 *server, int fromFd, int winSize, int bufSize);
static void sendAndStorePDU(int socketNum, struct sockaddr_in6 *server, uint32_t seqNum, uint8_t flag, uint8_t *payload, int payloadLen, uint8_t *pduBuf);
static int fillSendWindow(int socketNum, struct sockaddr_in6 *server, int fromFd, int bufSize, uint32_t *seqNum, int *eof, int *tries, uint8_t *fileBuf, int *bytesRead);
static int waitForServer(int socketNum, struct sockaddr_in6 *server, uint32_t seqNum, int eof, int *tries);
int processRecvPackets(int socketNum, struct sockaddr_in6 *server);

int main (int argc, char *argv[])
 {
	struct sockaddr_in6 server;	
	int socketNum = 0, portNumber = 0, fromFd = 0, winSize = 0, bufSize = 0;
    double errRate = 0.0;
	
	portNumber = checkArgs(argc, argv);
    winSize = atoi(argv[3]);
    bufSize = atoi(argv[4]);
    errRate = atof(argv[5]);
	fromFd = open(argv[1], O_RDONLY);

    if (fromFd < 0) {
        printf("Error: file %s not found.\n", argv[1]);
        exit(-1);
    }
	sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

	socketNum = setupUdpClientToServer(&server, argv[6], portNumber);
	
	if (setupFileExchange(socketNum, &server, argv[2], winSize, bufSize) < 0) {
        close(fromFd);
        close(socketNum);
        exit(-1);
    }

    sendFile(socketNum, &server, fromFd, winSize, bufSize);

    close(fromFd);
	close(socketNum);

	return 0;
}

static int buildFilenamePayload(uint8_t *payload, char *toFile, int winSize, int bufSize) {
    int payLen = 0;
    uint32_t winNet = 0, bufNet = 0;

    strcpy((char *)payload, toFile);
    payLen = strlen(toFile) + 1;

    winNet = htonl(winSize);
    memcpy(payload + payLen, &winNet, 4);
    payLen += 4;

    bufNet = htonl(bufSize);
    memcpy(payload + payLen, &bufNet, 4);
    payLen += 4;

    return payLen;
}

// send filename/winsize/bufsize to server and wait for ok
int setupFileExchange(int socketNum, struct sockaddr_in6 *server, char *toFile, int winSize, int bufSize) {
    uint8_t pduBuf[MAX_PDU_SIZE];
    uint8_t payload[MAX_FILENAME + 9];
    uint8_t respBuf[MAX_PDU_SIZE];

    struct sockaddr_in6 fromAddr;

    int addrLen = sizeof(fromAddr);
    int payLen = 0, pduLen = 0, respLen = 0, tries = 0;

    uint8_t flag = 0;

    setupPollSet();
    addToPollSet(socketNum);

    payLen = buildFilenamePayload(payload, toFile, winSize, bufSize);
    pduLen = createPDU(pduBuf, 0, FLAG_FNAME, payload, payLen);

    while (tries < MAX_TRIES) {
        safeSendto(socketNum, pduBuf, pduLen, 0, (struct sockaddr *)server, sizeof(struct sockaddr_in6));

        tries++;

        if (pollCall(TIMEOUT_FNAME) <= 0) {
            continue;
        }

        respLen = safeRecvfrom(socketNum, respBuf, MAX_PDU_SIZE, 0, (struct sockaddr *)&fromAddr, &addrLen);

        if (respLen < HEADER_SIZE) {
            continue;
        }

        if (in_cksum((unsigned short *)respBuf, respLen) != 0) {
            continue;
        }

        flag = respBuf[6];

        if (flag == FLAG_FNAME_ERR) {
            printf("Error: Failed to open output file: %s\n", toFile);
            return -1;
        }

        if (flag == FLAG_FNAME_OK) {
            memcpy(server, &fromAddr, sizeof(fromAddr));
            return 0;
        }
    }

    printf("Error: Server not responding\n");
    return -1;
}

static void sendAndStorePDU(int socketNum, struct sockaddr_in6 *server, uint32_t seqNum, uint8_t flag, uint8_t *payload, int payloadLen, uint8_t *pduBuf) {
    int pduLen = createPDU(pduBuf, seqNum, flag, payload, payloadLen);

    safeSendto(socketNum, pduBuf, pduLen, 0, (struct sockaddr *)server, sizeof(struct sockaddr_in6));

    winStore(seqNum, pduBuf, pduLen);
    winAdvance();
}

static int fillSendWindow(int socketNum, struct sockaddr_in6 *server, int fromFd, int bufSize, uint32_t *seqNum, int *eof, int *tries, uint8_t *fileBuf, int *bytesRead) {
    uint8_t pduBuf[MAX_PDU_SIZE];
    uint8_t nextBuf[MAX_PDU_SIZE - 7];

    int nextLen = 0, flag = 0, ret = 0;

    while (!winClosed() && !(*eof)) {
        nextLen = read(fromFd, nextBuf, bufSize);
        flag = (nextLen <= 0) ? FLAG_EOF : FLAG_DATA;

        sendAndStorePDU(socketNum, server, *seqNum, flag, fileBuf, *bytesRead, pduBuf);
        (*seqNum)++;

        if (flag == FLAG_EOF) {
            *eof = 1;
            break;
        }

        memcpy(fileBuf, nextBuf, nextLen);
        *bytesRead = nextLen;

        while (pollCall(0) > 0) {
            ret = processRecvPackets(socketNum, server);

            if (ret == FLAG_EOF_ACK) {
                return 1;
            }

            if (ret != -1) {
                *tries = 0;
            }
        }
    }

    return 0;
}

static int waitForServer(int socketNum, struct sockaddr_in6 *server, uint32_t seqNum, int eof, int *tries) {
    uint8_t pduBuf[MAX_PDU_SIZE];
    int ret = 0, pduLen = 0;

    if (pollCall(TIMEOUT_1S) > 0) {
        ret = processRecvPackets(socketNum, server);

        if (ret == FLAG_EOF_ACK) {
            return 1;
        }

        if (eof && winGetLower() == seqNum) {
            return 1;
        }

        if (ret != -1) {
            *tries = 0;
        }

        return 0;
    }

    (*tries)++;

    if (*tries >= MAX_TRIES) {
        printf("Gave Up: Server not responding\n");
        return -1;
    }

    pduLen = winGetPDU(winGetLower(), pduBuf);

    if (pduLen > 0) {
        safeSendto(socketNum, pduBuf, pduLen, 0, (struct sockaddr *)server, sizeof(struct sockaddr_in6));
    }

    return 0;
}

void sendFile(int socketNum, struct sockaddr_in6 *server, int fromFd, int winSize, int bufSize) {
    uint8_t pduBuf[MAX_PDU_SIZE];
    uint8_t fileBuf[MAX_PDU_SIZE - 7];

    uint32_t seqNum = 0;
    int bytesRead = 0, eof = 0, done = 0, tries = 0, ret = 0;

    winAlloc(winSize);
	//kisckstart the window by sending the first packet
    bytesRead = read(fromFd, fileBuf, bufSize);

    if (bytesRead <= 0) {
        sendAndStorePDU(socketNum, server, seqNum, FLAG_EOF, fileBuf, 0, pduBuf);
        seqNum++;
        eof = 1;
    }
	//implemenation
    while (!done) {
        done = fillSendWindow(socketNum, server, fromFd, bufSize, &seqNum, &eof, &tries, fileBuf, &bytesRead);

        if (done) {
            break;
        }

        ret = waitForServer(socketNum, server, seqNum, eof, &tries);

        if (ret == 1) {
            done = 1;
        }
        else if (ret == -1) {
            break;
        }
    }
	//teardown
    if (done) {
        uint8_t ackBuf[MAX_PDU_SIZE];
        uint8_t a = 0;

        int ackLen = createPDU(ackBuf, seqNum, FLAG_EOF_ACK, &a, 0);

        safeSendto(socketNum, ackBuf, ackLen, 0, (struct sockaddr *)server, sizeof(struct sockaddr_in6));
    }

    winFree();
}

int processRecvPackets(int socketNum, struct sockaddr_in6 *server) {
    uint8_t pduBuf[MAX_PDU_SIZE];
    uint8_t retryBuf[MAX_PDU_SIZE];

    struct sockaddr_in6 fromAddr;

    int addrLen = sizeof(fromAddr);
    int pduLen = 0, retryLen = 0;

    uint32_t seqNum = 0;
    uint8_t flag = 0;

    pduLen = safeRecvfrom(socketNum, pduBuf, MAX_PDU_SIZE, 0, (struct sockaddr *)&fromAddr, &addrLen);
	//error chjecks
    if (pduLen < HEADER_SIZE) {
        return -1;
    }

    if (in_cksum((unsigned short *)pduBuf, pduLen) != 0) {
        return -1;
    }

    flag = pduBuf[6];
	//RR case
    if (flag == FLAG_RR) {
        memcpy(&seqNum, pduBuf + HEADER_SIZE, 4);
        seqNum = ntohl(seqNum);

        winSlide(seqNum);
    }
	//SREJ case
    else if (flag == FLAG_SREJ) {
        memcpy(&seqNum, pduBuf + HEADER_SIZE, 4);
        seqNum = ntohl(seqNum);

        retryLen = winGetPDU(seqNum, retryBuf);

        if (retryLen > 0) {
            safeSendto(socketNum, retryBuf, retryLen, 0, (struct sockaddr *)server, sizeof(struct sockaddr_in6));
        }
    }

    return flag;
}

int checkArgs(int argc, char * argv[])
{
    int portNumber = 0;
		
	if (argc != 8)
	{
		printf("usage: %s from-file to-file window-size buffer-size error-rate remote-machine remote-port \n", argv[0]);
		printf("Please include Error Rate 0.0-1.0");
		exit(-1);
	}
    	if (strlen(argv[1]) > MAX_FILENAME || strlen(argv[2]) > MAX_FILENAME) {
        	printf("Error: filename too long (max %d chars)\n", MAX_FILENAME);
        	exit(-1);
    	}

    	double rate = atof(argv[5]);
    	if (rate < 0.0 || rate >= 1.0) {
        	printf("Error: Error Rate must be between 0.0-1.0\n");
        	exit(-1);
    	}	

	portNumber = atoi(argv[7]);
		
	return portNumber;
}