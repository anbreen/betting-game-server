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

#define PORT "2222"        // the port users will be connecting to ./client 127.0.0.1
#define BACKLOG 64500      // how many pending connections queue will hold
///////Message Types////////
#define BEGASEP_OPEN    0x1 
#define BEGASEP_ACCEPT  0x2  
#define BEGASEP_BET     0x3
#define BEGASEP_RESULT  0x4

#define BEGASEP_NUM_CLIENTS  100 //it can be 64500 
#define BEGASEP_NUM_MIN      100
#define BEGASEP_NUM_MAX      200

#define PROTOCOL_VERSION 0x1

///////Message Headers////////
typedef struct BEGASEP_COMMONHEADER {
	unsigned ProtocolVersion :4;  //4bits
	unsigned PacketType :4;       //4 bits
	uint8_t PacketLength;         //1 byte
	uint16_t ClientId;            //2byte
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
void makeHeader(unsigned ProtocolVersion, unsigned PacketType, uint8_t PacketSIze, uint16_t ClientID, Begasep_CommonHeader *SendHeader) {
	SendHeader->ProtocolVersion = ProtocolVersion;
	SendHeader->PacketType = PacketType;
	SendHeader->PacketLength = PacketSIze;
	SendHeader->ClientId = ClientID;
}

//it randomly generates a number between the specified minimum and maximum limit 
int GenerateWinningNumber(int min, int max) {
	static int Init = 0;
	int rc;
	if (Init == 0) {
		srand(time(NULL));
		Init = 1;
	}
	rc = (rand() % (max - min + 1) + min);
	return (rc);
}

int main(int argc, char *argv[]) {
	fd_set master;
	fd_set read_fds;
	struct sockaddr_in serveraddr;
	struct sockaddr_in clientaddr;
	char buf[1024];
	int fdmax, listener, newfd, nbytes, i, j, addrlen, rc;
	int yes = 1;
	int ClientID = 0;
	int win_num[10];
	for (i = 0; i < 100; i++)
		win_num[i] = 0; //this array stores the bet generated by every client, it is initialized to zero
	int WinningNumber = GenerateWinningNumber(BEGASEP_NUM_MIN, BEGASEP_NUM_MAX); //at the start of a program a winning nnumber is generated and after every timeout a new winning number is generated

	struct timeval timeout;
	/* clear the master sets */
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	/* get the listener */
	if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Server-socket() error!");
		exit(1);
	}
	printf("Server-socket() is OK...\n");
	//address already in use error message , although I have set this option but still sometimes the program is giving this error
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("Server-setsockopt() error lol!");
		exit(1);
	}
	printf("Server-setsockopt() is OK...\n");
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_port = htons(2222);
	memset(&(serveraddr.sin_zero), '\0', 8);
	if (bind(listener, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) == -1) {
		perror("Server-bind() Error!");
		exit(1);
	}
	printf("Server-bind() is OK...\n");
	if (listen(listener, 10) == -1) {
		perror("Server-listen() Error!");
		exit(1);
	}
	printf("Server-listen() is OK...\n");
	// add the listener to the master set
	FD_SET(listener, &master);
	// keep track of the biggest file descriptor
	fdmax = listener;
	for (;;) {
		read_fds = master;
		timeout.tv_sec = 15; //set the timeout for the select statement as 15 seconds
		timeout.tv_usec = 0;
		rc = (select(fdmax + 1, &read_fds, NULL, NULL, &timeout));
		if (rc < 0) {
			perror("Server-select() error !"); //if there is some error in the select the program will exit
			exit(1);
		}
		if (rc == 0){ //rc is set to 0 , whenever time out takes place, in our case it is 15 secs
			printf("Time Out\n");
			for (i = 1; i <= fdmax; i++) {
				if (i == listener) {
				} else {
					if (win_num[i] == -1) { //this is a case for the client who did submitted a bet but it was invalid.
						close(i);
						FD_CLR(i, &master);
					} else { //this case is for the client who did submit a bet
						Begasep_ResultMsg ResultMessage;
						if (WinningNumber == win_num[i])
							ResultMessage.ResultStatus = 1; //if the submitted bet is equal to the winning number
						else
							ResultMessage.ResultStatus = 0;
						//the final BEGASEP_RESULT message is sent to all connected client who did submit a reques
						Begasep_CommonHeader SendHeader;
						ResultMessage.WinningNumber = WinningNumber;
						makeHeader(PROTOCOL_VERSION, BEGASEP_RESULT, sizeof(SendHeader) + sizeof(ResultMessage), i, &SendHeader);
						if (send(i, (Begasep_CommonHeader*) &SendHeader, sizeof(SendHeader), 0) == -1) {
						} else {
							if (send(i, (Begasep_ResultMsg*) &ResultMessage, sizeof(ResultMessage), 0) == -1) {
							} else {
								printf("\n\nServer Sends time out msg   **** |Version = %2u | Packet Type = %2u | Packet Length = %d | ClientID = %d | **** \n", SendHeader.ProtocolVersion, SendHeader.PacketType,SendHeader.PacketLength, i);
								printf("**** |Bet Status %d | Winning number %d | \n",ResultMessage.ResultStatus,ResultMessage.WinningNumber);
								close(i);
								FD_CLR(i, &master);
								WinningNumber = GenerateWinningNumber(BEGASEP_NUM_MIN, BEGASEP_NUM_MAX); //at the end of every time out a new winning number is generated for the next session
							}
						}
					}
				}
			}
		}

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)){ //now one by one we check all the descriptors as if they are set or not.
				//printf("socket %d %d is set \n",i,fdmax);
				if (i == listener) {
					/* handle new connections */
					addrlen = sizeof(clientaddr);
					if ((newfd = accept(listener,(struct sockaddr *) &clientaddr, &addrlen)) == -1)
						perror("Server-accept() !");
					else {
						printf("Server-accept() is OK...\n");
						FD_SET(newfd, &master); /* add to master set */
						if (newfd > fdmax) {
							fdmax = newfd;
						}
						printf("%s: New connection from %s on socket %d\n",argv[0], inet_ntoa(clientaddr.sin_addr), newfd);
					}
				} else {
					Begasep_CommonHeader RecvHeader;
					Begasep_CommonHeader SendHeader;
					if ((recv(i, &RecvHeader, sizeof(RecvHeader), 0)) <= 0) {
						//this call receives a header and based on the type of packet value present in the header, choose a case to be executed
						perror("recv");
						close(i);
						FD_CLR(i, &master);
					} else {
						printf("\n\nServer Receives **** |Version = %2u | Packet Type = %2u | Packet Length = %d | ClientID = %d | **** \n",RecvHeader.ProtocolVersion,RecvHeader.PacketType, RecvHeader.PacketLength,RecvHeader.ClientId);
						switch ((unsigned) (RecvHeader.PacketType)) {
						case BEGASEP_OPEN:{ //if the packet type field in the header is  BEGASEP_OPEN then this case is selected
							ClientID = i; //the client ID is set as the unique socket descriptor of that connection, that way the uniquness of the ID is automatically handled
							Begasep_AcceptMsg AcceptMessage;
							AcceptMessage.LowerEndofNumber = BEGASEP_NUM_MIN;
							AcceptMessage.UpperEndofNumber = BEGASEP_NUM_MAX;
							//header for BEGASEP_ACCEPT is created
							makeHeader(PROTOCOL_VERSION, BEGASEP_ACCEPT,sizeof(SendHeader) + sizeof(AcceptMessage),ClientID, &SendHeader);
							//header for the BEGASEP_ACCEPT is sent to the client
							if (send(i, (Begasep_CommonHeader*) &SendHeader,sizeof(SendHeader), 0) == -1)
								perror("send");
							//in response BEGASEP_ACCEPT is sent to the client
							if (send(i, (Begasep_AcceptMsg*) &AcceptMessage,sizeof(AcceptMessage), 0) == -1)
								perror("send");
							printf("\n\nServer Sends    **** |Version = %2u | Packet Type = %2u | Packet Length = %d | ClientID = %d | **** \n",SendHeader.ProtocolVersion,SendHeader.PacketType,SendHeader.PacketLength,SendHeader.ClientId);
							printf("**** | Minimum Limit = %d | Maximum Limit  %d | \n",AcceptMessage.LowerEndofNumber,AcceptMessage.UpperEndofNumber);
							break;
						}

						case BEGASEP_BET:{ //he packet type field in the header is BEGASEP_BET then this case is selected
							printf("");
							Begasep_BetMsg BetMessage;
							if ((recv(i, &BetMessage, sizeof(BetMessage), 0))<= 0) {
								perror("recv");
								exit(1);
							}
							printf("**** |Bet made = %d |\n",BetMessage.ClientBet);
							//this is a case if the bet made by the client is not within the specified limit so the socket si simply closed
							if (BetMessage.ClientBet <= BEGASEP_NUM_MIN || BetMessage.ClientBet >= BEGASEP_NUM_MAX) {
								printf("Bet made was not within the limit\n");
								win_num[i] = -1;
								close(i);
								FD_CLR(i, &master);
							} else {
								printf("Client made a valid Bet\n");
								Begasep_ResultMsg ResultMessage;
								win_num[i] = BetMessage.ClientBet;
							}
							break;
						}
					}
				}
			}
		}
	}
	printf("I am going to close too \n");
	return 0;
}

