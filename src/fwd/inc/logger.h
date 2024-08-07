/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino Forward -- An opensource lora gateway forward 
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
 * \brief FWD main include file . File version handling , generic functions.
 */

#ifndef __LGW_LOGGER_H
#define __LGW_LOGGER_H

#include <stdio.h>
#include <stdint.h>

#define LOG_INFO        0x01
#define LOG_PKT         0x02
#define LOG_WARNING     0x04
#define LOG_ERROR       0x08
#define LOG_REPORT      0x10
#define LOG_JIT         0x20
#define LOG_JIT_ERROR   0x40
#define LOG_BEACON      0x80
#define LOG_DEBUG       0x100
#define LOG_TIMERSYNC   0x200
#define LOG_MEM         0x400

#define PRINT_SIZE      4096


#ifdef CFG_color
#define INFOMSG      "\033[32m[INFO~]\033[m"
#define WARNMSG      "\033[33m[WARNING~]\033[m"
#define ERRMSG       "\033[1;31m[ERROR~]\033[m"
#define DEBUGMSG     "\033[34m[DEBUG~]\033[m"
#define PKTMSG       "\033[1;32m[PKTS~]\033[m"
#define RELAYMSG     "\033[35m[RELAY]\033[m"
#define STORAGEMSG   "\033[36m[STORAGE]\033[m"
#else
#define INFOMSG      "[INFO~]"
#define WARNMSG      "[WARNING~]"
#define ERRMSG       "[ERROR~]"
#define DEBUGMSG     "[DEBUG~]"
#define PKTMSG       "[PKTS~]"
#define RELAYMSG     "[RELAY]"
#define STORAGEMSG   "[STORAGE]"
#endif

#define MSG(args...) printf(args)

void lgw_log(int FLAG, const char *format, ...);
  
#endif /* _LGW_LOGGER_H */
