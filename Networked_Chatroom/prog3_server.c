/* Enya Garcia-Collazo
   Jordan Juel
   Thomas Jones-Moore

   Program 3
   CSCI 367

   Sources:
   1. Original server code from Brian Hutchinson
*/

#define _GNU_SOURCE
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "prog3_server.h"

#define QLEN 6 /* size of request queue */
#define COMMAND_LINE_ARG_SIZE 3 /* number of command line arguments */

int pCount, oCount;

int main(int argc, char **argv) {
  struct protoent *ptrp, *ptrp2, *current_ptrp; /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold server's address */
  struct sockaddr_in sad2; /* structure to hold server's address */
  struct sockaddr_in cad; /* structure to hold client's address */
  struct client PClients[257]; /* Holds all participant client structures */
  int sd, sd2; /* socket descriptors */
  int port, port2; /* protocol port number, port = participant, port2 = observer */
  unsigned int alen; /* length of address */
  int optval = 1; /* boolean value when we set socket option */
  char bufWithName[1014]; /* General message buffer */
  char serverFull; /* Character indicating if server full */
  char User[25]; /* Buffer for user has joined message */
  uint8_t nameLen = 0; /* username length */

  if( argc != COMMAND_LINE_ARG_SIZE ) {
    fprintf(stderr,"Error: Wrong number of arguments\n");
    fprintf(stderr,"usage:\n");
    fprintf(stderr,"./server port port2\n");
    exit(EXIT_FAILURE);
  }

  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  memset((char *)&sad2,0,sizeof(sad2)); /* clear sockaddr structure */
  sad.sin_family = AF_INET; /* set family to Internet */
  sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */
  sad2.sin_family = AF_INET; /* set family to Internet */
  sad2.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

  port = atoi(argv[1]); /* convert argument to binary */
  if (port > 0) { /* test for illegal value */
    sad.sin_port = htons((u_short)port);
  } else { /* print error message and exit */
    fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
    exit(EXIT_FAILURE);
  }

  port2 = atoi(argv[2]); /* convert argument to binary */
  if (port2 > 0) { /* test for illegal value */
    sad2.sin_port = htons((u_short)port2);
  } else { /* print error message and exit */
    fprintf(stderr,"Error: Bad port number %s\n",argv[2]);
    exit(EXIT_FAILURE);
  }

  /* Loops twice to setup socket descriptors for both client ports */
  for(int i = 0; i < 2; i++){
    int current_sd;
    struct sockaddr_in current_sad;
    if (i == 0){
      current_sd = sd;
      current_sad = sad;
      current_ptrp  = ptrp;
    }

    else{
      current_sd = sd2;
      current_sad = sad2;
      current_ptrp  = ptrp2;
    }

    /* Map TCP transport protocol name to protocol number */
    if ( ((long int)(current_ptrp = getprotobyname("tcp"))) == 0) {
      fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
      exit(EXIT_FAILURE);
    }

    /* Create a socket */
    current_sd = socket(PF_INET, SOCK_STREAM, current_ptrp->p_proto);
    if ((current_sd) < 0) {
      fprintf(stderr, "Error: Socket creation failed\n");
      exit(EXIT_FAILURE);
    }

    /* Allow reuse of port - avoid "Bind failed" issues */
    if( setsockopt(current_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
      fprintf(stderr, "Error Setting socket option failed\n");
      exit(EXIT_FAILURE);
    }

    /* Bind a local address to the socket */
    if (bind(current_sd, (struct sockaddr *)&current_sad, sizeof(current_sad)) < 0) {
      fprintf(stderr,"Error: Bind failed\n");
      exit(EXIT_FAILURE);
    }

    /* Specify size of request queue */
    if (listen(current_sd, QLEN) < 0) {
      fprintf(stderr,"Error: Listen failed\n");
      exit(EXIT_FAILURE);
    }

    if (i == 0){
      sd = current_sd;
      sad = current_sad;
      ptrp  = current_ptrp;
    }
    else{
      sd2 = current_sd;
      sad2 = current_sad;
      ptrp2  = current_ptrp;
    }
  }

  /* Init all usernames as null */
  char NList[1] = {'\0'};
  int k = 0;
  while(k < 255){
    strcpy(PClients[k].username, NList);
    k++;
  }

  /* Main server loop - accept and handle requests
     Handle accepts at start of while loop */
  while (1) {
    alen = sizeof(cad);
    struct client tempOClient;
    struct client tempPClient;
    char validName;

    /* Used for participant and observer selects */
    fd_set readfd;
    fd_set readfd2;
    FD_SET(sd,&readfd);
    FD_SET(sd2,&readfd2);
    struct timeval timer;
    timer.tv_sec = 0;
    timer.tv_usec = 1;

    /* Listens for connection for participant port. If select is > 0, then
       the participant has a message to send to server. */
    if(select(sd+1,&readfd, NULL, NULL, &timer) > 0 ){
      tempPClient.sd = accept(sd, (struct sockaddr *)&cad, &alen);
      if(tempPClient.sd != EWOULDBLOCK && tempPClient.sd < 0){
        fprintf(stderr, "Error: Accept failed\n");
        exit(EXIT_FAILURE);
      }

      printf("participant connected \n");

      /* Checks for full server */
      if(pCount == 255){
        serverFull = 'N';
        send(tempPClient.sd,&serverFull,sizeof(char),MSG_NOSIGNAL);
      }
      else{
        serverFull = 'Y';
        send(tempPClient.sd,&serverFull,sizeof(char),MSG_NOSIGNAL);
      }

      /* Getting name length and name from user client and checking for
         validity */
      validName = 'T';
      struct timeval usernameTimer;
      usernameTimer.tv_sec = 60;
      usernameTimer.tv_usec = 0;

      /* Checks for valid name from participant, loops accordingly. */
      while(validName == 'T' && serverFull != 'N'){
        char username[10];
        FD_ZERO(&readfd);
        FD_SET(tempPClient.sd,&readfd);

        /* If select returns > 0 then the participant wants to send username to
           server */
        if(select(tempPClient.sd+1, &readfd,NULL,NULL, &usernameTimer) > 0){
          recv(tempPClient.sd, &nameLen, sizeof(uint8_t), MSG_WAITALL);
          recv(tempPClient.sd, username, nameLen, MSG_WAITALL);
          username[nameLen] = '\0';

          /* Check name validity and if taken */
          int isValid = checkNameValidity(username);
          int isTaken = checkNameTaken(PClients, username);

          if(!isValid){
            validName = 'I';
          }
          else if(isTaken){
            validName = 'T';
          }
          else{
            /* If username valid, appends to participant list and sends out to all
               observers that a new participant has joined. */
            validName = 'Y';
            strcpy(tempPClient.username, username);
            appendList(PClients, &tempPClient);
            pCount++;

            strcpy(User, "User ");
            strcat(User,username);
            strcat(User, " has joined");

            int j = 0;
            int len;

            while(j < 255){
              len = strlen(User);
              send(PClients[j].sdO, &len, sizeof(uint16_t), MSG_NOSIGNAL);
              sendWrapper(send(PClients[j].sdO, User, len, MSG_NOSIGNAL), &PClients[j]);
              j++;
            }
          }
          send(tempPClient.sd, &validName, sizeof(char), 0);
        }
        else{
          /* Invalid username, closes the participant. */
          validName = 'I';
          close(tempPClient.sd);
        }
      }
    }

    /* Listens for connection for observer port. Almost the same as participant
       connection but checks to make sure the username is a valid username and
       also does not have an observer connected to it already. */
    if(select(sd2+1,&readfd2, NULL, NULL, &timer) > 0 ){
      tempOClient.sd = accept(sd2, (struct sockaddr *)&cad, &alen);
      if(tempOClient.sd != EWOULDBLOCK && tempOClient.sd < 0){
        fprintf(stderr, "Error: Accept failed\n");
        exit(EXIT_FAILURE);
      }

      printf("observer connected \n");
      if(oCount == 255){
	serverFull = 'N';
	send(tempOClient.sd,&serverFull,sizeof(char),MSG_NOSIGNAL);
      }
      else{
	serverFull = 'Y';
	send(tempOClient.sd,&serverFull,sizeof(char),MSG_NOSIGNAL);
      }

      /* Getting name length and name from user client and checking for
	 validity */
      validName = 'T';
      while(validName == 'T'){
	recv(tempOClient.sd, &nameLen, sizeof(uint8_t), MSG_WAITALL);
	char username[nameLen];
	recv(tempOClient.sd, username, nameLen, MSG_WAITALL);
	username[nameLen] = '\0';

	int i = 0;
	int bool = 0;

	/* Makes sure that username exists and is not already claimed
	   by another observer. */
	while(i < 255 && bool == 0){
	  if(PClients[i].sd >= 0){
	    if(strcmp(PClients[i].username, username) == 0){
	      if(PClients[i].sdO < 0){
		PClients[i].sdO = tempOClient.sd;
		validName = 'Y';
		oCount++;
		bool = 1;
	      }
	      else{
		validName = 'T';
		bool = 1;
	      }
	    }
	  }
	  i++;
	}

	//check valid
	if(i == 255){
	  validName = 'N';
	}
	send(tempOClient.sd, &validName, sizeof(char), MSG_NOSIGNAL);
      }
    }

    /* Main server loop that recieves messages */
    int i = 0;
    int sendRet;

    /* Selects each participant socket descriptor. If return value > 0 then the
       participant is trying to send message.  */
    while(i < 255){
      fd_set partfd;
      if(PClients[i].sd >= 0){
	FD_SET(PClients[i].sd, &partfd);

	struct timeval zeroTimer;
	zeroTimer.tv_sec = 0;
	zeroTimer.tv_usec = 0;

	/* Checks to see if any participant wants to send message */
	int ret = select(PClients[i].sd+1, &partfd, NULL, NULL, &zeroTimer);
	if(ret > 0){
	  char tempUsername[10];
	  strcpy(tempUsername, PClients[i].username);
          int p = recv(PClients[i].sd, &PClients[i].message.length, sizeof(uint16_t), MSG_WAITALL);

          /* If message length greater than 1000, disconnects participant and connected observer. */
          if(PClients[i].message.length > 1000){
            recvWrapper(-1, &PClients[i], &partfd);
          }

          p = recvWrapper(recv(PClients[i].sd, PClients[i].message.message, PClients[i].message.length, MSG_WAITALL), &PClients[i], &partfd);

          /* p value checks to see if participant has disconnected. If so, sends out to
             user has left message to everyone. */
          if(p == -1){
            strcpy(User, "User ");
            strcat(User,tempUsername);
            strcat(User, " has left");

            int j = 0;
            int len;

            /* Blasts user has left message out to all observers */
            while(j < 255){
              len = strlen(User);

              send(PClients[j].sdO, &len, sizeof(uint16_t), MSG_NOSIGNAL);
              sendWrapper(send(PClients[j].sdO, User, len, MSG_NOSIGNAL), &PClients[j]);
              j++;
            }
          }
          else if(p == 0){
            PClients[i].message.message[PClients[i].message.length] = '\0';

	    /* Check if private or public and valid username */
	    int kstart = 0;
	    int privateInd = isPrivateMessage(PClients[i].message.message, PClients);
	    if(privateInd >= 0){
              while(PClients[i].message.message[kstart] != ' ' && kstart < 1000){
                kstart++;
              }
              kstart++;
	    }

	    /* if privateInd >= 0, private message. if = -1, public message. if private
	       message, it returns the index in the PClients array of the intended
	       recipient. */
	    if(privateInd != -2){

	      /* Created the private message with leading '>' followed by spaces and username. */
	      int numSpaces = 11 - strlen(PClients[i].username);
	      int s = 0;
	      if(kstart){
		bufWithName[s] = '%';
	      }
	      else{
		bufWithName[s] = '>';
	      }
	      s++;
	      int k = 0;
	      while(s <= numSpaces){
		bufWithName[s] = ' ';
		s++;
	      }
	      while(PClients[i].username[k] != '\0'){
		bufWithName[s] = PClients[i].username[k];
		s++;
		k++;
	      }

	      bufWithName[s] = ':';
	      s++;
	      bufWithName[s] = ' ';
	      s++;

	      k = kstart;
	      while(PClients[i].message.message[k] != '\0'){
		bufWithName[s] = PClients[i].message.message[k];
		s++;
		k++;
	      }
	      bufWithName[s] = '\0';

	      //check validation
	      if(privateInd >= 0){

		/* Sending private message to intended recipient */
		int len = strlen(bufWithName);
		if(i != privateInd){
		  send(PClients[privateInd].sdO, &len, sizeof(uint16_t),MSG_NOSIGNAL);
		  sendRet = send(PClients[privateInd].sdO, bufWithName, len,MSG_NOSIGNAL);
		  sendWrapper(sendRet, &PClients[privateInd]);

		  send(PClients[i].sdO, &len, sizeof(uint16_t),MSG_NOSIGNAL);
		  sendRet = send(PClients[i].sdO, bufWithName, len,MSG_NOSIGNAL);
		  sendWrapper(sendRet, &PClients[i]);
		}
		else{
		  send(PClients[i].sdO, &len, sizeof(uint16_t),MSG_NOSIGNAL);
		  sendRet = send(PClients[i].sdO, bufWithName, len, MSG_NOSIGNAL);
		  sendWrapper(sendRet, &PClients[i]);
		}
	      }
	      else{
		int j = 0;
		int len;

		/* Blast out public message to all observers */
		while(j < 255){
		  len = strlen(bufWithName);

		  send(PClients[j].sdO, &len, sizeof(uint16_t), MSG_NOSIGNAL);
		  sendRet = send(PClients[j].sdO, bufWithName, len, MSG_NOSIGNAL);
		  sendWrapper(sendRet, &PClients[j]);
		  j++;
		}
	      }
	    }
	    else{
	      /* Creates and sends warning message if private message recipient is
		 not valid. */
              char warning[1000];
              char tempuser[10];

              int j  = 1;
              while(PClients[i].message.message[j] != ' ' && j <= 11){
                tempuser[j-1] = PClients[i].message.message[j];
                j++;
              }
              strcpy(warning, "Warning: user ");
              strcat(warning,tempuser);
              strcat(warning, " doesn’t exist");

              k = strlen(warning);
              send(PClients[i].sdO, &k, sizeof(uint16_t),MSG_NOSIGNAL);
              sendRet = send(PClients[i].sdO, warning, k, MSG_NOSIGNAL);
              sendWrapper(sendRet, &PClients[i]);

              j = 0;
              while(j < 10){
                tempuser[j] = '\0';
                j++;
              }
	    }
	  }
	}
      }
      i++;
    }
  }
}

/* Checks to see if participant has disconnected. Returns -1 for yes, 0 for no.
   If disconnection detected, close participant and connected observer. Also
   clears out username, so next client can reuse it if intended. */
int recvWrapper(int recvRet, struct client* pClient, fd_set* partSet){
  if(recvRet <= 0){
    printf("participant disconnecting\n");
    FD_CLR(pClient->sd, partSet);

    if(pClient->sdO > -1){
      oCount--;
    }

    close(pClient->sd);
    close(pClient->sdO);
    pClient->username[0] = '\0';
    pClient->sd = -1;
    pClient->sdO = -1;
    pCount--;

    return -1;
  }
  return 0;
}

/* Checks to see if observer has disconnected. If disconnection detected, close observer. */
void sendWrapper(int sendRet, struct client* pClient){
  if(sendRet < 0 && pClient->sdO > 0){
    close(pClient->sdO);
    pClient->sdO = -1;
    oCount--;
  }
}

/* Checks valid characters in intended username. Returns 0 for bad username,
   1 for valid username. */
int checkNameValidity(char* username){
  int i = 0;
  while(i < strlen(username)){
    if((username[i] <= 'Z' && username[i] >= 'A') ||
       (username[i] <= 'z' && username[i] >= 'a') ||
       (username[i] <= '9' && username[i] >= '0') ||
       username[i] == '_'){
      i++;
    }
    else{
      return 0;
    }
  }
  return 1;
}

/* Traverses through linked list structure and adds a new client node to the end */
void appendList(struct client* cList, struct client* newClient){
  int i = 0;
  while(cList[i].username[0] != '\0' && i < 255){
    i++;
  }
  if(i < 255){
    newClient->sdO = -1;
    cList[i] = *newClient;
  }
}

/* Traverse through client array and checks if username is already taken. Returns 1 for taken,
   0 for not taken.  */
int checkNameTaken(struct client* cList, char* username){
  int i = 0;
  while(i < 255){
    if(cList[i].username[0] != '\0'){
      if(strcmp(username, cList[i].username) == 0){
        return 1;
      }
    }
    i++;
  }
  return 0;
}

/* Checks if message is private. Then checks if username exists. Returns -2 for Invalid
   message, -1 for not private, or some the index in the client array for the wanted
   recipient. */
int isPrivateMessage(char* message, struct client* PClient){
  char username[10];

  if(message[0] == '@' && (message[1] == '\0' || message[1] == ' ')){
    return -2;
  }
  if(message[0] == '@'){
    int i = 1;
    while(message[i] != ' ' && message[i] != '\0' && message[i] != '\n'){
      username[i-1] = message[i];
      i++;
    }
    if(i > 11){
      return -2;
    }
    username[i-1] = '\0';
    i = 0;
    while(i < 255){
      if(strcmp(username, PClient[i].username) == 0){
        return i;
      }
      i++;
    }
    return -2;
  }
  return -1;
}
