#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>

/* needed for UDP*/
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//#define BUFSIZE 8192
clock_t start, end;
double cpu_time_used;

unsigned char buf2[12];

char *pdevice = "hw:0,0";
char *cdevice = "hw:0,0";
snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
int rate = 48000;
int channels = 2;
int latency_min = 32;		/* in frames / 2 */
int latency_max = 2048;		/* in frames / 2 */
int loop_sec = 10;		/* seconds */
int buffer_size, period_size, block, use_poll = 0;			/* auto, auto, block mode, '' */
int resample = 1;
unsigned long loop_limit;

snd_output_t *output = NULL;




/* 
 * error - wrapper for perror for UDP
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}


/* set configuration */
int setparams_stream(snd_pcm_t *handle, snd_pcm_hw_params_t *params, const char *id)
{
	int err;
	unsigned int rrate;

	/* request valid settings */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		printf("Broken configuration for %s PCM: no configurations available: %s\n", snd_strerror(err), id);
		return err;
	}

	/* set resample usage */
	err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
	if (err < 0) {
		printf("Resample setup failed for %s (val %i): %s\n", id, resample, snd_strerror(err));
		return err;
	}

	/* set interleaving (left/right) */
	err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		printf("Access type not available for %s: %s\n", id, snd_strerror(err));
		return err;
	}

	/* set format (16S_LE) */
	err = snd_pcm_hw_params_set_format(handle, params, format);
	if (err < 0) {
		printf("Sample format not available for %s: %s\n", id, snd_strerror(err));
		return err;
	}

	/* set number of channels */
	err = snd_pcm_hw_params_set_channels(handle, params, channels);
	if (err < 0) {
		printf("Channels count (%i) not available for %s: %s\n", channels, id, snd_strerror(err));
		return err;
	}

	/* set a rate close to what we requested */
	rrate = rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
	if (err < 0) {
		printf("Rate %iHz not available for %s: %s\n", rate, id, snd_strerror(err));
		return err;
	}
	if ((int)rrate != rate) {
		printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
		return -EINVAL;
	}
	return 0;
}

int setparams_bufsize(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_hw_params_t *tparams, snd_pcm_uframes_t bufsize, const char *id)
{
	int err;
	snd_pcm_uframes_t periodsize;

	/* copy our parameters */
	snd_pcm_hw_params_copy(params, tparams);
	periodsize = bufsize * 2;

	/* set buffer size, hoping to get a result close to what we request */
	err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &periodsize);
	if (err < 0) {
		printf("Unable to set buffer size %li for %s: %s\n", bufsize * 2, id, snd_strerror(err));
		return err;
	}
	if (period_size > 0)
		periodsize = period_size;
	else
		periodsize /= 2;

	/* set a period size, hoping to get a result close to what we request */
	err = snd_pcm_hw_params_set_period_size_near(handle, params, &periodsize, 0);
	if (err < 0) {
		printf("Unable to set period size %li for %s: %s\n", periodsize, id, snd_strerror(err));
		return err;
	}
	return 0;
}

int setparams_set(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_sw_params_t *swparams, const char *id)
{
	int err;
	snd_pcm_uframes_t val;

	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		printf("Unable to set hw params for %s: %s\n", id, snd_strerror(err));
		return err;
	}
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		printf("Unable to determine current swparams for %s: %s\n", id, snd_strerror(err));
		return err;
	}
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 0x7fffffff);
	if (err < 0) {
		printf("Unable to set start threshold mode for %s: %s\n", id, snd_strerror(err));
		return err;
	}
	if (!block)
		val = 4;
	else
		snd_pcm_hw_params_get_period_size(params, &val, NULL);
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, val);
	if (err < 0) {
		printf("Unable to set avail min for %s: %s\n", id, snd_strerror(err));
		return err;
	}
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) {
		printf("Unable to set sw params for %s: %s\n", id, snd_strerror(err));
		return err;
	}
	return 0;
}

int setparams(snd_pcm_t *phandle, snd_pcm_t *chandle, int *bufsize)
{
	int err, last_bufsize = *bufsize;
	snd_pcm_hw_params_t *pt_params, *ct_params;	/* templates with rate, format and channels */
	snd_pcm_hw_params_t *p_params, *c_params;
	snd_pcm_sw_params_t *p_swparams, *c_swparams;
	snd_pcm_uframes_t p_size, c_size, p_psize, c_psize;
	unsigned int p_time, c_time;
	unsigned int val;

	/* allocate memory to store configuration */
	snd_pcm_hw_params_alloca(&p_params);
	snd_pcm_hw_params_alloca(&c_params);
	snd_pcm_hw_params_alloca(&pt_params);
	snd_pcm_hw_params_alloca(&ct_params);
	snd_pcm_sw_params_alloca(&p_swparams);
	snd_pcm_sw_params_alloca(&c_swparams);

	/* set configuration for playback */
	if ((err = setparams_stream(phandle, pt_params, "playback")) < 0) {
		printf("Unable to set parameters for playback stream: %s\n", snd_strerror(err));
		exit(0);
	}

	/* set configuration for capture */
	if ((err = setparams_stream(chandle, ct_params, "capture")) < 0) {
		printf("Unable to set parameters for playback stream: %s\n", snd_strerror(err));
		exit(0);
	}

	if (buffer_size > 0) {
		*bufsize = buffer_size;
		goto __set_it;
	}

 __again:
	if (buffer_size > 0)
		return -1;
	if (last_bufsize == *bufsize)
		*bufsize += 4;
	last_bufsize = *bufsize;
	if (*bufsize > latency_max)
		return -1;

 __set_it:

	/* set buffer size */
	if ((err = setparams_bufsize(phandle, p_params, pt_params, *bufsize, "playback")) < 0) {
		printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
		exit(0);
	}
	if ((err = setparams_bufsize(chandle, c_params, ct_params, *bufsize, "capture")) < 0) {
		printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
		exit(0);
	}

	/* check period size - increase buffer size if necessary */
	snd_pcm_hw_params_get_period_size(p_params, &p_psize, NULL);
	if (p_psize > (unsigned int)*bufsize)
		*bufsize = p_psize;
	snd_pcm_hw_params_get_period_size(c_params, &c_psize, NULL);
	if (c_psize > (unsigned int)*bufsize)
		*bufsize = c_psize;

	/* get times related to our buffer sizes */
	snd_pcm_hw_params_get_period_time(p_params, &p_time, NULL);
	snd_pcm_hw_params_get_period_time(c_params, &c_time, NULL);
	if (p_time != c_time)
		goto __again;

	/* request buffersizes */
	snd_pcm_hw_params_get_buffer_size(p_params, &p_size);
	if (p_psize * 2 < p_size) {
                snd_pcm_hw_params_get_periods_min(p_params, &val, NULL);
                if (val > 2) {
			printf("playback device does not support 2 periods per buffer\n");
			exit(0);
		}
		goto __again;
	}
	snd_pcm_hw_params_get_buffer_size(c_params, &c_size);
	if (c_psize * 2 < c_size) {
                snd_pcm_hw_params_get_periods_min(c_params, &val, NULL);
		if (val > 2 ) {
			printf("capture device does not support 2 periods per buffer\n");
			exit(0);
		}
		goto __again;
	}

	if ((err = setparams_set(phandle, p_params, p_swparams, "playback")) < 0) {
		printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
		exit(0);
	}

	if ((err = setparams_set(chandle, c_params, c_swparams, "capture")) < 0) {
		printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
		exit(0);
	}

	/* prepare playback device */
	if ((err = snd_pcm_prepare(phandle)) < 0) {
		printf("Prepare error: %s\n", snd_strerror(err));
		exit(0);
	}

	/* make sure all debug info has been written */
	//snd_pcm_dump(phandle, output);
	//snd_pcm_dump(chandle, output);
	fflush(stdout);
	return 0;
}

void showstat(snd_pcm_t *handle, size_t frames)
{
	int err;
	snd_pcm_status_t *status;

	snd_pcm_status_alloca(&status);
	if ((err = snd_pcm_status(handle, status)) < 0) {
		printf("Stream status error: %s\n", snd_strerror(err));
		exit(0);
	}
	printf("*** frames = %li ***\n", (long)frames);
	snd_pcm_status_dump(status, output);
}

void showlatency(size_t latency)
{
	double d;
	latency *= 2;
	d = (double)latency / (double)rate;
	//printf("Trying latency %li frames, %.3fus, %.6fms (%.4fHz)\n", (long)latency, d * 1000000, d * 1000, (double)1 / d);
}

void showinmax(size_t in_max)
{
	double d;

	printf("Maximum read: %li frames\n", (long)in_max);
	d = (double)in_max / (double)rate;
	printf("Maximum read latency: %.3fus, %.6fms (%.4fHz)\n", d * 1000000, d * 1000, (double)1 / d);
}

void gettimestamp(snd_pcm_t *handle, snd_timestamp_t *timestamp)
{
	int err;
	snd_pcm_status_t *status;

	snd_pcm_status_alloca(&status);
	if ((err = snd_pcm_status(handle, status)) < 0) {
		printf("Stream status error: %s\n", snd_strerror(err));
		exit(0);
	}
	snd_pcm_status_get_trigger_tstamp(status, timestamp);
}

void setscheduler(void)
{
	struct sched_param sched_param;

	if (sched_getparam(0, &sched_param) < 0) {
		printf("Scheduler getparam failed...\n");
		return;
	}
	sched_param.sched_priority = sched_get_priority_max(SCHED_RR);
	if (!sched_setscheduler(0, SCHED_RR, &sched_param)) {
		printf("Scheduler set to Round Robin with priority %i...\n", sched_param.sched_priority);
		fflush(stdout);
		return;
	}
	printf("!!!Scheduler set to Round Robin with priority %i FAILED!!!\n", sched_param.sched_priority);
}

long timediff(snd_timestamp_t t1, snd_timestamp_t t2)
{
	signed long l;

	t1.tv_sec -= t2.tv_sec;
	l = (signed long) t1.tv_usec - (signed long) t2.tv_usec;
	if (l < 0) {
		t1.tv_sec--;
		l = 1000000 + l;
		l %= 1000000;
	}
	return (t1.tv_sec * 1000000) + l;
}

long readbuf(snd_pcm_t *handle, char *buf, long len, size_t *frames, size_t *max)
{
	long r;

	if (!block) {
		do {
			strncpy(buf, buf2, 12);
			r = snd_pcm_readi(handle, buf, len);
		} while (r == -EAGAIN);
		if (r > 0) {
			*frames += r;
			if ((long)*max < r)
				*max = r;
		}
		// printf("read = %li\n", r);
	} else {
		int frame_bytes = (snd_pcm_format_width(format) / 8) * channels;
		do {
			r = snd_pcm_readi(handle, buf, len);
			if (r > 0) {
				buf += r * frame_bytes;
				len -= r;
				*frames += r;
				if ((long)*max < r)
					*max = r;
			}
			// printf("r = %li, len = %li\n", r, len);
		} while (r >= 1 && len > 0);
	}
	// showstat(handle, 0);
	return r;
}

long writebuf(snd_pcm_t *handle, char *buf, long len, size_t *frames)
{
	long r;

	while (len > 0) {
		r = snd_pcm_writei(handle, buf, len);
		if (r == -EAGAIN)
			continue;
		// printf("write = %li\n", r);
		if (r < 0)
			return r;
		// showstat(handle, 0);
		buf += r * 4;
		len -= r;
		*frames += r;
	}
	return 0;
}

void help(void)
{
	int k;
	printf(
"Usage: latency [OPTION]... [FILE]...\n"
"-h,--help      help\n"
"-m,--min       minimum latency in frames\n"
"-M,--max       maximum latency in frames\n"
"-F,--frames    frames to transfer\n"
"-B,--buffer    buffer size in frames\n"
"-E,--period    period size in frames\n"
"-s,--seconds   duration of test in seconds\n"
"-b,--block     block mode\n"
"-p,--poll      use poll (wait for event - reduces CPU usage)\n"
);
        printf("Recognized sample formats are:");
        for (k = 0; k < SND_PCM_FORMAT_LAST; ++k) {
                const char *s = snd_pcm_format_name(k);
                if (s)
                        printf(" %s", s);
        }
        printf("\n\n");
        printf(
"Tip #1 (usable latency with large periods, non-blocking mode, good CPU usage,\n"
"        superb xrun prevention):\n"
"  latency -m 8192 -M 8192 -t 1 -p\n"
"Tip #2 (superb latency, non-blocking mode, but heavy CPU usage):\n"
"  latency -m 128 -M 128\n"
);
}

int main(int argc, char *argv[])
{
	struct option long_option[] =
	{
		{"help", 0, NULL, 'h'},		
		{"min", 1, NULL, 'm'},
		{"max", 1, NULL, 'M'},		
		{"frames", 1, NULL, 'F'},
		{"buffer", 1, NULL, 'B'},
		{"period", 1, NULL, 'E'},
		{"seconds", 1, NULL, 's'},
		{"block", 0, NULL, 'b'},
		{"poll", 0, NULL, 'p'},
		{NULL, 0, NULL, 0},
	};
	snd_pcm_t *phandle, *chandle;
	char *buffer;
	int err, latency, morehelp;
	int ok;
	snd_timestamp_t p_tstamp, c_tstamp;
	ssize_t r;
	size_t frames_in, frames_out, in_max;
	morehelp = 0;
	
		/*UDP*/
	int sockfd, portno, n, serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
	//char buf[BUFSIZE];
   // char *bufx[BUFSIZE];

    //hostname = "192.168.10.11";
	hostname = "192.168.10.11";
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
	
	
	/* writes all zeroes, empty it */
  //  bzero(bufx, BUFSIZE);
	
	/* send the message to the server */
	serverlen = sizeof(serveraddr);
	/*UDP*/
	
			/*
	14 bytes Ethernet + 20 bytes IP + 12 bytes RTP  (46 bytes)
	
	PT (dec) 10 = Linear PCM 16-bit Stereo audio 1411.2 kbit/s,[2][3][4] uncompressed
	PT (dec) 11 = Linear PCM 16-bit audio 705.6 kbit/s, uncompressed
	*/
	
    //0x800A0001; // wireshark turns it, so becomes => 0x01000A80; 
	//time_stamp = 0 => buf[4-7] = 0
	//ssrc = 0 => buf[8-11] = 0
/*
	    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
   
//12 bytes
buf2[0] = 1;  // sequence number
buf2[1] = 0;  // sequence number
buf2[2] = 10; // PT + M, M = 0
buf2[3] = 128;//v, p, x, cc , v = 2 (dec), rest is 0
buf2[4] = 0;  // timestamp
buf2[5] = 0;  // timestamp
buf2[6] = 0;  // timestamp
buf2[7] = 0;  // timestamp
buf2[8] = 0;  //ssrc
buf2[9] = 0;  //ssrc
buf2[10] = 0; //ssrc
buf2[11] = 0; //ssrc
	
	while (1) {
		int c;
		if ((c = getopt_long(argc, argv, "hP:C:m:M:F:f:c:r:B:E:s:bpn", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			morehelp++;
			break;
		case 'm':
			err = atoi(optarg) / 2;
			latency_min = err >= 4 ? err : 4;
			if (latency_max < latency_min)
				latency_max = latency_min;
			break;
		case 'M':
			err = atoi(optarg) / 2;
			latency_max = latency_min > err ? latency_min : err;
			break;
		case 'B':
			err = atoi(optarg);
			buffer_size = err >= 32 && err < 200000 ? err : 0;
			break;
		case 'E':
			err = atoi(optarg);
			period_size = err >= 32 && err < 200000 ? err : 0;
			break;
		case 's':
			err = atoi(optarg);
			loop_sec = err >= 1 && err <= 100000 ? err : 30;
			break;
		case 'b':
			block = 1;
			break;
		case 'p':
			use_poll = 1;
			break;
		case 'n':
			resample = 0;
			break;
		}
	}

	if (morehelp) {
		help();
		return 0;
	}

	/* put debug info onto stdout */
	err = snd_output_stdio_attach(&output, stdout, 0);
	if (err < 0) {
		printf("Output failed: %s\n", snd_strerror(err));
		return 0;
	}

	/* calculations */
	loop_limit = loop_sec * rate;
	latency = latency_min - 4;

	/* allocate memory */
	buffer = malloc((latency_max * snd_pcm_format_width(format) / 8) * 2); //(2048*4) = 8192
	
	
	/* ask for higher priority */
	setscheduler();

	/* list some info */
	//printf("Playback device is %s\n", pdevice);
	printf("Capture device is %s\n", cdevice);
	printf("Parameters are %iHz, %s, %i channels, %s mode\n", rate, snd_pcm_format_name(format), channels, block ? "blocking" : "non-blocking");
	printf("Poll mode: %s\n", use_poll ? "yes" : "no");
	printf("Loop limit is %li frames, minimum latency = %i, maximum latency = %i\n", loop_limit, latency_min * 2, latency_max * 2);

	/* open playback device */
	if ((err = snd_pcm_open(&phandle, pdevice, SND_PCM_STREAM_PLAYBACK, block ? 0 : SND_PCM_NONBLOCK)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		return 0;
	}

	/* open capture device */
	if ((err = snd_pcm_open(&chandle, cdevice, SND_PCM_STREAM_CAPTURE, block ? 0 : SND_PCM_NONBLOCK)) < 0) {
		printf("Record open error: %s\n", snd_strerror(err));
		return 0;
	}

	while(1) {
		/* reset frame counters */
		frames_in = frames_out = 0;

		/* latency is passed and used as buffersize */
		if (setparams(phandle, chandle, &latency) < 0)
			break;

		/* use bufsize to calculate / show latency */
		showlatency(latency);

		/* link capture / playback handle - make sure they start/stop together */
		if ((err = snd_pcm_link(chandle, phandle)) < 0) {
			printf("Streams link error: %s\n", snd_strerror(err));
			exit(0);
		}

		/* clean our buffer */
		if (snd_pcm_format_set_silence(format, buffer, latency*channels) < 0) {
			fprintf(stderr, "silence error\n");
			break;
		}

		/* write this silence to the buffer twice */
		if (writebuf(phandle, buffer, latency, &frames_out) < 0) {
			fprintf(stderr, "write error\n");
			break;
		}
		if (writebuf(phandle, buffer, latency, &frames_out) < 0) {
			fprintf(stderr, "write error\n");
			break;
		}

		/* start capture - and, since it's linked - playback */
		if ((err = snd_pcm_start(chandle)) < 0) {
			printf("Go error: %s\n", snd_strerror(err));
			exit(0);
		}

		/* get timestamp when the devices started */
		gettimestamp(phandle, &p_tstamp);
		gettimestamp(chandle, &c_tstamp);
#if 0
	//	printf("Playback:\n");
	//	showstat(phandle, frames_out);
		//printf("Capture:\n");
	//	showstat(chandle, frames_in);
#endif





		ok = 1;
		in_max = 0;
		int my_counter = 0;
	//	start = clock();
		while (ok && frames_in < loop_limit) {
			if (use_poll) {
				/* use poll to wait for next event */
				snd_pcm_wait(chandle, 1000);
			}
			if ((r = readbuf(chandle, buffer, latency, &frames_in, &in_max)) < 0)
			{
					ok = 0;		
			}
			
			else {
			      printf("\n r= %d \n",r);
			      if (writebuf(phandle, buffer, r, &frames_out) < 0)
				  {
					ok = 0;
				  }
				  
				  if (sendto(sockfd, buffer, r, 0, (struct sockaddr*)&serveraddr, serverlen) < 0) 
					{
						 error("ERROR in sendto");
						 ok = 0;
						 
					}
					my_counter = my_counter+1;
				    printf("\nmy_counter = %d \n",my_counter);
					printf("\nbuffer value: 0x%04X, %d", *buffer, sizeof(buffer));
				   
					//memcpy(buf, &frames_out, 4);
					//memcpy(buf, buffer, 4);
					//memcpy(bufx, buffer, 4);	
					//n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&serveraddr, serverlen);
                     
					//if (sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, serverlen) < 0) 
				
					/*
					printf("framesout (dec): %d \n", bufx);
					printf("framesout: 0x%02X \n", bufx);
					printf("strlen(buf): %d \n", strlen(bufx));*/

			 	//if (writebuf(phandle, buffer, r, &frames_out) < 0)
					//ok = 0;
			}
		}

		//end = clock();
		//cpu_time_used = (((double) (end - start)) / CLOCKS_PER_SEC)*1000;
//printf("\ncpu_time_used (RTT): %f ms \n", cpu_time_used);

		
		if (ok)
			printf("Success\n");
		else
			printf("Failure\n");
	//	printf("Playback:\n");
		//showstat(phandle, frames_out);
		//printf("Capture:\n");
		//showstat(chandle, frames_in);
		showinmax(in_max);
		if (p_tstamp.tv_sec == p_tstamp.tv_sec &&
		    p_tstamp.tv_usec == c_tstamp.tv_usec)
			printf("Hardware sync\n");
		snd_pcm_drop(chandle);
		snd_pcm_nonblock(phandle, 0);
		snd_pcm_drain(phandle);
		snd_pcm_nonblock(phandle, !block ? 1 : 0);
		if (ok) {
#if 1
			printf("Playback time = %li.%i, Record time = %li.%i, diff = %li\n",
			       p_tstamp.tv_sec,
			       (int)p_tstamp.tv_usec,
			       c_tstamp.tv_sec,
			       (int)c_tstamp.tv_usec,
			       timediff(p_tstamp, c_tstamp));
#endif
			break;
		}
		snd_pcm_unlink(chandle);
		snd_pcm_hw_free(phandle);
		snd_pcm_hw_free(chandle);
	}
	snd_pcm_close(phandle);
	snd_pcm_close(chandle);
	return 0;
}
