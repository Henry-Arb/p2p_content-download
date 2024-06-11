/* Index Server 

Message types:
R - used for registration
A - used by the server to acknowledge the success of registration
Q - used by chat users for de-registration
D - download content between peers (not used here)
C - Content (not used here)
S - Search content
L - Location of the content server peer
E - Error messages from the Server

*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>

#define MSG1 "Cannot find content"
#define BUFLEN	100
#define NAMESIZ	20
#define MAX_NUM_CON	200
                                                                                
typedef struct entry{
	char	usr[NAMESIZ];
	struct sockaddr_in addr;
	short	token;		
	struct entry *next;
} ENTRY; 

typedef struct{
	char name[NAMESIZ];
	ENTRY	*head;
} LIST;
LIST	list[MAX_NUM_CON];
int	max_index=0;

typedef struct{
	char type;
	char data[BUFLEN];
} PDU;
PDU 	tpdu;

void search(int, char *, struct sockaddr_in *);
void registration(int, char *, struct sockaddr_in *); 
void deregistration(int, char *, struct sockaddr_in *);

/*
 *------------------------------------------------------------------------
 * main - Iterative UDP server for Content Indexing service
 *------------------------------------------------------------------------
 */

int
main(int argc, char *argv[])
{
	struct sockaddr_in sin, *p_addr;	/* the from address of a client	*/
	ENTRY	*p_entry;
	char	*service = "10000";	/* service name or port number	*/
	char	name[NAMESIZ], usr[NAMESIZ];
	int	alen = sizeof(struct sockaddr_in);	/* from-address length		*/
        int     s, n, i, len,p_sock;        /* socket descriptor and socket type    */
	int	pdulen=sizeof(PDU);
	struct  hostent         *hp;
	PDU	rpdu;
	struct sockaddr_in fsin;	/* the from address of a client	*/


	for(n=0; n<MAX_NUM_CON; n++)
		list[n].head = NULL;
                                                                                
	switch (argc) {
	case	1:
		break;
	case	2:
		service = argv[1];
		break;
	default:
		fprintf(stderr, "usage: chat_server [host [port]]\n");

	}
                                                                        
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;

   /* Map service name to port number */
        sin.sin_port = htons((u_short)atoi(service));
                                                                                
    /* Allocate a socket */
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0){
		fprintf(stderr, "can't creat socket\n");
		exit(1);
	}
                                                                                
    /* Bind the socket */
        if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		fprintf(stderr, "can't bind to %s port\n", service);

	while (1) {
		if ( (n=recvfrom(s, &rpdu, pdulen, 0,(struct sockaddr *)&fsin, &alen)) < 0)
		  printf("recvfrom error: n=%d\n",n);


	/*	Content Registration Request			*/
		if(rpdu.type == 'R'){
		  /*	Call registration()
		  */		
		}

	/* Search Content		*/
		if(rpdu.type == 'S'){ 
		  /* Call search()
		  */
		}

	/*	List current Content */
		if(rpdu.type == 'O'){
		  /* Read from the content list and send the list to the 
		     client 		*/

		}
			
	/*	De-registration		*/
		if(rpdu.type == 'T'){
		  /* Call deregistration()
		  */
		}
	}
	return;
}

void search(int s, char *data, struct sockaddr_in *addr)
{
	/* Search content list and return the answer:
	   If found, send the address of the selected content server. 
	*/
}

void deregistration(int s, char *data, struct sockaddr_in *addr)
{
	/* De-register the server of that content 
	*/
}

void registration(int s, char *data, struct sockaddr_in *addr)
{
	/* Register the content and the server of the content
	*/
}
