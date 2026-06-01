/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "pollLib.h"
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"

#include "PDU.h"
#include "globalFlags.h"
#include "buffer.h"


void handleServer(int parentSock, double errRate);
void handleClient(int parentSock, uint8_t *firstPacket, int packetLen, struct sockaddr_in6 *client, int addrLen, double errRate);
int startFileExchange(uint8_t *firstPacket, int packetLen, char *outFile, int *winSize, int *bufSize);
static void handleFinalAck(int socket);
void recvFile(int sock, struct sockaddr_in6 *client, int outFd, int winSize);
int checkArgs(int argc, char *argv[]);

int main ( int argc, char *argv[])
{ 
	int socketNum = 0;				
	int portNumber = 0;

	portNumber = checkArgs(argc, argv);
	double errRate = atof(argv[1]);
	sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

	socketNum = udpServerSetup(portNumber);

    handleServer(socketNum, errRate);

	close(socketNum);
	
	return 0;
}

void handleServer(int parentSock, double errRate)
{
    uint8_t packetBuf[MAX_PDU_SIZE];
    struct sockaddr_in6 client;
    int addrLen = sizeof(client);
    int packetLen = 0;
    pid_t pid = 0;

    while (1) {
        packetLen = safeRecvfrom(parentSock, packetBuf, MAX_PDU_SIZE, 0, (struct sockaddr *)&client, &addrLen);

        pid = fork();
        if (pid < 0) {
            perror("fork problem");
            exit(-1);
        }
		//indic err init
        if (pid == 0) {
            sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
            handleClient(parentSock, packetBuf, packetLen, &client, addrLen, errRate);
            exit(0);
        }

        while (waitpid(-1, NULL, WNOHANG) > 0);
    }
}

void handleClient(int parentSock, uint8_t *firstPacket, int packetLen, struct sockaddr_in6 *client, int addrLen, double errRate){
	
	char outFile[MAX_FILENAME + 1];
    int childSock = 0, outFd = 0, winSize = 0, bufSize = 0;
    uint8_t pduBuf[MAX_PDU_SIZE];
    uint8_t a = 0;
    int pduLen = 0;

    // rcopy will get this sock from file ok
    childSock = udpServerSetup(0);
    close(parentSock);
	//check if valid
    if (startFileExchange(firstPacket, packetLen, outFile, &winSize, &bufSize) < 0) {
        close(childSock);
        return;
    }
	//check if file can be opened/made
    outFd = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (outFd < 0) {
        pduLen = createPDU(pduBuf, 0, FLAG_FNAME_ERR, &a, 0);
        safeSendto(childSock, pduBuf, pduLen, 0, (struct sockaddr *)client, addrLen);
        close(childSock);
        return;
    }

    pduLen = createPDU(pduBuf, 0, FLAG_FNAME_OK, &a, 0);
    safeSendto(childSock, pduBuf, pduLen, 0, (struct sockaddr *)client, addrLen);

    recvFile(childSock, client, outFd, winSize);

    close(outFd);
    close(childSock);
}

int startFileExchange(uint8_t *firstPacket, int packetLen, char *outFile, int *winSize, int *bufSize){

    uint8_t *payload;
    int payLen, nameLen;
    uint32_t winNet, bufNet;

    if (packetLen < HEADER_SIZE + 9)
        return -1;
    if (in_cksum((unsigned short *)firstPacket, packetLen) != 0)
        return -1;

    payload = firstPacket + HEADER_SIZE;
    payLen = packetLen - HEADER_SIZE;

    // find  \0
    for (nameLen = 0; nameLen < payLen && payload[nameLen] != '\0'; nameLen++);
    if (nameLen >= payLen || nameLen > MAX_FILENAME)
        return -1;

    strcpy(outFile, (char *)payload);

    memcpy(&winNet, payload + nameLen + 1, 4);
    memcpy(&bufNet, payload + nameLen + 1 + 4, 4);
    *winSize = ntohl(winNet);
    *bufSize = ntohl(bufNet);

    return 0;
}

static void handleFinalAck(int socket) {

    uint8_t pduBuf[MAX_PDU_SIZE];
    struct sockaddr_in6 fromAddr;
    int fromLen = sizeof(fromAddr);
    int pduLen = 0, tries = 0;

    while (tries < MAX_TRIES) {
        if (pollCall(TIMEOUT_1S) <= 0) {
            buffSendEOFAck();
            tries++;
            continue;
        }

        pduLen = safeRecvfrom(socket, pduBuf, MAX_PDU_SIZE, 0, (struct sockaddr *)&fromAddr, &fromLen);

        if (pduLen >= HEADER_SIZE && in_cksum((unsigned short *)pduBuf, pduLen) == 0 && pduBuf[6] == FLAG_EOF_ACK) {
            break;
        }

        buffDataCases(pduBuf, pduLen);
    }
}

void recvFile(int socket, struct sockaddr_in6 *client, int outFd, int winSize) {
    uint8_t pduBuf[MAX_PDU_SIZE];
    struct sockaddr_in6 fromAddr;
    int fromLen = sizeof(fromAddr);
    int pduLen = 0, done = 0;

    buffAlloc(winSize, socket, outFd, client);
    setupPollSet();
    addToPollSet(socket);

    while (!done) {
        if (pollCall(TIMEOUT_10S) <= 0) {
            printf("Timed OUt waitiong for Client Data\n");
            break;
        }

        pduLen = safeRecvfrom(socket, pduBuf, MAX_PDU_SIZE, 0, (struct sockaddr *)&fromAddr, &fromLen);
        done = buffDataCases(pduBuf, pduLen);
    }

    if (done) {
        handleFinalAck(socket);
    }
    buffFree();
}


int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;
	
	if (argc < 2 || argc > 3)
	{
		fprintf(stderr, "Usage %s error-rate [optional port number]\n", argv[0]);
		exit(-1);
	}
	double errRate = atof(argv[1]);

    if (errRate < 0 || errRate >= 1)
    {
        fprintf(stderr, "Error rate must be >= 0 and < 1\n");
        exit(-1);
    }
	
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}

	
	return portNumber;
}


