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
 * \brief lora packages simulation
 */

#ifndef __GHOST_H_
#define __GHOST_H_

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS AND FIELDS ------------------------------------------ */

/* Constants */
#define DEFAULT_LORA_BW             125     /* LoRa modulation bandwidth, kHz */
#define DEFAULT_LORA_SF             7       /* LoRa SF */
#define DEFAULT_LORA_CR             "4/5"   /* LoRa CR */
#define DEFAULT_FSK_FDEV            25      /* FSK frequency deviation */
#define DEFAULT_FSK_BR              50      /* FSK bitrate */
#define DEFAULT_LORA_PREAMBLE_SIZE  8       /* LoRa preamble size */
#define PUSH_TIMEOUT_MS             100

/* The total number of buffer bytes equals: (GHST_RX_BUFFSIZE + GHST_TX_BUFFSIZE) * GHST_NM_RCV */
#define GHST_RX_BUFFSIZE     240	/* Size of buffer held for receiving packets  */
#define GHST_TX_BUFFSIZE     320	/* Size of buffer held for sending packets  */

#define GHST_NB_PKT                12

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/* Call this to start/stop the server that communicates with the ghost node server. */
bool ghost_start(const char *ghost_addr, const char *ghost_port, const char *gwid);
void ghost_stop(void);

/* Call this to pull data from the receive buffer for ghost nodes.. */
int ghost_get(int max_pkt, struct lgw_pkt_rx_s *pkt_data);

#endif

/* --- EOF ------------------------------------------------------------------ */
