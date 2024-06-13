#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DEFAULT_PORT_NUMBER     3000
#define DEFAULT_HOST_NAME       "localhost"

typedef struct pdu {
    char type;
    char data[100];
} pdu;

void registerContent();
void deregisterContent();
void listOnLineRegisteredContent();
void downloadContent();
void readAcknowledgement(int socketDescriptor);

int main(int argc, char** argv){
    /* get&set server port # */
    char* indexServer;
    int indexPort;
    switch(argc){
        case 1:
            indexServer = DEFAULT_HOST_NAME;
            indexPort = DEFAULT_PORT_NUMBER;
            break;
        case 2:
            indexServer = DEFAULT_HOST_NAME;
            indexPort = atoi(argv[2]);
            break;
        case 3:
            indexServer = argv[1];
            indexPort = atoi(argv[2]);
            break;
        default:
            perror("Bad # of args\n");
            exit(EXIT_FAILURE);
    }

    /** creating a struct to store socket address of host */    
    struct sockaddr_in indexServerSocketAddr;
    bzero((char*)&indexServerSocketAddr, sizeof(struct sockaddr_in));
    indexServerSocketAddr.sin_family = AF_INET;    
    indexServerSocketAddr.sin_port = htons((u_short)indexPort);

    struct hostent *indexServerName;
    if ( indexServerName = gethostbyname(indexServer) ){
        memcpy(&indexServerSocketAddr.sin_addr, indexServerName-> h_addr, indexServerName->h_length);
    }
    else if ( (indexServerSocketAddr.sin_addr.s_addr = inet_addr(indexServer)) == INADDR_NONE ){
		perror("Couldn't get host entry\n");
		exit(EXIT_FAILURE);
    }

     /* create UDP client socket */
    int clientSocketDescriptorUDP;
    if ( (clientSocketDescriptorUDP=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        perror("Couldn't create socket\n");
        exit(EXIT_FAILURE);
    }

    /* connect to host */
    if( (connect(clientSocketDescriptorUDP, (struct sockaddr*)&indexServerSocketAddr, sizeof(indexServerSocketAddr))) == -1){
        perror("Couldn't connect to host\n");
        exit(EXIT_FAILURE);
    }

    char peerName[50];
    printf("Choose a username -> ");
    scanf("%50s", peerName);

    printf("Hi %s.\n", peerName);
    
    char commandType;
    printf("Command (type '?' for list of commands) -> ");
    scanf(" %c", &commandType);
    while(commandType != 'Q'){
        pdu commandPDU;
        commandPDU.type = commandType;
        switch(commandType){
            case 'R':
                registerContent();
                strcpy(commandPDU.data, "REGISTER CONTENT.");
                break;
            case 'T':
                deregisterContent();
                strcpy(commandPDU.data, "DE-REGISTER CONTENT.");
                break;
            case 'E':
                strcpy(commandPDU.data, "SPECIFIED ERROR.");
                break;
            default:
                strcpy(commandPDU.data, "ACTUAL ERROR.");
                break;
        }
        /*commandPDU.data[sizeof(commandPDU.data) -1] = '\0';*/ //actually not necessary as `strcpy` already handles null-termination
        write(clientSocketDescriptorUDP, &commandPDU, sizeof(commandPDU));
        
        readAcknowledgement(clientSocketDescriptorUDP);
        
        printf("Command (type '?' for list of commands) -> ");
        scanf(" %c", &commandType);
    }
    

    close(clientSocketDescriptorUDP);
    exit(0);
}

void registerContent(){
    printf("Content Registration\n");
}

void deregisterContent(){
    printf("Search Registration\n");
}

void readAcknowledgement(int socketDescriptor) {
    pdu ackPDU;
    int n = read(socketDescriptor, &ackPDU, sizeof(ackPDU));
    if (n < 0) {
        perror("Error reading from socket");
        exit(EXIT_FAILURE);
    }
    printf("Acknowledgement from server: %s\n", ackPDU.data);
}