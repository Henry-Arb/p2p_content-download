#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define DEFAULT_PORT_NUMBER     3000
#define SERVER_BUFFER_LENGTH    1000

typedef struct pdu {
    char type;
    char data[100];
} pdu;

void registerContent();
void searchContent();
void deregisterContent();
void listOnLineRegisteredContent();

int main(int argc, char** argv){    
    /* get&set server port # */
    int port = DEFAULT_PORT_NUMBER;
    switch(argc){
        case 1: /* default port # */
            break;
        case 2:
            port = atoi(argv[1]);
            break;
        case 3:
            perror("bad # of args\n");
            exit(EXIT_FAILURE);
    }

    /* create server socket */
    int serverSocketDescriptor;
    if( (serverSocketDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        perror("Couldn't create socket\n");
        exit(EXIT_FAILURE);
    }

    /* create address structure for server socket*/
    struct sockaddr_in serverSocketAddr;
    bzero((char*)&serverSocketAddr, sizeof(struct sockaddr_in));
    serverSocketAddr.sin_family = AF_INET;
    serverSocketAddr.sin_port = htons((u_short)port);
    serverSocketAddr.sin_addr.s_addr = INADDR_ANY;

    /* bind server socket with address struct*/
    if( (bind(serverSocketDescriptor, (struct sockaddr*)&serverSocketAddr, sizeof(serverSocketAddr))) == -1 ){
        perror("Couldn't bind server sd to address struct");
        exit(EXIT_FAILURE);
    }

    /** create an address struct for the client*/
    struct sockaddr_in clientSocketAddr;
    int clientSocketAddrLength;
    
    /* create a buffer to store incoming messages*/
    char serverBuffer[SERVER_BUFFER_LENGTH];

    int n;
    while(1){
        clientSocketAddrLength = sizeof(clientSocketAddr);
        if( (n = recvfrom(serverSocketDescriptor, serverBuffer, sizeof(serverBuffer), 
                    0, (struct sockaddr*)&clientSocketAddr, &clientSocketAddrLength)) == -1 ){
                perror("recvfrom error\n");                
        }
    
        if (n < SERVER_BUFFER_LENGTH) {
            serverBuffer[n] = '\0';
        } else {
            serverBuffer[SERVER_BUFFER_LENGTH - 1] = '\0';
        }

        time_t now;
        char pts[100];
        (void) time(&now);
        strncpy(pts, ctime(&now), sizeof(pts) - 1);
        pts[sizeof(pts) - 1] = '\0'; // Ensure null termination

        pdu acknowledgementPDU;
        acknowledgementPDU.type = 'A';
        snprintf(acknowledgementPDU.data, sizeof(acknowledgementPDU.data), "Command received at %s", pts);
        sendto(serverSocketDescriptor, &acknowledgementPDU, sizeof(acknowledgementPDU), 0, (struct sockaddr*)&clientSocketAddr, clientSocketAddrLength);
        
        pdu commandPDU;
        memcpy(&commandPDU, serverBuffer, sizeof(pdu));

        switch(commandPDU.type){
            case 'R':
                printf("%s\n", commandPDU.data);
                registerContent();
                break;            
            case 'T':
                printf("%s\n", commandPDU.data);
                deregisterContent();
                break;
            case 'E':
                printf("Error Message\n");
                printf("%s\n", commandPDU.data);
                break;
            default:
                printf("Default Message\n");
                printf("%s\n", commandPDU.data);
                break;
        }
    }

    close(serverSocketDescriptor);
    return 0;
}

void registerContent(){
    printf("Content Registration\n");
}

void deregisterContent(){
    printf("Content De-registration\n");
}

void searchContent(){
    printf("Search Registration\n");
}