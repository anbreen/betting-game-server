#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>

#define PORT "2222"        // the port users will be connecting to ./client 127.0.0.1#define BACKLOG 64500      // how many pending connections queue will hold///////Message Types////////
#define BEGASEP_OPEN    0x1 
#define BEGASEP_ACCEPT  0x2  
#define BEGASEP_BET     0x3
#define BEGASEP_RESULT  0x4

#define BEGASEP_NUM_CLIENTS  100 //it can be 64500 #define BEGASEP_NUM_MIN      100
#define BEGASEP_NUM_MAX      200

#define PROTOCOL_VERSION 0x1

///////Message Headers////////
typedef struct BEGASEP_COMMONHEADER {
	unsigned ProtocolVersion :4;  //4bits
	unsigned PacketType :4;       //4 bits
	uint8_t PacketLength;        //1 byte
	uint16_t ClientId;           //2byte
} Begasep_CommonHeader;

typedef struct BEGASEP_ACCEPTMSG {
	int LowerEndofNumber;
	int UpperEndofNumber;
} Begasep_AcceptMsg;

typedef struct BEGASEP_BETMSG {
	int ClientBet;
} Begasep_BetMsg;

typedef struct BEGASEP_RESULTMSG {
	uint8_t ResultStatus;  //1byte
	int WinningNumber;
} Begasep_ResultMsg;

//this function is used to make up the header of the 4 messages specified, as all messages share a common header
void makeHeader(unsigned ProtocolVersion, unsigned PacketType,
		uint8_t PacketSIze, uint16_t ClientID, Begasep_CommonHeader *SendHeader) {
	SendHeader->ProtocolVersion = ProtocolVersion;
	SendHeader->PacketType = PacketType;
	SendHeader->PacketLength = PacketSIze;
	SendHeader->ClientId = ClientID;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
	int sockfd, numbytes;
	char buf[1024];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2) {
		fprintf(stderr, "usage: client hostname\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can./client 127.0.0.1
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *) p->ai_addr), s,
			sizeof s);
	printf("client: connecting to %s\n", s);
	freeaddrinfo(servinfo); // all done with this structure

	Begasep_CommonHeader SendHeader;
	Begasep_CommonHeader RecvHeader;
	makeHeader(PROTOCOL_VERSION, BEGASEP_OPEN, sizeof(SendHeader), 0,
			&SendHeader);
	if (send(sockfd, (Begasep_CommonHeader*) &SendHeader, sizeof(SendHeader), 0)
			== -1)
		perror("send");
	printf(
			"Client Sends    **** |Version = %2u | Packet Type = %2u | Packet Length = %d | ClientID = %d | **** \n",
			SendHeader.ProtocolVersion, SendHeader.PacketType,
			SendHeader.PacketLength, SendHeader.ClientId);
	int ResultReceived = 0;
	while (1) {
		if ((recv(sockfd, &RecvHeader, sizeof(RecvHeader), 0)) <= 0) {
			perror("recv invalid Bet");
			close(sockfd);
			exit(1);
		}
		printf(
				"\n\nClient Receives **** |Version = %2u | Packet Type = %2u | Packet Length = %d | ClientID = %d | **** \n",
				RecvHeader.ProtocolVersion, RecvHeader.PacketType,
				RecvHeader.PacketLength, RecvHeader.ClientId);

		switch (RecvHeader.PacketType) {
		case BEGASEP_ACCEPT:
			printf("");
			Begasep_AcceptMsg AcceptMessage;
			if ((recv(sockfd, &AcceptMessage, sizeof(AcceptMessage), 0)) <= 0) {
				perror("recv");
				exit(1);
			}
			printf(
					"                **** |Minimum Limit = %d | Maximum Limit  %d | \n",
					AcceptMessage.LowerEndofNumber,
					AcceptMessage.UpperEndofNumber);
			Begasep_BetMsg BetMessage;
			printf("\n\nEnter the bet you want to make beteen %d and %d = ",
					BEGASEP_NUM_MIN, BEGASEP_NUM_MAX);
			scanf("%d", &(BetMessage.ClientBet));
			makeHeader(PROTOCOL_VERSION, BEGASEP_BET,
					sizeof(SendHeader) + sizeof(BetMessage),
					RecvHeader.ClientId, &SendHeader);
			if (send(sockfd, (Begasep_CommonHeader*) &SendHeader,
					sizeof(SendHeader), 0) == -1)
				perror("send");
			if (send(sockfd, (Begasep_BetMsg*) &BetMessage, sizeof(BetMessage),
					0) == -1)
				perror("send");
			printf(
					"\n\nClient Sends    **** |Version = %2u | Packet Type = %2u | Packet Length = %d | ClientID = %d | **** \n",
					SendHeader.ProtocolVersion, SendHeader.PacketType,
					SendHeader.PacketLength, SendHeader.ClientId);
			printf("                **** |Bet made = %d |\n",
					BetMessage.ClientBet);
			break;

		case BEGASEP_RESULT:
			printf("");
			Begasep_ResultMsg ResultMessage;
			if ((recv(sockfd, &ResultMessage, sizeof(ResultMessage), 0)) <= 0) {
				perror("recv");
				exit(1);
			}
			printf(
					"                **** |Bet Status %d | Winning number %d | \n",
					ResultMessage.ResultStatus, ResultMessage.WinningNumber);
			if (ResultMessage.ResultStatus == 1)
				printf("***wow you won the bet***\n");
			else
				printf("***:( you lost the bet***\n");

			ResultReceived = 1;
			break;
		}
		if (ResultReceived == 1)
			break;
	}

	close(sockfd);
	return 0;
}

