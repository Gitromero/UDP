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

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"
#include "PDU.h"

#define MAXBUF 80


void processClient(int socketNum);
int checkArgs(int argc, char *argv[]);

int main ( int argc, char *argv[]  )
{ 
	int socketNum = 0;				
	int portNumber = 0;

	portNumber = checkArgs(argc, argv);
	double errRate = atof(argv[1]);
	sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

	socketNum = udpServerSetup(portNumber);

	processClient(socketNum);

	close(socketNum);
	
	return 0;
}

void processClient(int socketNum)
{
	int dataLen = 0; 
	uint8_t buffer[MAX_PDU_SIZE];	  
	struct sockaddr_in6 client;		

	int clientAddrLen = sizeof(client);	
	
	while (1)
	{
		dataLen = safeRecvfrom(socketNum, buffer, MAX_PDU_SIZE, 0, (struct sockaddr *) &client, &clientAddrLen);
	
		printf("Received message via PDU from client with ");
		printIPInfo(&client);
		printPDU(buffer, dataLen);

		// just for fun send back to client number of bytes received
		safeSendto(socketNum, buffer, dataLen, 0, (struct sockaddr *) & client, clientAddrLen);

	}
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


