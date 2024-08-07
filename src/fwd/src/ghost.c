/*!>
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

/*!>!
 * \file
 * \brief lora packages simulation
 */

/*!> fix an issue between POSIX and C99 */
#ifdef __MACH__
#elif __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>				/*!> C99 types */
#include <stdbool.h>			/*!> bool type */
#include <stdio.h>				/*!> printf, fprintf, snprintf, fopen, fputs */

#include <string.h>				/*!> memset */
#include <signal.h>				/*!> sigaction */
#include <time.h>				/*!> time, clock_gettime, strftime, gmtime */
#include <sys/time.h>			/*!> timeval */
#include <unistd.h>				/*!> getopt, access */
#include <stdlib.h>				/*!> atoi, exit */
#include <errno.h>				/*!> error messages */

#include <sys/socket.h>			/*!> socket specific definitions */
#include <netinet/in.h>			/*!> INET constants and stuff */
#include <arpa/inet.h>			/*!> IP address conversion stuff */
#include <netdb.h>				/*!> gai_strerror */

#include <pthread.h>
#include "fwd.h"
#include "ghost.h"
#include "loragw_hal.h"

DECLARE_GW;

/*!> -------------------------------------------------------------------------- */
/*!> --- PRIVATE MACROS ------------------------------------------------------- */

static volatile bool ghost_run = false;	/*!> false -> ghost thread terminates cleanly */

static pthread_mutex_t cb_ghost = PTHREAD_MUTEX_INITIALIZER;	/*!> control access to the ghoststream measurements */

static int sock_ghost;			/*!> socket for downstream traffic */
static char gateway_id[16] = "";	/*!> string form of gateway mac address */

static struct lgw_pkt_rx_s rxpkt[GHST_NB_PKT];
static uint8_t cnt_ghost = 0;

/*!> ghost thread */
static pthread_t thrid_ghost;

/*!> for debugging purposes. */
static void print_buf(uint8_t * b, uint8_t len) __attribute__ ((unused));
static void print_buf(uint8_t * b, uint8_t len) {
	int i;
	for (i = 0; i < len; i++) {
		printf("%i,", b[i]);
	}
}

/*!> for debugging purposes. */
static void print_rx(struct lgw_pkt_rx_s *p) __attribute__ ((unused));
static void print_rx(struct lgw_pkt_rx_s *p) {
	printf("  p->freq_hz    = %i\n"
		   "  p->if_chain   = %i\n"
		   "  p->status     = %i\n"
		   "  p->count_us   = %i\n"
		   "  p->rf_chain   = %i\n"
		   "  p->modulation = %i\n"
		   "  p->bandwidth  = %i\n"
		   "  p->datarate   = %i\n"
		   "  p->coderate   = %i\n"
		   "  p->rssis       = %f\n"
		   "  p->snr        = %f\n"
		   "  p->snr_min    = %f\n"
		   "  p->snr_max    = %f\n"
		   "  p->crc        = %i\n"
		   "  p->size       = %i\n"
		   "  p->payload    = %s\n",
		   p->freq_hz, p->if_chain, p->status, p->count_us,
		   p->rf_chain, p->modulation, p->bandwidth, p->datarate,
		   p->coderate, p->rssis, p->snr, p->snr_min, p->snr_max,
		   p->crc, p->size, p->payload);
}


/*!> Method to fill lgw_pkt_rx_s with data */
static void fill_rx(struct lgw_pkt_rx_s *p, uint8_t *payload, uint32_t us) {
	p->freq_hz = (uint32_t) 868320000;
	p->if_chain = 2;
	p->status = STAT_CRC_OK;
	p->count_us = us;
	p->rf_chain = 1;
	p->modulation = MOD_LORA;
	p->bandwidth = BW_125KHZ;
	p->datarate = DR_LORA_SF10;
	p->coderate = CR_LORA_4_5;
	p->rssis = -112;
	p->snr = 22;
	p->snr_min = 11;
	p->snr_max = 43;
	p->crc = 2234;
	p->size = 48;
	memcpy((p->payload), payload, p->size);
}

static void thread_ghost(void);

/*!> -------------------------------------------------------------------------- */
/*!> --- THREAD: RECEIVING PACKETS FROM GHOST NODES --------------------------- */

bool ghost_start(const char *ghost_addr, const char *ghost_port, const char *gwid) {
	/*!> You cannot start a running ghost listener. */
	if (ghost_run)
		return true;

	int i;						/*!> loop variable and temporary variable for return value */

	/*!> copy the static coordinates (so if the gps changes, this is not reflected!) */
	strncpy(gateway_id, gwid, sizeof(gateway_id));

	struct addrinfo addresses;
	struct addrinfo *result;	/*!> store result of getaddrinfo */
	struct addrinfo *q;			/*!> pointer to move into *result data */
	char host_name[64];
	char port_name[64];

	memset(&addresses, 0, sizeof addresses);
	addresses.ai_family = AF_INET;	
	addresses.ai_socktype = SOCK_DGRAM;

	/*!> Get the credentials for this server. */
	i = getaddrinfo(ghost_addr, ghost_port, &addresses, &result);
	if (i != 0) {
		lgw_log(LOG_ERROR, "[ERROR~][ghost] getaddrinfo on address %s (PORT %s) returned %s\n", ghost_addr, ghost_port, gai_strerror(i));
        return false;
	}

	/*!> try to open socket for ghost listener */
	for (q = result; q != NULL; q = q->ai_next) {
		sock_ghost = socket(q->ai_family, q->ai_socktype, q->ai_protocol);
		if (sock_ghost == -1)
			continue;			/*!> try next field */
		else {
            i = bind(sock_ghost, q->ai_addr, q->ai_addrlen);
			if( i == -1 ) {
			    shutdown(sock_ghost, SHUT_RDWR);
				continue; /*!> bind failed, try next field */
			} else
			    break;
        }
	}

	/*!> See if the connection was a success, if not, this is a permanent failure */
	if (q == NULL) {
		lgw_log(LOG_ERROR, "[ERROR~][ghost] failed to open socket to any of server %s addresses (port %s)\n", ghost_addr, ghost_port);
		i = 1;
		for (q = result; q != NULL; q = q->ai_next) {
			getnameinfo(q->ai_addr, q->ai_addrlen, host_name, sizeof host_name, port_name, sizeof port_name, NI_NUMERICHOST);
			lgw_log(LOG_INFO, "[INFO~][ghost] result %i host:%s service:%s\n", i, host_name, port_name);
			++i;
		}
        return false;
	}

	freeaddrinfo(result);

	/*!> spawn thread to manage ghost connection */
	i = lgw_pthread_create(&thrid_ghost, NULL, (void *(*)(void *))thread_ghost, NULL);
	if (i != 0) {
		lgw_log(LOG_ERROR, "[ERROR~][ghost] impossible to create ghost thread\n");
        return false;
	}

	ghost_run = true;

    return true;
	/*!> We are done here, ghost thread is initialized and should be running by now. */

}

void ghost_stop(void) {
    cnt_ghost = 0;
	ghost_run = false;			/*!> terminate the loop. */
	pthread_cancel(thrid_ghost);	/*!> don't wait for downstream thread (is this okay??) */
	shutdown(sock_ghost, SHUT_RDWR);	/*!> close the socket. */
}

/*!> Call this to pull data from the receive buffer for ghost nodes.. */
int ghost_get(int max_pkt, struct lgw_pkt_rx_s *pkt_data) {	/*!> Calculate the number of available packets */
    int i = cnt_ghost;
    if (cnt_ghost == 0) {
        return 0;
    }

	pthread_mutex_lock(&cb_ghost);

    memcpy(pkt_data, &rxpkt, sizeof(struct lgw_pkt_rx_s) * cnt_ghost);

    cnt_ghost = 0;  /*!> reset */

	pthread_mutex_unlock(&cb_ghost);

	if (i > 0) {
		lgw_log(LOG_INFO, "[INFO~][ghost] copied %i packets from ghost \n", i);
	}

	return i;
}

static void thread_ghost(void) {

    struct timeval current_unix_time;
    uint32_t rec_us;

	/*!> data buffers */
    uint8_t databuf_rec[GHST_RX_BUFFSIZE];

    int byte_nb;

    struct sockaddr_storage dist_addr;
    socklen_t addr_len = sizeof dist_addr;

	lgw_log(LOG_INFO, "[INFO~][ghost] Ghost thread started...\n");

	while (ghost_run) {			

        memset( databuf_rec, 0, sizeof(databuf_rec) );
        byte_nb = recvfrom( sock_ghost, databuf_rec, sizeof(databuf_rec), 0, (struct sockaddr *)&dist_addr, &addr_len );

        if( byte_nb == -1 )
        {
            lgw_log( LOG_ERROR, "[ERROR~][ghost] recvfrom returned %s \n", strerror( errno ) );
            continue;
        }

		pthread_mutex_lock(&cb_ghost);

		/*!> make a copy to the data received to the circular buffer, and shift the write index. */
		if (++cnt_ghost > GHST_NB_PKT) {
			lgw_log(LOG_WARNING, "[WARNING~][ghost] buffer is full, dropping packet)\n");
		} else {
            gettimeofday(&current_unix_time, NULL);
            rec_us = current_unix_time.tv_sec + current_unix_time.tv_usec;
            fill_rx(&rxpkt[cnt_ghost - 1], databuf_rec, rec_us);
			lgw_log(LOG_INFO, "[INFO~][ghost] RECEIVED ghst_index = %i \n", cnt_ghost);
        }

		pthread_mutex_unlock(&cb_ghost);

        /*!> Display info about the sender */
        /*!>
        i = getnameinfo( (struct sockaddr *)&dist_addr, addr_len, host_name, sizeof host_name, port_name, sizeof port_name, NI_NUMERICHOST );

        if( i == -1 )
        {
            lgw_log( LOG_ERROR, "[ERROR~][ghost] getnameinfo returned %s \n", gai_strerror( i ) );
            continue;
        }
        
        lgw_log( LOG_INFO, "[INFO~][ghost] -> pkt in , host %s (port %s), %i bytes \n", host_name, port_name, byte_nb );
        */

        
    }

	lgw_log(LOG_INFO, "[INFO~][ghost] End of ghost thread\n");
}
