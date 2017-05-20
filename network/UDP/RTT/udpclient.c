/* 
 * udpclient.c - A simple UDP client
 */

//Added:
#include <time.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

//#define BUFSIZE 5
//#define BUFSIZE 512
#define BUFSIZE 1024

clock_t start, end;
double cpu_time_used;

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {

    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
   // char buf[BUFSIZE];
	unsigned char buf[BUFSIZE];

    hostname = "192.168.10.10";
    portno = 10000;

    /* socket: create the socket */	
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }
	
    
    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

	
    bzero(buf, BUFSIZE);

	int i, a;
	printf("\n Random data:\n");
	srand(time(NULL));
	for (i=0; i < sizeof(buf); i++) {
		buf[i] = ((rand() % 100)+1);
		printf("%u", buf[i]);
	}


   /* send the message to the server */
    serverlen = sizeof(serveraddr);

    start = clock();
//    n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
    n = sendto(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, serverlen);

    if (n < 0) 
      error("ERROR in sendto");
    
    /* print the server's reply */
//    n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
    n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR in recvfrom");
	
	printf("\nEcho from server\n");
	for (a=0; a<sizeof(buf); a++)
	{
        printf("%d", buf[a]);
	}

	end = clock();
	cpu_time_used = (((double) (end - start)) / CLOCKS_PER_SEC)*1000;
	printf("\ncpu_time_used (RTT): %f ms \n", cpu_time_used);
    return 0;
}