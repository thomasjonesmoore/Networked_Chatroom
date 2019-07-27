#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/*------------------------------------------------------------------------
 * Program: demo_client
 *
 * Purpose: allocate a socket, connect to a server, and print all output
 *
 * Syntax: ./demo_client server_address server_port
 *
 * server_address - name of a computer on which server is executing
 * server_port    - protocol port number server is using
 *
 *------------------------------------------------------------------------
 */
int main( int argc, char **argv) {
  struct hostent *ptrh; /* pointer to a host table entry */
  struct protoent *ptrp; /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold an IP address */
  int sd; /* socket descriptor */
  int port; /* protocol port number */
  char *host; /* pointer to host name */
  char buf[1000]; /* buffer for data from the server */
  char isFull, isvalid;

  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET; /* set family to Internet */

  if( argc != 3 ) {
    fprintf(stderr,"Error: Wrong number of arguments\n");
    fprintf(stderr,"usage:\n");
    fprintf(stderr,"./client server_address server_port\n");
    exit(EXIT_FAILURE);
  }

  port = atoi(argv[2]); /* convert to binary */
  if (port > 0) /* test for legal value */
    sad.sin_port = htons((u_short)port);
  else {
    fprintf(stderr,"Error: bad port number %s\n",argv[2]);
    exit(EXIT_FAILURE);
  }

  host = argv[1]; /* if host argument specified */

  /* Convert host name to equivalent IP address and copy to sad. */
  ptrh = gethostbyname(host);
  if ( ptrh == NULL ) {
    fprintf(stderr,"Error: Invalid host: %s\n", host);
    exit(EXIT_FAILURE);
  }

  memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

  /* Map TCP transport protocol name to protocol number. */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }

  /* Create a socket. */
  sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sd < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }

  /* Connect the socket to the specified server. */
  if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
    fprintf(stderr,"connect failed\n");
    exit(EXIT_FAILURE);
  }

  recv(sd,&isFull, sizeof(char), MSG_WAITALL);
  printf("isFull: %c\n", isFull);

  if(isFull == 'N'){
    exit(EXIT_FAILURE);
  }

  isvalid = 'T';
  while(isvalid == 'T'){
    char username[10];/* buffer for username */

    int size = 0;

    //if username is not 1 - 10
    while(size > 10 || size < 1){
      printf("Please enter a username: ");
      scanf("%s",username);
      size = strlen(username);
    }
    send(sd,&size,sizeof(uint8_t),0);
    if(send(sd,username,size,0) < 0){
      exit(EXIT_FAILURE);
    }

    recv(sd,&isvalid, sizeof(char), MSG_WAITALL);
  }

  if(isvalid == 'N'){
    exit(EXIT_FAILURE);
  }
  if(isvalid == 'Y'){
    printf("A new observer has joined\n");
  }

  char c;
  while((c = getchar()) != '\n' && c != EOF);

  /* Main observer loop for recieving server messages. */
  while(1){
    int i = 0;
    while(i < 1000){
      buf[i] = '\0';
      i++;
    }
    int wordLen = 0;
    recv(sd, &wordLen, sizeof(uint16_t), MSG_WAITALL);
    if(recv(sd, &buf, wordLen, MSG_WAITALL) <= 0){
      exit(EXIT_FAILURE);
    }
    printf("%s\n",buf);
  }
}
