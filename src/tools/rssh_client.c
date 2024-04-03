/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino_gw_fwd -- An opensource lora gateway forward 
 *
 * See http://www.dragino.com for more information about
 * the lora gateway project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 *
 * Maintainer: skerlan
 *
 */

/*!
 * \file
 * \brief rssh uliti 
 */

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif

#include <stdint.h>         /* C99 types */
#include <stdbool.h>        /* bool type */
#include <stdio.h>          /* printf, fprintf, snprintf, fopen, fputs */
#include <inttypes.h>       /* PRIx64, PRIu64... */

#include <string.h>         /* memset */
#include <signal.h>         /* sigaction */
#include <time.h>           /* time, clock_gettime, strftime, gmtime */
#include <sys/time.h>       /* timeval */
#include <unistd.h>         /* getopt, access */
#include <stdlib.h>         /* atoi, exit */
#include <errno.h>          /* error messages */
#include<dirent.h>          /* opendir, readdir */
#include <ctype.h>          /* isdigit */

#include <sys/socket.h>     /* socket specific definitions */
#include <netinet/in.h>     /* INET constants and stuff */
#include <arpa/inet.h>      /* IP address conversion stuff */
#include <netdb.h>          /* gai_strerror */

#include <pthread.h>

#include "logger.h"
#include "util_rssh.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */
#define STRINGIFY(x)     #x
#define STR(x)          STRINGIFY(x)

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */
#define PROTOCOL_VERSION    1           /* v1.0 */
#define DEFAULT_PORT_UP     3721
#define DEFAULT_STAT        60          /* 60 seconds */
#define PULL_TIMEOUT_MS     200          
#define DEFAULT_KEEPALIVE   5

#define BUFF_SIZE           32

/* -------------------------------------------------------------------------- */
/* --- PUBLIC DECLARATION ---------------------------------------- */
uint8_t LOG_INFO = 1;
uint8_t LOG_DEBUG = 1;
uint8_t LOG_WARNING = 1;
uint8_t LOG_ERROR = 1;

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

/* signal handling variables */
volatile bool exit_sig = false; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */

/* network configuration variables */
static uint64_t lgwm = 0; /* Lora gateway MAC address */
static char serv_addr[64] = "161.117.181.127"; /* address of the server (host name or IPv4/IPv6) */
static char serv_port[8] = STR(DEFAULT_PORT_UP); /* server port for upstream traffic */
static unsigned stat_interval = DEFAULT_STAT; /* time interval (in sec) at which statistics are collected and displayed */
static char ssh_user[16] = "guest"; 
static char ssh_port[8] = "22"; 
static char ssh_key[32] = {'\0'}; 
static char ssh_opt[32] = {'\0'}; 
static char str_mac[17] = {'\0'}; 
static int ssh_alive = 60;  /* keepalive seconds */

/* gateway <-> MAC protocol variables */
static uint32_t net_mac_h; /* Most Significant Nibble, network order */
static uint32_t net_mac_l; /* Least Significant Nibble, network order */

static struct timeval pull_timeout = {0, (PULL_TIMEOUT_MS * 1000)}; /* critical for throughput */
static struct timeval push_timeout = {0, (PULL_TIMEOUT_MS * 250)};  /* critical for throughput */

static uint16_t remot_port = 0;

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

static void usage(void);

static void sig_handler(int sigio);

static double difftimespec(struct timespec end, struct timespec beginning);

static void wait_ms(unsigned long a);

static int init_sock(const char *addr, const char *port, const void *timeout, int size); 

static uint16_t get_rssh_port();

static int check(char* dir);

static int pidof(char* processname);

/* threads */
void thread_status(void);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

static void usage( void )
{
    printf("~~~ Available options ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf(" -h  print this help\n");
    printf(" -s <server>  rssh server address, default: 161.117.181.127\n");
    printf(" -p <port>  rssh server port, default 3721\n");
    printf(" -u <username>  ssh username, default guest\n");
    printf(" -P <port>  ssh server port, default 22\n");
    printf(" -i <identityfile>   (multiple allowed, default .ssh/id_rssh)\n");
    printf(" -o <option>  extra option of rssh connect\n");
    printf(" -m <gatewayID> Gateway mac ID\n");
    printf(" -k <keepalive> Senconds for alive Interval\n");
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}

static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
        exit_sig = true;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = true;
    }
    return;
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char ** argv)
{
    struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
    int i = 0; /* loop variable and temporary variable for return value */
    int loop = 0;
    int fd;

    /* threads */
    pthread_t thrid_status;

    /* network socket creation */
    struct addrinfo hints;
    struct addrinfo *result; /* store result of getaddrinfo */
    struct addrinfo *q; /* pointer to move into *result data */
    char host_name[64];
    char port_name[64];

    char pid_file[32];
    char file_buff[64];

    pid_t pid;
    char cmdstring[128];

    unsigned long long ull = 0;

    /* Parse command line options */
    while( (i = getopt( argc, argv, "hs:p:P:i:o:m:u:" )) != -1 )
    {
        switch( i ) {
        case 'h':
            usage( );
            return EXIT_SUCCESS;
            break;

        case 's':
            if (NULL != optarg)
                strncpy(serv_addr, optarg, sizeof(serv_addr));
            break;

        case 'p':
            if (NULL != optarg)
                strncpy(serv_port, optarg, sizeof(serv_port));
            break;

        case 'P':
            if (NULL != optarg)
                strncpy(ssh_port, optarg, sizeof(ssh_port));
            break;

        case 'i':
            if (NULL != optarg)
                strncpy(ssh_key, optarg, sizeof(ssh_key));
            break;

        case 'o':
            if (NULL != optarg)
                strncpy(ssh_opt, optarg, sizeof(ssh_opt));
            break;

        case 'u':
            if (NULL != optarg)
                strncpy(ssh_user, optarg, sizeof(ssh_user));
            break;

        case 'k':
            if (NULL != optarg)
                ssh_alive = atoi(optarg);
            break;

        case 'm':
            if (NULL != optarg) {
                strncpy(str_mac, optarg, sizeof(str_mac));
                sscanf(str_mac, "%llx", &ull);
                lgwm = ull;
                lgw_log(LOG_INFO, "INFO~ GatewayID is configure to %016llX(%s) \n", ull, str_mac);
            } 
            break;

        default:
            lgw_log(LOG_ERROR, "ERROR~ argument parsing options, use -h option for help\n" );
            usage( );
            return EXIT_FAILURE;
        }
    }

    if (strlen(str_mac) < 16) {
        lgw_log(LOG_ERROR, "ERROR~ Must specified gateway MAC address, use option -m\n"); 
        usage();
        return EXIT_FAILURE;
    }

    /* process some of the configuration variables */
    net_mac_h = htonl((uint32_t)(0xFFFFFFFF & (lgwm>>32)));
    net_mac_l = htonl((uint32_t)(0xFFFFFFFF &  lgwm  ));

    i = pthread_create(&thrid_status, NULL, (void * (*)(void *))thread_status, NULL);
    if (i != 0) {
        printf("ERROR~ [main] impossible to create status thread\n");
        exit(EXIT_FAILURE);
    }

    /* configure signal handling */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction(SIGQUIT, &sigact, NULL); /* Ctrl-\ */
    sigaction(SIGINT, &sigact, NULL); /* Ctrl-C */
    sigaction(SIGTERM, &sigact, NULL); /* default "kill" command */

    /* main loop task :  */
    lgw_log(LOG_INFO, "INFO~ [Main] Main loop task: fork rssh process\n");
    
    loop = 0;

    while (!exit_sig) {

        if ((remot_port = get_rssh_port()) <= 0) {
            lgw_log(LOG_INFO, "INFO~ [Main] remote port (%u), run again! (loop:%d)\n", remot_port, loop++);
            wait_ms(1000 * 20); //20s
            continue;
        }

        if ((pid = fork()) < 0) {
            lgw_log(LOG_ERROR, "ERROR~ [RSSH-FORK] fork rssh error, run again!\n");
            wait_ms(250 * DEFAULT_STAT);//15s  
            continue;
        } else if (pid == 0) {         
            if (strlen(ssh_key) > 1) 
                snprintf(cmdstring, sizeof(cmdstring), "ssh -K %d -i %s -yfNR %u:localhost:%s %s@%s", ssh_alive, ssh_key, remot_port, ssh_port, ssh_user, serv_addr);
            else
                snprintf(cmdstring, sizeof(cmdstring), "ssh -K %d -yfNR %u:localhost:%s %s@%s",  ssh_alive, remot_port, ssh_port, ssh_user, serv_addr);
            lgw_log(LOG_DEBUG, "DEBUG~ [RSSH-FORK] run: %s\n", cmdstring);
            execl("/bin/sh", "sh", "-c", cmdstring, (char *)0);
            lgw_log(LOG_DEBUG, "DEBUG~ [RSSH-FORK] exec rssh error!\n");
            _exit(127);     
        } else {             
            for (;;) {
                if (exit_sig) {
                    system("killall ssh");
                    break;
                }
                wait_ms(1000 * 6); //6s
            } 
             
            //lgw_log(LOG_DEBUG, "DEBUG~ [RSSH-CHECK] rssh disconnect, restart rssh connect\n");
        }

    }

    /* wait for upstream thread to finish (1 fetch cycle max) */
    pthread_cancel(thrid_status); 

    lgw_log(LOG_INFO, "INFO~ [Main] Exiting rssh client program\n");
    exit(EXIT_SUCCESS);
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 1: RECEIVING PACKETS AND FORWARDING THEM ---------------------- */

void thread_status(void) {
    int i, j; /* loop variables */

    /* network sockets */
    int sock_up = -1; /* socket for upstream traffic */

    /* data buffers */
    uint8_t buff_up[BUFF_SIZE]; /* buffer to compose the upstream packet */
    uint8_t buff_ack[BUFF_SIZE]; /* buffer to receive acknowledges */

    uint32_t status_push = 0, status_ack = 0;

    /* protocol variables */
    uint8_t token_h; /* random token for acknowledgement matching */
    uint8_t token_l; /* random token for acknowledgement matching */

    /* pre-fill the data buffer with fixed fields */
    buff_up[0] = PROTOCOL_VERSION;
    buff_up[3] = PUSH_STATUS;
    *(uint32_t *)(buff_up + 4) = net_mac_h;
    *(uint32_t *)(buff_up + 8) = net_mac_l;

    lgw_log(LOG_INFO, "INFO~ [PushStatus] Stating Thread for status push\n");

    while (!exit_sig) {

        wait_ms(DEFAULT_STAT * 1000);

        sock_up = init_sock(serv_addr, serv_port, (void*)&push_timeout, sizeof(struct timeval));

        lgw_log(LOG_DEBUG, "DEBUG~ [PushStatus] Create socke to server(%s), port(%s), sock=(%d)\n", serv_addr, serv_port, sock_up);

        if (sock_up == -1) continue;

        /* start composing datagram with the header */
        token_h = (uint8_t)rand(); /* random token */
        token_l = (uint8_t)rand(); /* random token */
        buff_up[1] = token_h;
        buff_up[2] = token_l;

        /* send datagram to server */
        send(sock_up, (void *)buff_up, 12, 0);

        status_push++;

        for (i=0; i<2; ++i) {
            j = recv(sock_up, (void *)buff_ack, sizeof buff_ack, 0);
            if (j == -1) {
                if (errno == EAGAIN) { /* timeout */
                    continue;
                } 
            } else if ((j < 4) || (buff_ack[0] != PROTOCOL_VERSION) || (buff_ack[3] != ACK_STATUS)) {
                lgw_log(LOG_DEBUG, "DEBUG~ [PushStatus] ignored invalid non-ACL packet\n");
                continue;
            } else if ((buff_ack[1] != token_h) || (buff_ack[2] != token_l)) {
                lgw_log(LOG_DEBUG, "DEBUG~ [PushStatus] ignored out-of sync ACK packet\n");
                continue;
            } else {
                status_ack++;
                lgw_log(LOG_DEBUG, "DEBUG~ [PushStatus] ACK_STATUS received\n");
                break;
            }
        }

        shutdown(sock_up, SHUT_RDWR);

        lgw_log(LOG_DEBUG, "DEBUG~ [PushStatus] PUSH_ACK = %u, ACK_STATUS = %u;\n", status_push, status_ack);

    }

    lgw_log(LOG_INFO, "\nINFO~ [PushStatus] End of pull status thread\n");
}

static uint16_t get_rssh_port() {
    int j;
    uint16_t port = 0;

    /* network sockets */
    int sock_up = -1; /* socket for upstream traffic */

    /* data buffers */
    uint8_t buff_up[BUFF_SIZE]; /* buffer to compose the upstream packet */
    uint8_t buff_ack[BUFF_SIZE]; /* buffer to receive acknowledges */

    /* protocol variables */
    uint8_t token_h; /* random token for acknowledgement matching */
    uint8_t token_l; /* random token for acknowledgement matching */

    /* ping measurement variables */
    struct timespec send_time;
    struct timespec recv_time;

    buff_up[0] = PROTOCOL_VERSION;
    buff_up[3] = PULL_PORT;
    /* start composing datagram with the header */
    token_h = (uint8_t)rand(); /* random token */
    token_l = (uint8_t)rand(); /* random token */
    buff_up[1] = token_h;
    buff_up[2] = token_l;
    *(uint32_t *)(buff_up + 4) = net_mac_h;
    *(uint32_t *)(buff_up + 8) = net_mac_l;

    sock_up = init_sock(serv_addr, serv_port, (void*)&pull_timeout, sizeof(struct timeval));

    lgw_log(LOG_DEBUG, "DEBUG~ [GetPort] Create socke to server(%s), port(%s), sock=(%d)\n", serv_addr, serv_port, sock_up);

    if (sock_up == -1) return 0;
    /* send datagram to server */
    send(sock_up, (void *)buff_up, 12, 0);

    clock_gettime(CLOCK_MONOTONIC, &send_time);
    recv_time = send_time;

    while ((int)difftimespec(recv_time, send_time) < DEFAULT_KEEPALIVE) {

        j = recv(sock_up, (void *)buff_ack, sizeof buff_ack, 0);
        clock_gettime(CLOCK_MONOTONIC, &recv_time);
        if (j == -1) {
            if (errno == EAGAIN) { /* timeout */
                continue;
            } 
        } else if ((j < 4) || (buff_ack[0] != PROTOCOL_VERSION) || (buff_ack[3] != ACK_PORT)) {
            lgw_log(LOG_DEBUG, "DEBUG~ [GetPort] ignored invalid non-ACL packet\n");
            continue;
        } else if ((buff_ack[1] != token_h) || (buff_ack[2] != token_l)) {
            lgw_log(LOG_DEBUG, "DEBUG~ [GetPort] ignored out-of sync ACK packet\n");
            continue;
        } else {
#ifdef BIGENDIAN
            uint8_t port_h = *((uint8_t *)(buff_ack + 5));
            uint8_t port_l = *((uint8_t *)(buff_ack + 4));
            port = (uint16_t)((port_h << 8) + port_l);
#else
            port = *((uint16_t *)(buff_ack + 4));
#endif
            lgw_log(LOG_DEBUG, "DEBUG~ [GetPort] PULL_PORT received in %i ms, port=%u\n", (int)(1000 * difftimespec(recv_time, send_time)), port);
            break;
        }
    }

    shutdown(sock_up, SHUT_RDWR);

    return port;
}

static double difftimespec(struct timespec end, struct timespec beginning) {
    double x;

    x = 1E-9 * (double)(end.tv_nsec - beginning.tv_nsec);
    x += (double)(end.tv_sec - beginning.tv_sec);

    return x;
}

static void wait_ms(unsigned long a) {
    struct timespec dly;
    struct timespec rem;

    dly.tv_sec = a / 1000;
    dly.tv_nsec = ((long)a % 1000) * 1000000;

    //lgw_log(LOG_DEBUG, "NOTE dly: %ld sec %ld ns\n", dly.tv_sec, dly.tv_nsec);

    if((dly.tv_sec > 0) || ((dly.tv_sec == 0) && (dly.tv_nsec > 100000))) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &dly, &rem);
        //lgw_log(LOG_DEBUG, "NOTE remain: %ld sec %ld ns\n", rem.tv_sec, rem.tv_nsec);
    }
    return;
}

static int init_sock(const char *addr, const char *port, const void *timeout, int size) {
	int i;
    int sockfd;
	/* network socket creation */
	struct addrinfo hints;
	struct addrinfo *result;	/* store result of getaddrinfo */
	struct addrinfo *q;			/* pointer to move into *result data */

	char host_name[64];
	char port_name[64];

	/* prepare hints to open network sockets */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;	/* WA: Forcing IPv4 as AF_UNSPEC makes connection on localhost to fail */
	hints.ai_socktype = SOCK_DGRAM;

	/* look for server address w/ upstream port */
	i = getaddrinfo(addr, port, &hints, &result);
	if (i != 0) {
		lgw_log(LOG_ERROR, "ERROR~ [NETWORK] getaddrinfo on address %s (PORT %s) returned %s\n", addr, port, gai_strerror(i));
	    freeaddrinfo(result);
		return -1;
	}

	/* try to open socket for upstream traffic */
	for (q = result; q != NULL; q = q->ai_next) {
		sockfd = socket(q->ai_family, q->ai_socktype, q->ai_protocol);
		if (sockfd == -1)
			continue;			/* try next field */
		else
			break;			/* success, get out of loop */
	}

	if (q == NULL) {
		lgw_log(LOG_ERROR, "ERROR~ [NETWORK] failed to open socket to any of server %s addresses (port %s)\n", addr, port);
		i = 1;
		for (q = result; q != NULL; q = q->ai_next) {
			getnameinfo(q->ai_addr, q->ai_addrlen, host_name, sizeof host_name,
						port_name, sizeof port_name, NI_NUMERICHOST);
			++i;
		}
    	freeaddrinfo(result);

		return -1;
	}

	/* connect so we can send/receive packet with the server only */
	i = connect(sockfd, q->ai_addr, q->ai_addrlen);
	if (i != 0) {
		lgw_log(LOG_ERROR, "WARM~ [NETWORK] connecting... %s\n", strerror(errno));
	    freeaddrinfo(result);
		return -1;
	}

	freeaddrinfo(result);

	if ((setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, timeout, size)) != 0) {
		lgw_log(LOG_ERROR, "ERROR~ [NETWORK] setsockopt returned %s\n", strerror(errno));
		return -1;
	}

	return sockfd;
}

static int check(char *dir)
{
	int i;
	for(i = 0; dir[i] != '\0'; i++)
	{
		if(!isdigit(dir[i]))
		{
			return 0;
		}
	}
	return 1;
}

static int pidof(char *processname)
{
	DIR *dir;
	FILE *f;
	struct dirent *record;
    int i = 0, pid = 0;

	int *pidlist,pidlist_x=0, pidlist_realloc=1;
	char path[128], buffer[8];

	dir = opendir("/proc/");
	if(dir == NULL) {
		return -1;
	}

	while ((record = readdir(dir)) != NULL) {
		if (check(record->d_name)) {
			strcpy(path, "/proc/");
			strcat(path, record->d_name);
			strcat(path, "/comm");

			f = fopen(path, "r");

			if (f != NULL) {
                memset(buffer, 0, sizeof(buffer));
                fread(buffer, sizeof(char), sizeof(buffer) - 1, f);
                //printf("#%s++buffer:%s\n", record->d_name, buffer);
				if (strcmp(buffer, processname) == 0) {
					pid = atoi(record->d_name);
			        fclose(f);
                    return pid;
				}
			} 

			fclose(f);
		}		 
	}

	closedir(dir);

	return -1;
}



