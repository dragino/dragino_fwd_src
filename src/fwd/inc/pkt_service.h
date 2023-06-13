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
 */

#ifndef _GWPKT_PROTO_H
#define _GWPKT_PROTO_H

#include "linkedlists.h"

/*!下发的数据结据，下发的格式是： devaddr, txmode, payload format, payload
* txmode: time, imme
* format: txt, hex
**/

#define DEFAULT_PAYLOAD_SIZE        256
#define DEFAULT_DOWN_FPORT          2

#define DNPATH                      "/var/iot/push" 

#define UP                          0
#define DOWN                        1

#define FCNT_GAP                    8

typedef struct _dn_pkt {
	LGW_LIST_ENTRY(_dn_pkt) list;
    char devaddr[16];
    char txmode[8];
    char pdformat[8];
    uint8_t payload[512];
    uint32_t txfreq;
    uint8_t *fopt;
    uint8_t ftype;
    uint8_t mode;
    uint8_t psize;
    uint8_t txdr;
    uint8_t txbw;
    uint8_t txpw;
    uint8_t rxwindow;
    uint8_t txport;
    uint8_t optlen;
} dn_pkt_s;

int pkt_start(serv_s*);
void pkt_stop(serv_s*);

#endif						
