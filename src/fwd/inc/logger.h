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

#include <stdint.h>
#include <stdio.h>

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

#define NONE         "\033[m"
#define WHITE        "\033[37m"
#define RED          "\033[31m"
#define LIGHT_RED    "\033[1;31m"
#define GREEN        "\033[32m"
#define YELLOW       "\033[33m"
#define BLUE         "\033[34m"
#define INFOMSG      "\033[32m[INFO~]\033[m"
#define WARNMSG      "\033[33m[WARNING~]\033[m"
#define ERRMSG       "\033[1;31m[ERROR~]\033[m"
#define DEBUGMSG     "\033[34m[DEBUG~]\033[m"
#define PKTMSG       "\033[1;32m[PKTS~]\033[m"
#define RELAYMSG     "\033[35m[RELAY]\033[m"
#define STORAGEMSG   "\033[36m[STORAGE]\033[m"

#define MSG(args...) printf(args) /* message that is destined to the user */

#define lgw_msg(args...) printf(args) /* message that is destined to the user */

#define lgw_log(FLAG, fmt, ...)                                       \
            do  {                                                     \
                if (FLAG & GW.log.debug_mask)                         \
                    fprintf(stdout, fmt, ##__VA_ARGS__);              \
                } while (0)

#define MSG_DEBUG(FLAG, fmt, ...)                               

#endif /* _LGW_LOGGER_H */
