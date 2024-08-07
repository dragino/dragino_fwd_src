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
#include <sqlite3.h>

#include "fwd.h"
#include "service.h"
#include "delay_service.h"

#include "timersync.h"
#include "loragw_aux.h"

#include "loragw_hal.h"

#define CREATE_TB_DELAY_PKTS "CREATE TABLE IF NOT EXISTS delay_pkts(id INTEGER PRIMARY KEY AUTOINCREMENT, payload BLOB, size INTEGER);"

const char *store_pkts = "INSERT OR REPLACE INTO delay_pkts (payload, size) VALUES (?, ?);";
const char *get_pkts = "SELECT id, payload, size FROM delay_pkts ORDER BY id LIMIT ?;";
const char *del_pkts = "DELETE FROM delay_pkts WHERE id < ?;";

static sqlite3* delay_db;
static sqlite3_stmt *store_pkts_stmt;
static sqlite3_stmt *get_pkts_stmt;
static sqlite3_stmt *del_pkts_stmt;
static sqlite3_stmt *count_pkts_stmt;
static char *err_msg = 0;

static bool has_db_storage = false;

DECLARE_GW;

typedef struct _delay_pkt {
    struct lgw_pkt_rx_s *rxpkt;
    uint8_t nb_pkt;
    LGW_LIST_ENTRY(_delay_pkt) list;
} delay_pkt_s;

LGW_LIST_HEAD_STATIC(delay_pkt_list, _delay_pkt);    /*?> defined data list for delay package */

static bool delay_service_status_alive = false;

static void delay_package_thread(void* arg);

int delay_start(serv_s* serv) {

    if (!GW.cfg.delay_enabled)
        return -1;

    if (lgw_pthread_create_background(&serv->thread.t_up, NULL, (void *(*)(void *))delay_package_thread, serv)) {
        lgw_log(LOG_WARNING, "%s[%s] Can't create delay push up pthread.\n", WARNMSG, serv->info.name);
        serv->state.live = false;
        return -1;
    }

    serv->state.live = true;
    delay_service_status_alive = true;
    serv->state.stall_time = 0;
    serv->state.startup_time = time(NULL);  //UTC seconds
    serv->state.connecting = false;

    LGW_LIST_LOCK(&GW.rxpkts_list);
    GW.info.service_count++;
    LGW_LIST_UNLOCK(&GW.rxpkts_list);
    lgw_db_put("service/delay", serv->info.name, "running");
    lgw_db_put("thread", serv->info.name, "running");

    if (GW.cfg.delay_db_path[0] != '\0') {
	    if (sqlite3_open(GW.cfg.delay_db_path, &delay_db) != SQLITE_OK) {
		    lgw_log(LOG_WARNING, "%s[\033[1;34mDELAY\033[m] Unable to open storage database '%s': %s\n", WARNMSG, GW.cfg.delay_db_path, sqlite3_errmsg(delay_db));
        } else if (sqlite3_exec(delay_db, CREATE_TB_DELAY_PKTS, NULL, 0, &err_msg) != SQLITE_OK) {
		    lgw_log(LOG_WARNING, "%s[\033[1;34mDELAY\033[m] Unable to create storage tables.\n", WARNMSG);
            sqlite3_free(err_msg);
        } else if (sqlite3_prepare_v2(delay_db, store_pkts, -1, &store_pkts_stmt, 0) != SQLITE_OK) {
		    lgw_log(LOG_WARNING, "%s[\033[1;34mDELAY\033[m] Unable prepare store_pkts stmt : %s\n", WARNMSG, sqlite3_errmsg(delay_db));
        } else if (sqlite3_prepare_v2(delay_db, get_pkts, -1, &get_pkts_stmt, 0) != SQLITE_OK) {
		    lgw_log(LOG_WARNING, "%s[\033[1;34mDELAY\033[m] Unable prepare get_pkts stmt : %s\n", WARNMSG, sqlite3_errmsg(delay_db));
        } else if (sqlite3_prepare_v2(delay_db, del_pkts, -1, &del_pkts_stmt, 0) != SQLITE_OK) {
		    lgw_log(LOG_WARNING, "%s[\033[1;34mDELAY\033[m] Unable prepare del_pkts stmt : %s\n", WARNMSG, sqlite3_errmsg(delay_db));
        } else if (sqlite3_prepare_v2(delay_db, "SELECT COUNT(*) FROM delay_pkts", -1, &count_pkts_stmt, 0) != SQLITE_OK) {
		    lgw_log(LOG_WARNING, "%s[\033[1;34mDELAY\033[m] Unable prepare count_pkts stmt : %s\n", WARNMSG, sqlite3_errmsg(delay_db));
        } else 
            has_db_storage = true;

        if (!has_db_storage) {
            sqlite3_close_v2(delay_db);
        }
    }

    return 0;
}

int delay_stop(serv_s* serv) {

    if (serv->state.live) {
        serv->thread.stop_sig = true;
        sem_post(&serv->thread.sema);
        pthread_join(serv->thread.t_up, NULL);
    }

    serv->state.live = false;
    delay_service_status_alive = false;
    LGW_LIST_LOCK(&GW.rxpkts_list);
    GW.info.service_count--;
    LGW_LIST_UNLOCK(&GW.rxpkts_list);
    lgw_db_del("service/delay", serv->info.name);
    lgw_db_del("thread", serv->info.name);

    if (has_db_storage) {
        sqlite3_finalize(store_pkts_stmt);
        sqlite3_finalize(get_pkts_stmt);
        sqlite3_finalize(del_pkts_stmt);
        sqlite3_finalize(count_pkts_stmt);
        sqlite3_close_v2(delay_db);
    }

    return 0;
}

int delay_pkt_get(int max, struct lgw_pkt_rx_s *pkt_data) {	/*!> Calculate the number of available packets */
    
    if (!GW.info.network_status || (!delay_service_status_alive) || (max < 1)) 
        return 0;

    uint8_t nb_pkt = 0;
    
    if (!has_db_storage) { 
        if (delay_pkt_list.size < 1)
            return 0;
        delay_pkt_s *entry = NULL;
        LGW_LIST_LOCK(&delay_pkt_list);
        LGW_LIST_TRAVERSE_SAFE_BEGIN(&delay_pkt_list, entry, list) {
            if ((nb_pkt + entry->nb_pkt) > max)
                break;
            memcpy(pkt_data, entry->rxpkt, sizeof(struct lgw_pkt_rx_s) * entry->nb_pkt);
            nb_pkt += entry->nb_pkt;
            pkt_data += entry->nb_pkt;
            LGW_LIST_REMOVE_CURRENT(list);
            lgw_free(entry->rxpkt);
            lgw_free(entry);
            delay_pkt_list.size--;
        }
        LGW_LIST_TRAVERSE_SAFE_END;
        LGW_LIST_UNLOCK(&delay_pkt_list);

	    lgw_log(LOG_DEBUG, "%s[\033[1;34mDELAY\033[m] get %i, remain %i packet(s)\n", DEBUGMSG, nb_pkt, delay_pkt_list.size);

    } else {
        int count = 0;

        if (sqlite3_step(count_pkts_stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(count_pkts_stmt, 0);
            sqlite3_reset(count_pkts_stmt);
        }

        if (count == 0) {
            return 0;
        }

        uint32_t id = 0; 
        const void *rxpkt = NULL;
        int pkts = 0;

        sqlite3_bind_int(get_pkts_stmt, 1, max);

        while (sqlite3_step(get_pkts_stmt) == SQLITE_ROW && (nb_pkt < max)) {
            rxpkt = sqlite3_column_blob(get_pkts_stmt, 1);
            pkts = sqlite3_column_int(get_pkts_stmt, 2);
            if (nb_pkt + pkts > max)
                break;
            id = sqlite3_column_int(get_pkts_stmt, 0);
            memcpy(pkt_data, rxpkt, sizeof(struct lgw_pkt_rx_s) * pkts);
            pkt_data += pkts;
            nb_pkt += pkts;
        };

        sqlite3_reset(get_pkts_stmt);

	    lgw_log(LOG_DEBUG, "%s[\033[1;34mDELAY\033[m] get %i packets from db storage.\n", DEBUGMSG, nb_pkt);

        if (id > 0) {
            sqlite3_bind_int(del_pkts_stmt, 1, id + 1);
            pkts = sqlite3_column_count(del_pkts_stmt);
            sqlite3_step(del_pkts_stmt);
            sqlite3_reset(del_pkts_stmt);
	        lgw_log(LOG_DEBUG, "%s[\033[1;34mDELAY\033[m] remove %i packets from db storage.\n", DEBUGMSG, pkts);
        }

    }

	return nb_pkt;
}

static void delay_package_thread(void* arg) {
    serv_s* serv = (serv_s*) arg;

    int count = 0, i = 0;

    struct lgw_pkt_rx_s *p; 

    lgw_log(LOG_INFO, "%s[THREAD][%s-UP] Starting....\n", INFOMSG, serv->info.name);

	while (!serv->thread.stop_sig) {

        sem_wait(&serv->thread.sema);

        do {
            serv_ct_s *serv_ct = lgw_malloc(sizeof(serv_ct_s));
            serv_ct->serv = serv;
            serv_ct->nb_pkt = get_rxpkt(serv_ct);     
                                                      
            if (GW.info.network_status || (serv_ct->nb_pkt == 0)) { 
                lgw_free(serv_ct);
                break;
            }

            for (i = 0; i < serv_ct->nb_pkt; i++) {
                p = &serv_ct->rxpkt[i];
                p->if_chain = IF_DELAY;
            }

            if (!has_db_storage) {
                delay_pkt_s *entry = lgw_malloc(sizeof(delay_pkt_s));
                entry->rxpkt = lgw_malloc(sizeof(struct lgw_pkt_rx_s) * serv_ct->nb_pkt);
                entry->nb_pkt = serv_ct->nb_pkt;
                memcpy(entry->rxpkt, serv_ct->rxpkt, serv_ct->nb_pkt * sizeof(struct lgw_pkt_rx_s));
                LGW_LIST_LOCK(&delay_pkt_list);
                LGW_LIST_INSERT_TAIL(&delay_pkt_list, entry, list);
                if (delay_pkt_list.size > DELAY_PKTS_MAX) {
                    entry = LGW_LIST_REMOVE_HEAD(&delay_pkt_list, list);
                    lgw_free(entry->rxpkt);
                    lgw_free(entry);
                    delay_pkt_list.size--;
                    lgw_log(LOG_DEBUG, "%s[\033[1;34mDELAY\033[m] Do Remove, overload MAXPKTS!\n", DEBUGMSG);
                }
                LGW_LIST_UNLOCK(&delay_pkt_list);
                lgw_log(LOG_DEBUG, "%s[\033[1;34mDELAY\033[m] Store package, total %d \n", DEBUGMSG, delay_pkt_list.size);
            } else {
                sqlite3_bind_blob(store_pkts_stmt, 1, serv_ct->rxpkt, serv_ct->nb_pkt * sizeof(struct lgw_pkt_rx_s), SQLITE_STATIC);
                sqlite3_bind_int(store_pkts_stmt, 2,  serv_ct->nb_pkt);
                sqlite3_step(store_pkts_stmt);
                sqlite3_reset(store_pkts_stmt);

                if (sqlite3_step(count_pkts_stmt) == SQLITE_ROW) {
                    count = sqlite3_column_int(count_pkts_stmt, 0);
                    sqlite3_reset(count_pkts_stmt);
                }

                lgw_log(LOG_DEBUG, "%s[\033[1;34mDELAY\033[m] Store %u package, total %d \n", DEBUGMSG, serv_ct->nb_pkt, count);
            }

            lgw_free(serv_ct);

            wait_ms(100);

        } while (GW.rxpkts_list.size > 1 && (!serv->thread.stop_sig));
    }

	lgw_log(LOG_INFO, "\n%s[THREAD][%s-UP] Ended!\n", INFOMSG, serv->info.name);
}

