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

//#define BUFSIZE 524
#define BUFSIZE 1036

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

    int sockfd, portno, n, serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    unsigned char buf[BUFSIZE];

	
	// receivers IP
    hostname ="192.168.10.10"; 
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
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno); // portnumber
	
    //errases the buffer space, all zeroes
    bzero(buf, BUFSIZE);
    
	/*
	14 bytes Ethernet
     20 bytes IP
     8 bytes UDP  (42 bytes)
		
	
	First 4 octets
	v = 2 bit
	p = 1 bit
	x = 1 bit
	cc = 4 bit
	m = 1 bit
	pt = 7 bit
	sequence number = 16 bit
	
	PT (dec) 10 = Linear PCM 16-bit Stereo audio 1411.2 kbit/s,[2][3][4] uncompressed
	PT (dec) 11 = Linear PCM 16-bit audio 705.6 kbit/s, uncompressed
	*/
	
    //0x800A0001; // wireshark turns it, so becomes => 0x01000A80;
	//time_stamp = 0 => buf[4-7] = 0
	//ssrc = 0 => buf[8-11] = 0
	
    //12 bytes
	buf[0] = 1;
	buf[1] = 0;
	buf[2] = 10;
	buf[3] = 128;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 0;
	buf[9] = 0;
	buf[10] = 0;
	buf[11] = 0;
	
	int i, a;
	printf("\n Random data:\n");
	srand(time(NULL));
	for (i=12; i < sizeof(buf); i++) {
		buf[i] = ((rand() % 100)+1);
		printf("%u", buf[i]);
	}
	
	/* send the message to the server */
    serverlen = sizeof(serveraddr);

	// RTT measurement calculation start
    start = clock();
	//n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, serverlen);
     n = sendto(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, serverlen);
    //printf("sizeof of buf %lu \n", sizeof(buf));

    if (n < 0) 
      error("ERROR in sendto");
    
    /* print the server's reply */
    //n = recvfrom(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, &serverlen);
	n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, &serverlen);

    if (n < 0) 
      error("ERROR in recvfrom");
  
	printf("\nEcho from server\n");
	for (a=0; a<sizeof(buf); a++)
	{
        printf("%d", buf[a]);
	}
	
	// Stop timing and print the RTT
	end = clock();
	cpu_time_used = (((double) (end - start)) / CLOCKS_PER_SEC)*1000;
	printf("\ncpu_time_used (RTT): %f ms \n", cpu_time_used);
	
    return 0;
}