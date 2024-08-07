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
 * \brief 
 *  Description:
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>
#include <inttypes.h>  
#include <math.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "fwd.h"
#include "uart.h"
#include "service.h"
#include "relay_service.h"
#include "jitqueue.h"
#include "parson.h"
#include "base64.h"

#include "timersync.h"
#include "loragw_aux.h"
#include "mac-header-decode.h"

DECLARE_GW;

static void relay_push_up(void* arg);

int relay_start(serv_s* serv) {

    if (!GW.relay.as_relay) 
        return -1;

    if (lgw_pthread_create_background(&serv->thread.t_up, NULL, (void *(*)(void *))relay_push_up, serv)) {
        lgw_log(LOG_WARNING, "%s[\033[1;32mRELAY\033[m][%s] Can't create relay push up pthread.\n", WARNMSG, serv->info.name);
        return -1;
    }

    serv->state.live = true;
    serv->state.stall_time = 0;
    serv->state.startup_time = time(NULL);  //UTC seconds
    serv->state.connecting = false;
    LGW_LIST_LOCK(&GW.rxpkts_list);
    GW.info.service_count++;
    LGW_LIST_UNLOCK(&GW.rxpkts_list);
    lgw_db_put("service/relay", serv->info.name, "running");
    lgw_db_put("thread", serv->info.name, "running");

    return 0;
}

int relay_stop(serv_s* serv) {
    if (!GW.relay.as_relay) 
        return 0;
    serv->thread.stop_sig = true;
	sem_post(&serv->thread.sema);
    pthread_join(serv->thread.t_up, NULL);
    serv->state.live = false;
    LGW_LIST_LOCK(&GW.rxpkts_list);
    GW.info.service_count--;
    LGW_LIST_UNLOCK(&GW.rxpkts_list);
    lgw_db_del("service/relay", serv->info.name);
    lgw_db_del("thread", serv->info.name);
    return 0;
}

static void relay_push_up(void* arg) {
    serv_s* serv = (serv_s*) arg;
    int i = 0, j = 0, l = 0;

    char buffer[544] = {'\0'};
    char payload_to_hex[512] = {'\0'};
    struct lgw_pkt_rx_s *p; /*!> pointer on a RX packet */

    lgw_log(LOG_INFO, "%s[THREAD][%s-UP] Starting...\n", INFOMSG, serv->info.name);

	while (!serv->thread.stop_sig) {
        sem_wait(&serv->thread.sema);
        do {
            serv_ct_s *serv_ct = lgw_malloc(sizeof(serv_ct_s));
            serv_ct->serv = serv;
            serv_ct->nb_pkt = get_rxpkt(serv_ct);     //only get the first rxpkt of list
                                                               //
            if (serv_ct->nb_pkt == 0) { 
                lgw_free(serv_ct);
                break;
            }

            for (i = 0; i < serv_ct->nb_pkt; i++) {
                p = &serv_ct->rxpkt[i];
                if (p->if_chain == 8)  // ignore lora service channel 
                    continue;

                memset(buffer, 0, sizeof(buffer));
                memset(payload_to_hex, 0, sizeof(payload_to_hex));

                lgw_log(LOG_DEBUG, "%s[\033[1;32mRELAY\033[m] count_us(%u)\n", DEBUGMSG, p->count_us);

                sprintf(buffer, "%08x", p->count_us);  
                sprintf(&payload_to_hex[0], "%02x", RELAY_UP);  /*!> paylad direction */

#ifdef BIGENDIAN
                payload_to_hex[2] = buffer[6];         /*! count us */
                payload_to_hex[3] = buffer[7];
                payload_to_hex[4] = buffer[4];
                payload_to_hex[5] = buffer[5];
                payload_to_hex[6] = buffer[2];
                payload_to_hex[7] = buffer[3];
                payload_to_hex[8] = buffer[0];
                payload_to_hex[9] = buffer[1];
#else
                payload_to_hex[2] = buffer[0];
                payload_to_hex[3] = buffer[1];
                payload_to_hex[4] = buffer[2];
                payload_to_hex[5] = buffer[3];
                payload_to_hex[6] = buffer[4];
                payload_to_hex[7] = buffer[5];
                payload_to_hex[8] = buffer[6];
                payload_to_hex[9] = buffer[7];
#endif

                lgw_log(LOG_DEBUG, "%s[\033[1;32mRELAY\033[m] PAYLOAD(%d):", DEBUGMSG, p->size);
                for (j = 0, l = 10; j < (p->size < sizeof(payload_to_hex)/2 ? p->size : sizeof(payload_to_hex)/2); ++j) {
                    sprintf(&payload_to_hex[l], "%02x", p->payload[j]);
                    l += 2;
                    lgw_log(LOG_DEBUG, "%02x", p->payload[j]);
                }
                lgw_log(LOG_DEBUG, "\n");


                //bin2hex((char*)p->payload, payload_to_hex, p->size);
                snprintf(buffer, sizeof(buffer), "AT+SEND=0,%s,0,0\r\n", payload_to_hex);
                lgw_log(LOG_DEBUG, "%s[\033[1;32mRELAY\033[m] AS-relay: %s \n", INFOMSG, buffer);
                if (uart_send(GW.relay.tty_fd, buffer, strlen(buffer) + 1) == -1) { 
                    lgw_log(LOG_ERROR, "%s[\033[1;32mRELAY\033[m] (cannot send command to uart)\n", ERRMSG);
                }

                if ( i < serv_ct->nb_pkt)
                    wait_ms(10);  /*!> wait uart */
            }

            lgw_log(LOG_DEBUG, "%s[%s] relay_push_up push %d %s.\n", DEBUGMSG, serv->info.name, serv_ct->nb_pkt, serv_ct->nb_pkt < 2 ? "packet" : "packets");

            lgw_free(serv_ct);

        } while (GW.rxpkts_list.size > 1 && (!serv->thread.stop_sig));
    }

	lgw_log(LOG_INFO, "\n%s[THREAD][%s-UP] Ended!\n", INFOMSG, serv->info.name);
}

