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

void registerContent(int, pdu, char*, size_t);
void deregisterContent(int, pdu);
void errorMessage(int, pdu);
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

    /* get name for peer (no more than 10 characters) */
    char peerName[11];
    char tempNameBuffer[100]; //temporary buffer for peer-name. needed to handle bad input
    printf("Choose a username -> ");    
    fgets(tempNameBuffer, sizeof(tempNameBuffer), stdin);
    sscanf(tempNameBuffer, "%10s", peerName);
    peerName[10] = '\0'; // Ensure null termination
    /*if (strchr(tempNameBuffer, '\n') == NULL) { //overkill with the buffer clearing; might not need
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { }
    }*/
    printf("Hi %s.\n", peerName);
    
    /* prompt user for a command */
    char commandType;
    char tempCommandBuffer[100]; //temporary buffer for handling bad command inputs
    printf("Command (type '?' for list of commands) -> ");
    fgets(tempCommandBuffer, sizeof(tempCommandBuffer), stdin);
    sscanf(tempCommandBuffer, " %c", &commandType);    

    while(commandType != 'Q'){
        if (tempCommandBuffer[0] == '\n') {// in case user presses the Enter key
            printf("Command (type '?' for list of commands) -> ");
            fgets(tempCommandBuffer, sizeof(tempCommandBuffer), stdin);
            sscanf(tempCommandBuffer, " %c", &commandType);
            continue;
        }
        
        pdu commandPDU;
        commandPDU.type = commandType;
        switch(commandType){
            case '?':
                printf("\n[R]:\tContent Registration\n");
                printf("[T]:\tContent De-egistration\n");
                printf("[L]:\tList LOCAL Content\n");
                printf("[D]:\tDownload Content\n");
                printf("[O]:\tList ONLINE Content\n");
                printf("[Q]:\tQuit\n\n");
                break;
            case 'R':
                registerContent(clientSocketDescriptorUDP, commandPDU, peerName, sizeof(peerName));
                readAcknowledgement(clientSocketDescriptorUDP);
                break;
            case 'T':
                deregisterContent(clientSocketDescriptorUDP, commandPDU);
                readAcknowledgement(clientSocketDescriptorUDP);
                break;
            case 'E':
                errorMessage(clientSocketDescriptorUDP, commandPDU);
                readAcknowledgement(clientSocketDescriptorUDP);
                break;
            default:
                printf("Input Error.\n");
                break;
        }        
        
        printf("Command (type '?' for list of commands) -> ");
        fgets(tempCommandBuffer, sizeof(tempCommandBuffer), stdin);
        sscanf(tempCommandBuffer, " %c", &commandType);
    }

    close(clientSocketDescriptorUDP);
    exit(0);
}

void registerContent(int sd, pdu registerCommandPDU, char* peerName, size_t peerNameLength){
    /* get name of content from user*/
    char contentName[11];
    char tempContentNameBuffer[100]; //temporary buffer for handling inputs
    printf("Name of Content (10 char long) -> ");
    fgets(tempContentNameBuffer, sizeof(tempContentNameBuffer), stdin);
    sscanf(tempContentNameBuffer, "%10s", contentName);
    
    while(tempContentNameBuffer[0] == '\n'){ //handle user pressing Enter key
        printf("Name of Content (10 char long) -> ");
        fgets(tempContentNameBuffer, sizeof(tempContentNameBuffer), stdin);
        sscanf(tempContentNameBuffer, "%10s", contentName);
    }

    contentName[10] = '\0'; // Ensure null termination

    // zero out the data field of the pdu, just in case
    memset(registerCommandPDU.data, 0, sizeof(registerCommandPDU.data));
    memcpy(registerCommandPDU.data, peerName, peerNameLength);              // bytes 0-10 for peer name
    memcpy(registerCommandPDU.data + 10, contentName, strlen(contentName)); // bytes 10-20 for content name
    
    

    // to do ...
    
    write(sd, &registerCommandPDU, sizeof(registerCommandPDU));
}

void deregisterContent(int sd, pdu deregisterCommandPDU){
    strcpy(deregisterCommandPDU.data, "DE-REGISTER CONTENT.");
    write(sd, &deregisterCommandPDU, sizeof(deregisterCommandPDU));
}

void errorMessage(int sd, pdu errorCommandPDU){
    strcpy(errorCommandPDU.data, "SPECIFIED ERROR.");
    write(sd, &errorCommandPDU, sizeof(errorCommandPDU));
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