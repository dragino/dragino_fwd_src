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
 * \brief GW service process 
 *      
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "fwd.h"
#include "db.h"
#include "service.h"
#include "semtech_service.h"
#include "pkt_service.h"
#include "relay_service.h"
#include "delay_service.h"
#include "mqtt_service.h"
#include "logger.h"

DECLARE_GW;

int init_sock(const char *addr, const char *port, const void *timeout, int size) {
    if (!addr || !port || !timeout || size <= 0) {
        lgw_log(LOG_ERROR, "%s[NETWORK] Invalid parameters in init_sock\n", ERRMSG);
        return -1;
    }

    int ret = -1;
    int sockfd = -1;

    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *q = NULL;

    /* prepare hints to open network sockets */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;    /* Force IPv4 */
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_ADDRCONFIG; /* Only return addresses that this host can use */

    /* DNS lookup */
    ret = getaddrinfo(addr, port, &hints, &result);
    if (ret != 0) {
        lgw_log(LOG_ERROR, "%s[NETWORK] getaddrinfo failed: %s\n", ERRMSG, gai_strerror(ret));
        if (result != NULL) {
            freeaddrinfo(result);
        }
        return -1;
    }

    /* try each address until we successfully connect */
    for (q = result; q != NULL; q = q->ai_next) {
        sockfd = socket(q->ai_family, q->ai_socktype, q->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        /* set socket options */
        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            lgw_log(LOG_WARNING, "%s[NETWORK] Failed to set SO_REUSEADDR: %s\n", WARNMSG, strerror(errno));
        }

        /* set receive timeout */
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, timeout, size) < 0) {
            lgw_log(LOG_ERROR, "%s[NETWORK] Failed to set SO_RCVTIMEO: %s\n", ERRMSG, strerror(errno));
            Close(sockfd);
            freeaddrinfo(result);
            return -1;
        }

        /* connect to server */
        if (connect(sockfd, q->ai_addr, q->ai_addrlen) == 0) {
            /* Success */
            break;
        }

        /* Connect failed, close this socket and try next address */
        Close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(result);

    if (sockfd < 0) {
        lgw_log(LOG_ERROR, "%s[NETWORK] Could not connect to %s:%s: %s\n", 
                ERRMSG, addr, port, strerror(errno));
        return -1;
    }

    return sockfd;
}

bool pkt_basic_filter(serv_s* serv, const uint32_t addr, const uint8_t fport, const char* deveui) {
    char addr_key[64] = {0};
    char fport_key[48] = {0};
    char nwkid_key[64] = {0};
    char deveui_key[64] = {0};
    uint8_t nwkid = 0;

    snprintf(addr_key, sizeof(addr_key), "%s/devaddr/%08X", serv->info.name, addr);
    snprintf(fport_key, sizeof(fport_key), "%s/fport/%u", serv->info.name, fport);
#ifdef BIGENDIAN
    nwkid = (addr >> 25) & 0x7F;   /* Devaddr Format:  31..25(NwkID)  24..0(NwkAddr) */
#else
    nwkid = (addr) & 0x7F;
#endif
    snprintf(nwkid_key, sizeof(nwkid_key), "%s/nwkid/%02X", serv->info.name, nwkid);
    snprintf(deveui_key, sizeof(deveui_key), "%s/deveui/%s", serv->info.name, deveui);

    lgw_log(LOG_INFO, "%s[%s-filter] fport-lv=%d, addr-lv=%d, nwkid-lv=%d, deveui-lv=%d, addr_key=%s, fport_key=%s, nwkid_key=%s, deveui_key=%s\n", INFOMSG, 
                        serv->info.name, serv->filter.fport, serv->filter.devaddr, serv->filter.nwkid, serv->filter.deveui, 
                        addr_key, fport_key, nwkid_key, deveui_key);
    
    switch (serv->filter.fport) {
    case INCLUDE: // 1
        if (lgw_db_key_exist(fport_key)){
            lgw_log(LOG_INFO, "%s[%s-filter] fport filter include\n", INFOMSG, serv->info.name);
            return true;  // filter
        }
        lgw_log(LOG_INFO, "%s[%s-filter] fport filter not include\n", INFOMSG, serv->info.name);
        break;
    case EXCLUDE: // 2
        if (!lgw_db_key_exist(fport_key) && fport>0){
            lgw_log(LOG_INFO, "%s[%s-filter] fport filter exclude\n", INFOMSG, serv->info.name);
            return true;  //filter
        }
        lgw_log(LOG_INFO, "%s[%s-filter] fport filter not exclude\n", INFOMSG, serv->info.name);
        break;
    default:
        lgw_log(LOG_INFO, "%s[%s-filter] fport no filter\n", INFOMSG, serv->info.name);
        break;
    }

    switch(serv->filter.devaddr) {
    case INCLUDE: //1
        if (lgw_db_key_exist(addr_key)){
            lgw_log(LOG_INFO, "%s[%s-filter] devaddr filter include\n", INFOMSG, serv->info.name);
            return true; // filter
        }
        lgw_log(LOG_INFO, "%s[%s-filter] devaddr filter not include\n", INFOMSG, serv->info.name);
        break;
    case EXCLUDE:
        if (!lgw_db_key_exist(addr_key) && addr>0){
            lgw_log(LOG_INFO, "%s[%s-filter] devaddr filter exclude\n", INFOMSG, serv->info.name);
            return true; 
        }
        lgw_log(LOG_INFO, "%s[%s-filter] devaddr filter not exclude\n", INFOMSG, serv->info.name);
        break;
    default:
        lgw_log(LOG_INFO, "%s[%s-filter] devaddr no filter\n", INFOMSG, serv->info.name);
        break;
    }

    switch(serv->filter.nwkid) {
    case INCLUDE: //1
        if (lgw_db_key_exist(nwkid_key)){
            lgw_log(LOG_INFO, "%s[%s-filter] nwkid(%02X) filter include \n", INFOMSG, serv->info.name, nwkid);
            return true; // filter
        }
        lgw_log(LOG_INFO, "%s[%s-filter] nwkid(%02X) filter not include \n", INFOMSG, serv->info.name, nwkid);
        break;
    case EXCLUDE:
        if (!lgw_db_key_exist(nwkid_key) && addr>0){
            lgw_log(LOG_INFO, "%s[%s-filter] nwkid(%02X) filter exclude \n", INFOMSG, serv->info.name, nwkid);
            return true; 
        }
        lgw_log(LOG_INFO, "%s[%s-filter] nwkid(%02X) filter not exclude \n", INFOMSG, serv->info.name, nwkid);
        break;
    default:
        lgw_log(LOG_INFO, "%s[%s-filter] nwkid(%02X) no filter\n", INFOMSG, serv->info.name, nwkid);
        break;
    }

    switch(serv->filter.deveui) {
    case INCLUDE: //1
        if (lgw_db_key_exist(deveui_key)){
            lgw_log(LOG_INFO, "%s[%s-filter] deveui(%s) filter include \n", INFOMSG, serv->info.name, deveui);
            return true; // filter
        }
        lgw_log(LOG_INFO, "%s[%s-filter] deveui(%s) filter not include \n", INFOMSG, serv->info.name, deveui);
        break;
    case EXCLUDE:
        if (!lgw_db_key_exist(deveui_key)){
            lgw_log(LOG_INFO, "%s[%s-filter] deveui(%s) filter exclude \n", INFOMSG, serv->info.name, deveui);
            return true; 
        }
        lgw_log(LOG_INFO, "%s[%s-filter] deveui(%s) filter not exclude \n", INFOMSG, serv->info.name, deveui);
        break;
    default:
        lgw_log(LOG_INFO, "%s[%s-filter] deveui(%s) no filter\n", INFOMSG, serv->info.name, deveui);
        break;
    }
    //lgw_log(LOG_DEBUG, "%s[%s-filter] default: no filter\n", DEBUGMSG, serv->info.name);
    return false;  // no-filter
}

/*
void service_handle_rxpkt(rxpkts_s* rxpkt) {
    serv_s* serv_entry = NULL;
    rxpkts_s* rxpkt_entry = NULL;
    LGW_LIST_TRAVERSE(&GW.serv_list, serv_entry, list) { 
        rxpkt_entry = (rxpkts_s*)lgw_malloc(sizeof(rxpkts_s));
        if (NULL == rxpkt_entry) continue;
        memcpy(rxpkt_entry, rxpkt, sizeof(rxpkts_s));
        LGW_LIST_LOCK(&serv_entry->rxpkts_list);
        LGW_LIST_INSERT_HEAD(&serv_entry->rxpkts_list, rxpkt_entry, list);
        LGW_LIST_UNLOCK(&GW.rxpkts_list);
        sem_post(&serv_entry->thread.sema);
    }
}
*/

void service_start() {
    serv_s* serv_entry;
    LGW_LIST_TRAVERSE(&GW.serv_list, serv_entry, list) { 
        switch (serv_entry->info.type) {
        case semtech:
            semtech_start(serv_entry);
            break;
        case pkt:
            pkt_start(serv_entry);
            break;
        case relay:
            relay_start(serv_entry);
            break;
        case delay:
            delay_start(serv_entry);
            break;
        case mqtt:
            mqtt_start(serv_entry);
            break;
        default:
            break;
        }
    }
}

void service_stop() {
    serv_s* serv_entry;
    LGW_LIST_TRAVERSE(&GW.serv_list, serv_entry, list) { 
        switch (serv_entry->info.type) {
        case semtech:
            semtech_stop(serv_entry);
            break;
        case pkt:
            pkt_stop(serv_entry);
            break;
        case relay:
            relay_stop(serv_entry);
            break;
        case delay:
            delay_stop(serv_entry);
            break;
        case mqtt:
            mqtt_stop(serv_entry);
            break;
        default:
            semtech_stop(serv_entry);
            break;
        }
    }
}

uint16_t crc16(const uint8_t * data, unsigned size) {
    const uint16_t crc_poly = 0x1021;
    const uint16_t init_val = 0x0000;
    uint16_t x = init_val;
    unsigned i, j;

    if (data == NULL)  {
        return 0;
    }

    for (i=0; i<size; ++i) {
        x ^= (uint16_t)data[i] << 8;
        for (j=0; j<8; ++j) {
            x = (x & 0x8000) ? (x<<1) ^ crc_poly : (x<<1);
        }
    }

    return x;
}

