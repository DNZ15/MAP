/*
 *  This extra small demo sends a random samples to your speakers.
 */

#include <alsa/asoundlib.h>

#include <unistd.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static char *device = "hw:0,0";		/* playback device */
snd_output_t *output = NULL;
unsigned char buffer[2048];				/* some random data */
//char buffer[2048];				/* some random data */
//char *buffer;	

int sockfd; /* socket */
int portno = 10000; /* port to listen on */
int clientlen; /* byte size of client's address */
struct sockaddr_in serveraddr; /* server's addr */
struct sockaddr_in clientaddr; /* client addr */
struct hostent *hostp; /* client host info */
char *hostaddrp; /* dotted decimal host addr string */
int optval; /* flag value for setsockopt */
int n; /* message byte size */

//buffer = malloc(8192); //(((2048*16)/8)*2) = 8192
/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(void)
{
        int err;
        unsigned int i;
        snd_pcm_t *handle;
        snd_pcm_sframes_t frames;
		int counter=0;
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

	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	if ((err = snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2,
	                              44100, 1, 0)) < 0) {	/* 0.5sec */
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	
	
	 
	
	
        while(1){
		/*	
		if(n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &clientaddr, &clientlen)<0)
		{
			printf("\n n =%d \n", n);	
		}	*/
		
		n = recvfrom(sockfd, buffer, 128, 0, (struct sockaddr *) &clientaddr, &clientlen);
		//printf("\n n =%d \n", n);	
		
		
		//else {
			    counter = counter+1;
				printf("\n counter =%d \n", counter);	
                frames = snd_pcm_writei(handle, buffer, 128);
                if (frames < 0)
                        frames = snd_pcm_recover(handle, frames, 0);
                if (frames < 0) {
                        printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
                        break;
                }
                if (frames > 0 && frames < (long)sizeof(buffer))
                        printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buffer), frames);
       
			   // printf("\nbuffer value: 0x%04X, %d", *buffer, sizeof(buffer));
		//}
	   }

	snd_pcm_close(handle);
	return 0;
}