/* 
 * udpserver.c - A simple UDP echo server 

 */
#include <sys/time.h>
#include <time.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//#define BUFSIZE 1024

#define BUFSIZE 64

clock_t start, end;
double cpu_time_used;
/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  //char buf[BUFSIZE]; /* message buf */
  //char *buf[BUFSIZE]; /* message buf */
  char *buf; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */


  portno = 10000;

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, 8);

  //  gettimeofday(&tv1, NULL);
  //  start = clock();
    n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &clientaddr, &clientlen);
	 // printf("server received %zu/%d bytes: %02X\n", sizeof(buf), n, *buf);
	  printf("\nbuffer value: 0x%04X, %d", *buf, sizeof(buf));
   // end = clock();
    if (n < 0)
      error("ERROR in recvfrom");


  //  cpu_time_used = (((double) (end - start)) / CLOCKS_PER_SEC)*1000;
  //  printf("cpu_time_used: %f ms \n", cpu_time_used);    


	
	
//    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
    
    
  }
}
