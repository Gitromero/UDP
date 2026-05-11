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

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"
#include "PDU.h"

#define MAXBUF 80

void talkToServer(int socketNum, struct sockaddr_in6 * server);
int readFromStdin(char * buffer);
int checkArgs(int argc, char * argv[]);

int main (int argc, char *argv[])
 {
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	double errRate = atof(argv[1]);
	sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

	socketNum = setupUdpClientToServer(&server, argv[2], portNumber);
	
	talkToServer(socketNum, &server);
	
	close(socketNum);

	return 0;
}

void talkToServer(int socketNum, struct sockaddr_in6 * server)
{
	int serverAddrLen = sizeof(struct sockaddr_in6);
	char * ipString = NULL;
	int dataLen = 0; 
	char buffer[MAXBUF+1];
	
	uint8_t pduBuffer[MAX_PDU_SIZE];
    uint8_t recvPDU[MAX_PDU_SIZE];

    int pduLen = 0;
    int recvLen = 0;

    uint32_t sequenceNumber = 0;
    uint8_t flag = 222;

	buffer[0] = '\0';
	while (buffer[0] != '.')
	{
		dataLen = readFromStdin(buffer);

		pduLen = createPDU(pduBuffer, sequenceNumber, flag, (uint8_t *)buffer, dataLen);

		printf("Sending: %s with len: %d as a PDU\n", buffer, dataLen);
		printPDU(pduBuffer, pduLen);
	
		safeSendto(socketNum, pduBuffer, pduLen, 0, (struct sockaddr *) server, serverAddrLen);
		
		safeRecvfrom(socketNum, recvPDU, MAX_PDU_SIZE, 0, (struct sockaddr *) server, &serverAddrLen);
		
		// print out bytes received
		ipString = ipAddressToString(server);
		printf("Server with ip: %s and port %d said it received %s\n", ipString, ntohs(server->sin6_port), buffer);
	    
		printPDU(recvPDU, recvLen);
		sequenceNumber++;
	}
}

int readFromStdin(char * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

int checkArgs(int argc, char * argv[])
{

    int portNumber = 0;
	
    /* check command line arguments  */
	
	if (argc != 4)
	{
		printf("usage: %s error-rate host-name port-number \n", argv[0]);
		printf("Please include Error Rate 0-1");
		exit(1);
	}
	
	portNumber = atoi(argv[3]);
		
	return portNumber;
}





