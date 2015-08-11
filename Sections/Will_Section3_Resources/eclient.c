/* Simple TCP client example */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>

void error(char *msg)
{
  perror(msg);
  exit(1);
}

#define MAXIN  1024
#define MAXOUT 4096

char *getreq(char *inbuf, int len) {
  /* Get request char stream */
  printf("REQ: ");		/* prompt */
  memset(inbuf,0,len);		/* clear for good measure */
  return fgets(inbuf,len,stdin); /* read up to a EOL */
}

void client(int sockfd) {
  int n;
  char sndbuf[MAXIN];
  char rcvbuf[MAXOUT];

  getreq(sndbuf, MAXIN);
  while (strlen(sndbuf) > 0) {

    /* Write char stream to socket */
    if ((n = write(sockfd,sndbuf,strlen(sndbuf))) < 0)
      error("ERROR writing to socket");
      
    /* Read response from the scoket */
    memset(rcvbuf,0,MAXOUT);
    if ((n = read(sockfd,rcvbuf,MAXOUT-1)) < 0)
      error("ERROR reading from socket");

    if ((n = write(STDOUT_FILENO,rcvbuf,n)) < 0)
      error("ERROR writing to stdout");

    /* Get request char stream to send */
    getreq(sndbuf, MAXIN);
  }
}

struct hostent *buildServerAddr(struct sockaddr_in *serv_addr, 
				char *hostname, int portno) {
  struct hostent *server;
  /* Get host entry associated with a hostname or IP address */
  server = gethostbyname(hostname); 
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(1);
  }

  /* Construct an address for remote server */
  memset((char *) serv_addr, 0, sizeof(struct sockaddr_in));
  serv_addr->sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&(serv_addr->sin_addr.s_addr), server->h_length);
  serv_addr->sin_port = htons(portno);
  return server;
}

int main(int argc, char *argv[])
{
  char *hostname;
  int sockfd, portno;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  if (argc < 3) {
    fprintf(stderr,"usage %s hostname port\n", argv[0]);
    exit(0);
  }
  hostname = argv[1];
  portno = atoi(argv[2]);

  server = buildServerAddr(&serv_addr, hostname, portno);

  /* Create a TCP socket */
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    error("ERROR opening socket");

  /* Connect to server on port */
  if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    error("ERROR connecting");

  printf("Connected to %s:%d\n",server->h_name, portno);

  /* Carry out Client-server protocol */
  client(sockfd);		

  /* Clean up on termination */
  close(sockfd);
  return 0;
}
