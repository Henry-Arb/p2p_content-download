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

typedef struct content_node {
    int usage;
    char peerName[11];
    char contentName[11];
    uint16_t port;
    struct content_node* next;
} content_node;

content_node* head = NULL; //make global makes shi easier

void registerContent(int, struct sockaddr_in*, socklen_t, pdu);
void deregisterContent(int, struct sockaddr_in*, socklen_t, pdu);
void listOnlineContent(int, struct sockaddr_in*, socklen_t);
void searchContent(int, struct sockaddr_in*, socklen_t, pdu);

content_node* createContentNode(char* peerName, char* contentName, uint16_t port) {
    content_node* newNode = malloc(sizeof(content_node));
    if (newNode == NULL) {
        printf("Error: malloc failed to allocate memory for new node\n");
        exit(1);
    }
    newNode->usage = 0;
    strncpy(newNode->peerName, peerName, sizeof(newNode->peerName) - 1);
    newNode->peerName[sizeof(newNode->peerName) - 1] = '\0';        // null terminate
    strncpy(newNode->contentName, contentName, sizeof(newNode->contentName) - 1);
    newNode->contentName[sizeof(newNode->contentName) - 1] = '\0';  // null terminate
    newNode->port = port;
    newNode->next = NULL;
    return newNode;
}

void insertContentNodeAtEnd(content_node** head, char* peerName, char* contentName, uint16_t port) {
    content_node* nodeToAdd = createContentNode(peerName, contentName, port);
    if (*head == NULL) {
        *head = nodeToAdd;
        return;
    }
    content_node* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = nodeToAdd;
}

int checkContentConflict(content_node* head, char* peerName, char* contentName) {
    content_node* temp = head;
    while (temp != NULL) {
        if (strcmp(temp->peerName, peerName) == 0 && strcmp(temp->contentName, contentName) == 0) {
            return 1; // set flag if conflict
        }
        temp = temp->next;
    }
    return 0; //no flag if no conflict
}

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

    // // Bind to a specific virtual IP address
    // const char* virtual_ip = "192.168.1.100";  // Replace with your virtual IP
    // if (inet_pton(AF_INET, virtual_ip, &serverSocketAddr.sin_addr) <= 0) {
    //     perror("inet_pton() failed");
    //     exit(EXIT_FAILURE);
    // }


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

        // time_t now;
        // char pts[100];
        // (void) time(&now);
        // strncpy(pts, ctime(&now), sizeof(pts) - 1);
        // pts[sizeof(pts) - 1] = '\0'; //
        // pdu acknowledgementPDU;
        // acknowledgementPDU.type = 'A';
        // snprintf(acknowledgementPDU.data, sizeof(acknowledgementPDU.data), "Command received at %s", pts);
        // sendto(serverSocketDescriptor, &acknowledgementPDU, sizeof(acknowledgementPDU), 0, (struct sockaddr*)&clientSocketAddr, clientSocketAddrLength);
        
        pdu commandPDU;
        memcpy(&commandPDU, serverBuffer, sizeof(pdu));
        switch(commandPDU.type){
            case 'R':                
                registerContent(serverSocketDescriptor, &clientSocketAddr, clientSocketAddrLength, commandPDU);
                break;            
            case 'T':
                deregisterContent(serverSocketDescriptor, &clientSocketAddr, clientSocketAddrLength, commandPDU);
                break;
            case 'O':
                listOnlineContent(serverSocketDescriptor, &clientSocketAddr, clientSocketAddrLength);
                break;
            case 'S':
                searchContent(serverSocketDescriptor, &clientSocketAddr, clientSocketAddrLength, commandPDU);
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

void registerContent(int serverSocketDescriptor, struct sockaddr_in* clientSocketAddr, socklen_t clientSocketAddrLength, pdu registerCommandPDU) {
    // De-stuffing
    char peerName[11];
    char contentName[11];
    uint16_t port;

    memcpy(peerName, registerCommandPDU.data, 10);
    memcpy(contentName, registerCommandPDU.data + 10, 10);
    memcpy(&port, registerCommandPDU.data + 20, sizeof(uint16_t));
    peerName[10] = '\0';    // null terminate
    contentName[10] = '\0'; // null terminate
    port = ntohs(port);     // turning it back to host-byte format

    int error = 0;

    if (checkContentConflict(head, peerName, contentName)) {
        error = 1;        
        printf("\nxxxxxxxxxxxxxxxxxxxxxxx Registration Naming Conflict xxxxxxxxxxxxxxxxxxxxxxx\n");
        pdu errorPDU;
        errorPDU.type = 'E';        
        errorPDU.data[0] = '0';
        errorPDU.data[1] = '\0';    // null terminate
        sendto(serverSocketDescriptor, &errorPDU, sizeof(errorPDU), 0, (struct sockaddr*)clientSocketAddr, clientSocketAddrLength);
    } else {        
        insertContentNodeAtEnd(&head, peerName, contentName, port);

        // Send A-type PDU if registration successful
        time_t now;
        char pts[100];
        (void)time(&now);
        strncpy(pts, ctime(&now), sizeof(pts) - 1);
        pts[sizeof(pts) - 1] = '\0';

        pdu acknowledgementPDU;
        acknowledgementPDU.type = 'A';
        snprintf(acknowledgementPDU.data, sizeof(acknowledgementPDU.data), "Acknowledgement From Index Server on %s", pts);
        sendto(serverSocketDescriptor, &acknowledgementPDU, sizeof(acknowledgementPDU), 0, (struct sockaddr*)clientSocketAddr, clientSocketAddrLength);
    }
    if (error != 1)
        printf("----------------------------------------------------------------------------\n");
    printf("Extracted Peer Name:\t\t%s\n", peerName);
    printf("Extracted Content Name:\t\t%s\n", contentName);
    printf("Extracted Network Port Number:\t%u\n", (unsigned int)htons(port)); 
    printf("Converted Host Port Number:\t%u\n", (unsigned int)port);
    
    if (error !=1)
        printf("----------------------------------------------------------------------------\n\n");
    else
        printf("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n");
}

void deregisterContent(int serverSocketDescriptor, struct sockaddr_in* clientSocketAddr, socklen_t clientSocketAddrLength, pdu deregisterCommandPDU) {
    char peerName[11];
    char contentName[11];

    memcpy(peerName, deregisterCommandPDU.data, 10);
    memcpy(contentName, deregisterCommandPDU.data + 10, 10);
    peerName[10] = '\0';    // Null terminate
    contentName[10] = '\0'; // Null terminate

    content_node* temp = head;
    content_node* prev = NULL;

    while (temp != NULL) {
        if (strcmp(temp->peerName, peerName) == 0 && strcmp(temp->contentName, contentName) == 0) { //find the matching peer-name and content-name and unlink from linked list
            if (prev == NULL) {
                head = temp->next; 
            } else {
                prev->next = temp->next;
            }
            
            printf("----------------------------------------------------------------------------\n");
            printf("De-Registering '%s' from [%s]\n", temp->contentName, temp->peerName);
            printf("----------------------------------------------------------------------------\n\n");

            free(temp); //free the shi

            pdu acknowledgementPDU;
            acknowledgementPDU.type = 'A';
            snprintf(acknowledgementPDU.data, sizeof(acknowledgementPDU.data), "Content de-registered successfully.");
            sendto(serverSocketDescriptor, &acknowledgementPDU, sizeof(acknowledgementPDU), 0, (struct sockaddr*)clientSocketAddr, clientSocketAddrLength);
            return;
        }
        prev = temp;        //nextttt
        temp = temp->next;  //nextttt
    }

    pdu errorPDU;
    errorPDU.type = 'E';
    errorPDU.data[0] = '2';
    errorPDU.data[1] = '\0';
    sendto(serverSocketDescriptor, &errorPDU, sizeof(errorPDU), 0, (struct sockaddr*)clientSocketAddr, clientSocketAddrLength);
}

void listOnlineContent(int serverSocketDescriptor, struct sockaddr_in* clientSocketAddr, socklen_t clientSocketAddrLength) {
    char contentList[SERVER_BUFFER_LENGTH] = "";
    content_node* temp = head;
    while (temp != NULL) {
        strncat(contentList, temp->contentName, sizeof(temp->contentName) - 1);
        strcat(contentList, "\n"); // Add a newline for readability
        temp = temp->next;
    }

    pdu responsePDU;
    responsePDU.type = 'O';
    strncpy(responsePDU.data, contentList, sizeof(responsePDU.data) - 1);
    responsePDU.data[sizeof(responsePDU.data) - 1] = '\0';

    if ((sendto(serverSocketDescriptor, &responsePDU, sizeof(responsePDU), 0, (struct sockaddr*)clientSocketAddr, clientSocketAddrLength)) == -1){
        perror("sendto() failed");        
    }
}

void searchContent(int serverSocketDescriptor, struct sockaddr_in* clientSocketAddr, socklen_t clientSocketAddrLength, pdu searchCommandPDU) {
    char peerName[11];
    char contentName[11];

    memcpy(peerName, searchCommandPDU.data, 10);
    memcpy(contentName, searchCommandPDU.data + 10, 10);
    peerName[10] = '\0';    // Null terminate
    contentName[10] = '\0'; // Null terminate

    content_node* temp = head;
    content_node* leastUsedNode = NULL;

    while (temp != NULL) {
        if (strcmp(temp->contentName, contentName) == 0) {
            if (leastUsedNode == NULL || (temp->usage) < (leastUsedNode->usage)) {
                leastUsedNode = temp;
            }
        }
        temp = temp->next;
    }

    if (leastUsedNode != NULL) {
        pdu responsePDU;
        responsePDU.type = 'S';
        uint16_t networkBytePort = htons(leastUsedNode->port);
        
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientSocketAddr->sin_addr), clientIP, INET_ADDRSTRLEN);
        uint16_t clientPort = ntohs(clientSocketAddr->sin_port);

        printf("\n------------------------------- Port Forward -------------------------------\n");
        printf("Sending Network_Port=%u (%s) to Client UDP_Port=(%u)\n", (unsigned int)networkBytePort, leastUsedNode->peerName, (unsigned int)clientPort);
        printf("----------------------------------------------------------------------------\n\n");
        
        memcpy(responsePDU.data, &networkBytePort, sizeof(networkBytePort));
        leastUsedNode->usage += 1;
        sendto(serverSocketDescriptor, &responsePDU, sizeof(responsePDU), 0, (struct sockaddr*)clientSocketAddr, clientSocketAddrLength);
    } else {
        pdu errorPDU;
        errorPDU.type = 'E';
        errorPDU.data[0] = '3';
        errorPDU.data[1] = '\0';        
        sendto(serverSocketDescriptor, &errorPDU, sizeof(errorPDU), 0, (struct sockaddr*)clientSocketAddr, clientSocketAddrLength);
    }
}