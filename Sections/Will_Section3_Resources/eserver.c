/* A simple server TCP echo with port number is passed as an argument */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

void error(char *msg)
{
    perror(msg);
    exit(1);
}


#define MAXREQ  256
#define MAXQUEUE 5

void server(int consockfd) {
  char reqbuf[MAXREQ];
  int n;
  while (1) {			/* echo stream on this connection */
    memset(reqbuf,0, MAXREQ);
    n = read(consockfd,reqbuf,MAXREQ-1); /* Read request from the socket */
    if (n <= 0) return;
    n = write(STDOUT_FILENO, reqbuf, strlen(reqbuf)); /* print it */
    n = write(consockfd, reqbuf, strlen(reqbuf)); /* Reply to client */
    if (n < 0) error("ERROR writing to socket");
  }
}

int startserver(int portno)
{
  int sockfd, newsockfd;
  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;
  uint clilen = sizeof(cli_addr);

  /* Create Socket to receive requests*/
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) error("ERROR opening socket");

  /* Bind socket to port */
  memset((char *) &serv_addr,0,sizeof(serv_addr));
  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port        = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    error("ERROR on binding");

  while (1) {
    /* Listen for incoming connections */
    listen(sockfd,MAXQUEUE);
    printf("new connection\n");
    /* Accept incoming connection, obtaining a new socket for it */
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) error("ERROR on accept");
    printf("connection socket: %d\n", newsockfd);    

    server(newsockfd);
    close(newsockfd); 
  }
  printf("Server exiting\n");
  close(sockfd);
  return 0; 
}



int main(int argc, char *argv[])
{
  int portno;
  if (argc < 2) {
    fprintf(stderr,"ERROR, no port provided\n");
    exit(1);
  }
  portno = atoi(argv[1]);
  printf("Opening server on port %d\n",portno);
  return startserver(portno);
}
