#ifndef prog3_server
#define prog3_server

struct clientMessage{
  char message[1000];
  int length;
};

struct client{
  struct clientMessage message;
  char username[10];
  int sd;
  int sdO; /* Connected observer socket descriptor */
};

int checkNameValidity(char* username);
int checkNameTaken(struct client* cList, char* username);
void appendList(struct client* cList, struct client* newClient);
int isPrivateMessage(char* message, struct client* PClient);
void sendWrapper(int sendRet, struct client* pClient);
int recvWrapper(int recvRet, struct client* pClient, fd_set* partSet);

#endif
