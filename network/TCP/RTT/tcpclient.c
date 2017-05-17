/* 
 * tcpclient.c - A simple TCP client
 * usage: tcpclient <host> <port>
 */
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

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
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
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
    serveraddr.sin_port = htons(portno);
    start = clock();

    /* connect: create a connection with the server */
//    if (connect(sockfd, &serveraddr, sizeof(serveraddr)) < 0) 
    if (connect(sockfd,(struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");

    /* get message line from the user */
    //printf("Please enter msg: ");
    bzero(buf, BUFSIZE);
    strcpy(buf, "hallojlkjjdfhkjfgjhfgdteksngfhtfhfhjfdgfdgdfgfdgdfgdfgdfgdfgtrhgrtezfzfsdfsdfsdfsdfgsdfssdfsdfsdfsdfjkhfdssfsdyhfysfghsdgfjhfgezjkyhfgzekjhfekhjfrekjfhdskfjhkjvdfjkvhdfkjfhdkjfhdsfyzeukfghyzerikfgdsfhkgsdkhfkdhflkjzgfhizelkgfzeigfkuzehvffdhgv;dfchjvlkdfjhvfkljdhfjsdhfkjsdhfkjsdhflkjdsghfliregfkuzerygfkjzehferhilfgrehyiolgvfdkjhg,jhvjfd,bvh;jxc,bvxhjcvghlisdhfglomifhezlifgzekyfhgzejhkyfhkzejgfhkzejgfhkzreifgkjuhfgdhsufydjshykfjsdhgfhkzejhgfhkizeugfkezgfzekuhfgezkjhfgzekhfgzekjhfgzejkfyhgjzehfgkjzehyfkzeyhfikuzegfhkuzegfkuezdskfsdnvbnvbrfhfdskhjfdkuhcioydfsyihfsdkjhrzeuyikrzesdfyiufezdskiuyzfgekkgfjghfdjhgjfhgkfjhgfjhgdfjhgfdjghfdjkhgfdjghfdjkghdfgjhfdgkjhgjdfhgdffhjkfsdjhkfsdkjhsdfkjhfsdkjhfsdkjhsdfjhksdfkjhsdfkjhfsdkjhfsdkjhfsdkjhsdfkjhsdfiuysdfutyidfstyiuarzejhrzeahgdsfqkrzehgfukdshfulgkhgfkhcjfdehctvezjgyckvezgjyckfyrfyjyhfhgfezhfghkjgfkjezhrkjehrilzeiruezpruzelkjfgdekfgkerjyhlreigtydskugdsbfkjsdhkjzehdsudfhjkdhksdkjhdfsgiukysdfiuysdftyiusdturfdrcgfestznb,qdf;dr;kfqj,hbntiflkjreofhizrehkjoirhreoioihfghfghgd");
//    fgets(buf, BUFSIZE, stdin);
    /* send the message line to the server */
    n = write(sockfd, buf, strlen(buf));
    if (n < 0) 
      error("ERROR writing to socket");

    /* print the server's reply */
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, BUFSIZE);
    if (n < 0) 
      error("ERROR reading from socket");
    printf("\nEcho from server: %s", buf);
    close(sockfd);

    end = clock();
    cpu_time_used = (((double) (end - start)) / CLOCKS_PER_SEC)*1000;
    printf("\ncpu_time_used (RTT): %f ms \n", cpu_time_used);

    return 0;
}
