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
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

#define DEFAULT_PORT_NUMBER 3000
#define DEFAULT_HOST_NAME "localhost"
#define CONTENT_SERVER_BACKLOG 10
#define CONTENT_CHUNK_SIZE 100

typedef struct pdu
{
    char type;
    char data[100];
} pdu;

typedef struct sd_node
{
    int sd;
    char contentName[11];
    struct sd_node *next;
} sd_node;

char peerName[11];
sd_node *head = NULL;
fd_set sockets_masterSet;
fd_set sockets_activeSet;
char *indexServer;

char *promptContentName(int);
void registerContent(int, struct sockaddr_in *, char *);
void deregisterContent(int, struct sockaddr_in *, char *);
void listOnlineContent(int, struct sockaddr_in *);
void downloadContent(int, struct sockaddr_in *);
uint16_t searchContent(int, struct sockaddr_in *, char *);
void provideContent(int);
void quit(int, struct sockaddr_in *);
void printErrorType(int errorCode);
void listLocalFiles();

/*********************** sd_node linked-list struct operations ***********************/
void printNodeList(sd_node *head)
{
    sd_node *temp = head;
    if (temp == NULL)
    {
        printf("\nxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
        printf("TCP Socket List is empty\n");
        printf("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n");
        return;
    }
    printf("\n------------------------------ SD Linked List ------------------------------\n");
    while (temp != NULL)
    {
        printf("sd=%d\tcontent=%s\n", temp->sd, temp->contentName);
        temp = temp->next;
    }
    printf("----------------------------------------------------------------------------\n\n");
}

sd_node *createNode(int sd, char *contentName)
{
    sd_node *newNode = malloc(sizeof(sd_node));
    if (newNode == NULL)
    {
        printf("Error: malloc failed to allocate memory for new node\n");
        exit(1);
    }
    newNode->sd = sd;
    strncpy(newNode->contentName, contentName, sizeof(newNode->contentName) - 1);
    newNode->contentName[sizeof(newNode->contentName) - 1] = '\0'; // null terminate
    newNode->next = NULL;
    return newNode;
}

void createAndInsertNodeAtEnd(sd_node **head, int sd, char *contentName)
{
    sd_node *nodeToAdd = createNode(sd, contentName);
    // handle case where list is empty
    if (*head == NULL)
    { // remember: the function parameter is initially node**, a pointer to a pointer. so we need to deference it once to get the pointer to head node itself (if it exists)
        *head = nodeToAdd;
        return;
    }
    sd_node *temp = *head; // running this line means the pointer of the head node itself exists. we again dereference (otherwise, it's just a pointer to a pointer, not a node pointer)
    while (temp->next != NULL)
    {
        temp = temp->next;
    }
    temp->next = nodeToAdd;
}

void removeNodeBySpecificValue(sd_node **head, char *contentName)
{
    if (head == NULL || *head == NULL)
    {
        return;
    }

    sd_node *temp = *head;
    sd_node *prev = NULL;

    if (strcmp(temp->contentName, contentName) == 0)
    { // in this case, head-node is the one that has the content to be removed
        *head = temp->next;
        close(temp->sd); // Close the TCP socket
        free(temp);
        return;
    }

    while (temp != NULL && strcmp(temp->contentName, contentName) != 0)
    {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
    { // contentName not found in linked list
        return;
    }
    prev->next = temp->next; // Unlink current node from the linked list by making the prev node's pointer -> next node's pointer
    close(temp->sd);         // Close the TCP socket
    free(temp);              // Free memory
}
/*************************************************************************************/

int main(int argc, char **argv)
{
    /* get&set server port # */
    int indexPort;
    switch (argc)
    {
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
    bzero((char *)&indexServerSocketAddr, sizeof(struct sockaddr_in));
    indexServerSocketAddr.sin_family = AF_INET;
    indexServerSocketAddr.sin_port = htons((u_short)indexPort);

    struct hostent *indexServerName;
    if (indexServerName = gethostbyname(indexServer))
    {
        memcpy(&indexServerSocketAddr.sin_addr, indexServerName->h_addr, indexServerName->h_length);
        indexServer = inet_ntoa(*(struct in_addr *)indexServerName->h_addr); // Convert to IP address string
    }
    else if ((indexServerSocketAddr.sin_addr.s_addr = inet_addr(indexServer)) == INADDR_NONE)
    {
        perror("Couldn't get host entry\n");
        exit(EXIT_FAILURE);
    }

    /* create UDP client socket */
    int clientSocketDescriptorUDP;
    if ((clientSocketDescriptorUDP = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("Couldn't create UDP socket\n");
        exit(EXIT_FAILURE);
    }

    /* connect to host */
    if ((connect(clientSocketDescriptorUDP, (struct sockaddr *)&indexServerSocketAddr, sizeof(indexServerSocketAddr))) == -1)
    {
        perror("Couldn't connect to host\n");
        exit(EXIT_FAILURE);
    }

    /* get name for peer (no more than 10 characters) */
    char tempNameBuffer[100]; // temporary buffer for peer-name. needed to handle bad input
    printf("Choose a username -> ");
    fgets(tempNameBuffer, sizeof(tempNameBuffer), stdin);
    sscanf(tempNameBuffer, "%10s", peerName);
    peerName[10] = '\0'; // null terminate
    printf("Hi %s.\n", peerName);

    /* prompt user for a command */
    char commandType;
    char tempCommandBuffer[100]; // temporary buffer for handling bad command inputs

    /* set up fd_sets to track TCP socket activity */

    FD_ZERO(&sockets_masterSet);
    FD_ZERO(&sockets_activeSet);
    FD_SET(0, &sockets_masterSet); // 0 for stdin

    while (1)
    {
        printf("Command (type '?' for list of commands) -> ");
        fflush(stdout);

        sockets_activeSet = sockets_masterSet; // "refresh" active set

        if (select(FD_SETSIZE, &sockets_activeSet, NULL, NULL, NULL) < 0)
        {
            perror("select() error");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(0, &sockets_activeSet))
        {
            char commandType;
            char tempCommandBuffer[100];

            if (fgets(tempCommandBuffer, sizeof(tempCommandBuffer), stdin) != NULL)
            {
                sscanf(tempCommandBuffer, " %c", &commandType);

                if (commandType == 'Q')
                {
                    break;
                }

                pdu commandPDU;
                commandPDU.type = commandType;
                switch (commandType)
                {
                case '?':
                    printf("[O]:\tList ONLINE Content\n");
                    printf("\n[R]:\tContent Registration\n");
                    printf("[T]:\tContent De-registration\n");
                    printf("[D]:\tDownload Content\n");
                    printf("[L]:\tList LOCALLY Registered Content\n");
                    printf("[l]:\tList LOCAL Content\n");
                    printf("[Q]:\tQuit\n\n");
                    break;
                case 'R':
                {
                    char *contentName = promptContentName(0);
                    if (contentName != NULL)
                    {
                        registerContent(clientSocketDescriptorUDP, &indexServerSocketAddr, contentName);
                        free(contentName); // Free the allocated memory for content name
                    }
                    printNodeList(head);
                    break;
                }
                case 'T':
                {
                    char *contentName = promptContentName(2);
                    if (contentName != NULL)
                    {
                        deregisterContent(clientSocketDescriptorUDP, &indexServerSocketAddr, contentName);
                        free(contentName); // Free the allocated memory for content name
                    }
                    printNodeList(head);
                    break;
                }
                case 'O':
                    listOnlineContent(clientSocketDescriptorUDP, &indexServerSocketAddr);
                    break;
                case 'D':
                    downloadContent(clientSocketDescriptorUDP, &indexServerSocketAddr);
                    break;
                case 'L':
                    printNodeList(head);
                    break;
                case 'l':
                    listLocalFiles();
                    break;
                default:
                    printf("Input Error.\n");
                    break;
                }
                printf("\n");
            }
        }

        /* Check if the opps have intercepted our TCP sockets */
        int i;
        for (i = 0; i < FD_SETSIZE; i++)
        {
            if (FD_ISSET(i, &sockets_activeSet) && i != 0)
            { // the '0' in `i!=0` refers to stdin
                provideContent(i);
            }
        }
    }

    printf("\n------------------------ De-registering all content ------------------------\n");
    quit(clientSocketDescriptorUDP, &indexServerSocketAddr);
    printf("----------------------------------------------------------------------------\n");

    printf("\nEXITING PROGRAM.\n");
    close(clientSocketDescriptorUDP);
    exit(0);
}

char *promptContentName(int promptChoice)
{
    int promptChoiceSize;
    const char *promptMessage;

    switch (promptChoice)
    {
    case 0: // "register" = 8 + '\0' = 9
        promptChoiceSize = 9;
        promptMessage = "Register";
        break;
    case 1: // "download" = 8 + '\0' = 9
        promptChoiceSize = 9;
        promptMessage = "Download";
        break;
    case 2: // "de-register" = 11 + '\0' = 12
        promptChoiceSize = 12;
        promptMessage = "De-register";
        break;
    default: // "enter" = 5 + '\0' = 6
        promptChoiceSize = 6;
        promptMessage = "enter";
        break;
    }

    char *contentName = malloc(11); // using malloc here as returning local variable pointer is not guaranteed to be saved forever outside the function. malloc saves to heap so it'll stay alive outside of function
    if (contentName == NULL)
    {
        printf("malloc() error for contentName\n");
        exit(EXIT_FAILURE);
    }
    char tempContentNameBuffer[100]; // temporary buffer for handling inputs
    printf("Name of Content (10 char long) to %s-> ", promptMessage);
    fgets(tempContentNameBuffer, sizeof(tempContentNameBuffer), stdin);
    sscanf(tempContentNameBuffer, "%10s", contentName);
    while (tempContentNameBuffer[0] == '\n')
    { // handle user pressing Enter key
        printf("Name of Content (10 char long) to %s -> ", promptMessage);
        fgets(tempContentNameBuffer, sizeof(tempContentNameBuffer), stdin);
        sscanf(tempContentNameBuffer, "%10s", contentName);
    }
    contentName[10] = '\0';
    return contentName;
}

void registerContent(int sd, struct sockaddr_in *server_addr, char *contentName)
{
    pdu registerCommandPDU;
    registerCommandPDU.type = 'R';

    /* zero out the data field of the pdu, just in case */
    memset(registerCommandPDU.data, 0, sizeof(registerCommandPDU.data));

    /* copy peer name and content name to pdu data field */
    memcpy(registerCommandPDU.data, peerName, sizeof(peerName));            // bytes 0-10 for peer name
    memcpy(registerCommandPDU.data + 10, contentName, strlen(contentName)); // bytes 10-20 for content name

    /* create TCP socket for peer becoming a content server */
    int clientSocketDescriptorTCP;
    if ((clientSocketDescriptorTCP = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("TCP socket() error.\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in reg_addr;
    reg_addr.sin_family = AF_INET;
    reg_addr.sin_port = htons(0); // assining 0 to htons() => TCP module to choose a unique port number
    reg_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((bind(clientSocketDescriptorTCP, (struct sockaddr *)&reg_addr, sizeof(reg_addr))) == -1)
    {
        perror("bind() error");
        exit(EXIT_FAILURE);
    }

    socklen_t alen = sizeof(struct sockaddr_in);
    if (getsockname(clientSocketDescriptorTCP, (struct sockaddr *)&reg_addr, &alen) == -1)
    {
        perror("getsockname() error");
        exit(EXIT_FAILURE);
    }

    uint16_t networkBytePort = reg_addr.sin_port;
    uint16_t hostBytePort = ntohs(networkBytePort);
    memcpy(registerCommandPDU.data + 20, &networkBytePort, sizeof(networkBytePort));

    if (sendto(sd, &registerCommandPDU, sizeof(registerCommandPDU), 0, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("sendto() failed");
        exit(EXIT_FAILURE);
    }

    pdu responsePDU;
    int addr_len = sizeof(struct sockaddr_in);
    int n;
    if ((n = recvfrom(sd, &responsePDU, sizeof(responsePDU), 0, (struct sockaddr *)server_addr, &addr_len)) == -1)
    {
        perror("recvfrom error\n");
        exit(EXIT_FAILURE);
    }

    if (responsePDU.type == 'A')
    {
        printf("\n----------------------------------------------------------------------------\n");
        printf("%s", responsePDU.data);

        if ((listen(clientSocketDescriptorTCP, CONTENT_SERVER_BACKLOG)) == -1)
        {
            perror("listen() error");
            exit(EXIT_FAILURE);
        }

        /* store sd of this TCP socket in a linked list, otherwise lost forever after function call finishes*/
        createAndInsertNodeAtEnd(&head, clientSocketDescriptorTCP, contentName);
        /* Add the new TCP socket to the master set */
        FD_SET(clientSocketDescriptorTCP, &sockets_masterSet);

        printf("Peer Name:\t\t%s\n", registerCommandPDU.data);
        printf("Content Name:\t\t%s\n", registerCommandPDU.data + 10);
        printf("Host Port Number:\t%u\n", (unsigned int)hostBytePort);
        printf("Network Port Number:\t%u\n", (unsigned int)(*(uint16_t *)(registerCommandPDU.data + 20)));
        /* printf("Network Port Number:\t%u\n", (unsigned int)networkBytePort); // just to check if the pointer to registerCommandPDU.data+20 really is pointing to the right*value */
        printf("----------------------------------------------------------------------------\n\n");

        return;
    }

    else if (responsePDU.type == 'E')
    {
        if (responsePDU.data[0] == '0')
        {
            int counter = 0; // allow 3 attempts to re-name yourself
            while (counter < 3)
            {
                printErrorType(atoi(responsePDU.data));
                char tempNameBuffer[100];
                printf("Choose a new username -> ");
                fgets(tempNameBuffer, sizeof(tempNameBuffer), stdin);
                sscanf(tempNameBuffer, "%10s", peerName);
                peerName[10] = '\0';
                printf("Resending %s as %s.\n", contentName, peerName);

                memset(registerCommandPDU.data, 0, sizeof(registerCommandPDU.data));

                memcpy(registerCommandPDU.data, peerName, sizeof(peerName));
                memcpy(registerCommandPDU.data + 10, contentName, strlen(contentName));
                memcpy(registerCommandPDU.data + 20, &networkBytePort, sizeof(uint16_t));

                if (sendto(sd, &registerCommandPDU, sizeof(registerCommandPDU), 0, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in)) == -1)
                {
                    perror("sendto() failed");
                    exit(EXIT_FAILURE);
                }

                if ((n = recvfrom(sd, &responsePDU, sizeof(responsePDU),
                                  0, (struct sockaddr *)server_addr, &addr_len)) == -1)
                {
                    perror("recvfrom error\n");
                    exit(EXIT_FAILURE);
                }

                if (responsePDU.type == 'A')
                {
                    printf("Acknowledgement from server: %s\n", responsePDU.data);

                    if ((listen(clientSocketDescriptorTCP, CONTENT_SERVER_BACKLOG)) == -1)
                    {
                        perror("listen() error");
                        exit(EXIT_FAILURE);
                    }

                    /* store sd of this TCP socket in a linked list, otherwise lost forever after function call finishes*/
                    createAndInsertNodeAtEnd(&head, clientSocketDescriptorTCP, contentName);
                    /* Add the new TCP socket to the master set */
                    FD_SET(clientSocketDescriptorTCP, &sockets_masterSet);

                    printf("Peer Name:\t\t%s\n", registerCommandPDU.data);
                    printf("Content Name:\t\t%s\n", registerCommandPDU.data + 10);
                    printf("Host Port Number:\t%u\n", (unsigned int)hostBytePort);
                    printf("Network Port Number:\t%u\n", (unsigned int)(*(uint16_t *)(registerCommandPDU.data + 20)));
                    /* printf("Network Port Number:\t%u\n", (unsigned int)networkBytePort); // just to check if the pointer to registerCommandPDU.data+20 really is pointing to the right*value */
                    printf("\n");

                    return;
                }
                counter += 1;
            }
            printf("Too many failed attempts. ");
        }
        printf("Registration NOT successful.\n");
    }
    close(clientSocketDescriptorTCP);
}

void listOnlineContent(int sd, struct sockaddr_in *server_addr)
{
    pdu listCommandPDU;
    listCommandPDU.type = 'O';
    memset(listCommandPDU.data, 0, sizeof(listCommandPDU.data));

    if (sendto(sd, &listCommandPDU, sizeof(listCommandPDU), 0, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("sendto() failed");
        exit(EXIT_FAILURE);
    }

    pdu responsePDU;
    int addr_len = sizeof(struct sockaddr_in);
    int n;
    if ((n = recvfrom(sd, &responsePDU, sizeof(responsePDU), 0, (struct sockaddr *)server_addr, &addr_len)) == -1)
    {
        perror("recvfrom error\n");
        exit(EXIT_FAILURE);
    }

    if (responsePDU.type == 'O')
    {
        printf("\n------------------------------ Online Content ------------------------------\n");
        printf("%s\n", responsePDU.data);
        printf("----------------------------------------------------------------------------\n\n");
    }
    else
    {
        printf("Unexpected response from server.\n");
    }
}

void deregisterContent(int sd, struct sockaddr_in *server_addr, char *contentName)
{
    pdu deregisterCommandPDU;
    deregisterCommandPDU.type = 'T';
    memset(deregisterCommandPDU.data, 0, sizeof(deregisterCommandPDU.data)); // zero out

    memcpy(deregisterCommandPDU.data, peerName, sizeof(peerName));            // set
    memcpy(deregisterCommandPDU.data + 10, contentName, strlen(contentName)); // set

    if ((sendto(sd, &deregisterCommandPDU, sizeof(deregisterCommandPDU), 0, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in))) == -1)
    {
        perror("sendto() failed");
        exit(EXIT_FAILURE);
    }

    pdu responsePDU;
    int addr_len = sizeof(struct sockaddr_in);
    int n;
    if ((n = recvfrom(sd, &responsePDU, sizeof(responsePDU), 0, (struct sockaddr *)server_addr, &addr_len)) == -1)
    {
        perror("recvfrom error\n");
        exit(EXIT_FAILURE);
    }

    if (responsePDU.type == 'A')
    {
        printf("Acknowledgement from server: %s\n", responsePDU.data);

        // Find the socket descriptor for the content
        sd_node *temp = head;
        int content_sd = -1;
        while (temp != NULL)
        {
            if (strcmp(temp->contentName, contentName) == 0)
            {
                content_sd = temp->sd;
                break;
            }
            temp = temp->next;
        }

        if (content_sd != -1)
        {
            // Remove the socket descriptor from the master and active set
            FD_CLR(content_sd, &sockets_masterSet);
            FD_CLR(content_sd, &sockets_activeSet);

            // Remove the content node from the linked list
            removeNodeBySpecificValue(&head, contentName);
        }
    }
    else if (responsePDU.type == 'E')
    {
        printErrorType(atoi(responsePDU.data));
        printf("De-registration cancelled.\n");
    }

    else
    {
        printf("Unexpected server response.\n");
    }
}

void provideContent(int server_sd)
{
    int client_sd;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if ((client_sd = accept(server_sd, (struct sockaddr *)&client_addr, &addr_len)) == -1)
    {
        perror("accept() error");
        return;
    }

    /*
    char buffer[100];   //TESTING RECEIVING
    int bytes_received = recv(client_sd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received == -1) {
        perror("recv error");
        return;
    } else {
        buffer[bytes_received] = '\0';
        printf("\nMessage from the requesting peer: %s\n", buffer);
    }

    const char *message = "HELLO HERE 2 GIB U GUD STUFF!"; //TESTING MESSAGING
    if (send(client_sd, message, strlen(message), 0) == -1) {
        perror("send error");
        return;
    }*/

    pdu requestPDU;
    int bytes_received = recv(client_sd, &requestPDU, sizeof(requestPDU), 0);
    if (bytes_received == -1)
    {
        perror("recv error");
        close(client_sd);
        return;
    }

    if (requestPDU.type != 'D')
    {
        perror("Unexpected PDU type received");
        close(client_sd);
        return;
    }

    char contentName[11];
    strncpy(contentName, requestPDU.data, 10);
    contentName[10] = '\0';

    pdu responsePDU;

    FILE *contentFile = fopen(contentName, "rb");
    if (contentFile == NULL)
    {

        responsePDU.type = 'E';
        responsePDU.data[0] = '1';
        responsePDU.data[1] = '\0';

        if (send(client_sd, &responsePDU, sizeof(responsePDU), 0) == -1)
        {
            perror("send() error");
        }

        perror("File not found.\n");
        close(client_sd);
        return;
    }

    responsePDU.type = 'C';
    size_t bytesRead;
    while ((bytesRead = fread(responsePDU.data, 1, CONTENT_CHUNK_SIZE, contentFile)) > 0)
    {
        if (send(client_sd, &responsePDU, sizeof(pdu), 0) == -1)
        {
            perror("send() error");
            fclose(contentFile);
            close(client_sd);
            return;
        }
    }

    fclose(contentFile);
    close(client_sd);
}

void downloadContent(int sd, struct sockaddr_in *server_addr)
{
    char *contentName = promptContentName(1);

    /* request index server to search for a content provider. returns port # (host byte format)*/
    uint16_t contentPort_networkByte = searchContent(sd, server_addr, contentName);
    if (contentPort_networkByte == 0)
    {
        return;
    }
    uint16_t contentPort_hostByte = ntohs(contentPort_networkByte);
    printf("Content found at\thost port:\t%u\n", (unsigned int)contentPort_hostByte);
    printf("\t\t\tnetwork port:\t%u\n", (unsigned int)contentPort_networkByte);

    int sd_providerLink;
    if ((sd_providerLink = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Can't creat a socket\n");
        exit(1);
    }

    struct sockaddr_in content_server;
    bzero((char *)&content_server, sizeof(struct sockaddr_in));
    content_server.sin_family = AF_INET;
    content_server.sin_port = contentPort_networkByte;

    // struct	hostent	*hp;
    //... WHAT HOST IP ???

    // CHANGE TO #DEFINE
    if (inet_pton(AF_INET, indexServer, &content_server.sin_addr) <= 0)
    {
        fprintf(stderr, "Invalid address/ Address not supported \n");
        close(sd_providerLink);
        return;
    }

    // Test to see what IP Address is being use for TCP connection
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(content_server.sin_addr), ip_str, INET_ADDRSTRLEN);
    printf("Attempting to connect to IP: %s, Port: %u\n", ip_str, ntohs(content_server.sin_port));

    if (connect(sd_providerLink, (struct sockaddr *)&content_server, sizeof(content_server)) == -1)
    {
        perror("Connection Failed");
        fprintf(stderr, "Error code: %d\n", errno);
        close(sd_providerLink);
        return;
    }

    /*
    const char *message = "Hello from the requesting peer."; //TESTING MESSAGING
    if (send(sd_providerLink, message, strlen(message), 0) == -1) {
        perror("send error");
        close(sd_providerLink);
        return;
    }

    char buffer[100];   // TESTING RECEIVING
    int bytes_received = recv(sd_providerLink, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received == -1) {
        perror("recv error");
        return;
    } else {
        buffer[bytes_received] = '\0';
        printf("Message from the content provider: %s\n", buffer);
    }*/

    pdu requestPDU;
    requestPDU.type = 'D';
    strncpy(requestPDU.data, contentName, sizeof(requestPDU.data) - 1);
    requestPDU.data[sizeof(requestPDU.data) - 1] = '\0';

    if (send(sd_providerLink, &requestPDU, sizeof(requestPDU), 0) == -1)
    {
        perror("send error");
        close(sd_providerLink);
        return;
    }

    FILE *contentFile = fopen(contentName, "wb");
    if (contentFile == NULL)
    {
        perror("File open error");
        close(sd_providerLink);
        return;
    }

    pdu responsePDU;
    int bytes_received;
    while ((bytes_received = recv(sd_providerLink, &responsePDU, sizeof(responsePDU), 0)) > 0)
    {
        if (responsePDU.type == 'E')
        {
            printErrorType(atoi(responsePDU.data));
            printf("Registration Cancelled.\n");
            return;
        }
        else if (responsePDU.type != 'C')
        {
            perror("Unexpected PDU type received");
            printf("Registration Cancelled.\n");
            return;
        }

        fwrite(responsePDU.data, 1, bytes_received - sizeof(char), contentFile);
    }

    if (bytes_received == -1)
    {
        perror("recv error");
        return;
    }

    printf("Content downloaded successfully.\n");
    fclose(contentFile);

    printf("Registering content ...\n");
    registerContent(sd, server_addr, contentName);
}

/**
 * @sd: The socket descriptor for communication with the index server.
 * @server_addr: The sockaddr_in structure containing the index server address.
 * @contentName: the content the requesting peer is searching
 *
 * Return: The network byte order port number of the peer with the requested content,
 *         or 0 if an error occurs or the content is not found.
 */
uint16_t searchContent(int sd, struct sockaddr_in *server_addr, char *contentName)
{
    pdu searchCommandPDU;
    searchCommandPDU.type = 'S';

    memset(searchCommandPDU.data, 0, sizeof(searchCommandPDU.data)); // zero out

    memcpy(searchCommandPDU.data, peerName, sizeof(peerName));            // Bytes 0-10 for peer name
    memcpy(searchCommandPDU.data + 10, contentName, strlen(contentName)); // Bytes 10-20 for content name

    if (sendto(sd, &searchCommandPDU, sizeof(searchCommandPDU), 0, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("sendto() failed");
        exit(EXIT_FAILURE);
    }

    pdu responsePDU;
    int addr_len = sizeof(struct sockaddr_in);
    int n;
    if ((n = recvfrom(sd, &responsePDU, sizeof(responsePDU), 0, (struct sockaddr *)server_addr, &addr_len)) == -1)
    {
        perror("recvfrom error\n");
        exit(EXIT_FAILURE);
    }

    if (responsePDU.type == 'S')
    {
        uint16_t networkBytePort;
        memcpy(&networkBytePort, responsePDU.data, sizeof(uint16_t));
        return networkBytePort;
    }
    else if (responsePDU.type == 'E')
    {
        printErrorType(atoi(responsePDU.data));
        return 0;
    }

    printf("Unexpected response from server.\n");
    return 0;
}

void quit(int sd, struct sockaddr_in *server_addr)
{
    sd_node *temp = head;
    while (temp != NULL)
    {
        sd_node *next = temp->next;
        deregisterContent(sd, server_addr, temp->contentName);
        temp = next;
    }

    head = NULL; // idk if this makes a diff
}

void printErrorType(int errorCode)
{
    switch (errorCode)
    {
    case 0:
        printf("Error from server: naming conflict.\n");
        break;
    case 1:
        printf("Error from peer: File does not exist.\n");
        break;
    case 2:
        printf("Error from server: Content not found.\n");
        break;
    case 3:
        printf("Error from server: Search Fail.\n");
        break;
    }
}

void listLocalFiles()
{
    pid_t pid = fork();

    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        // Child process
        char *args[] = {"ls", NULL};
        execvp(args[0], args);

        // If execvp fails
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
    }
}