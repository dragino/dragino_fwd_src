/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    Minimum test program for the loragw_i2c module

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* Fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>     /* sigaction */
#include <unistd.h>     /* getopt, access */
#include <time.h>

#include "loragw_i2c.h"
#include "loragw_stts751.h"
#include "loragw_aux.h"
#include "loragw_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define I2C_PORT_STTS751        0x39

#define STTS751_REG_TEMP_H      0x00
#define STTS751_REG_TEMP_L      0x02
#define STTS751_REG_CONF        0x03
#define STTS751_REG_RATE        0x04
#define STTS751_REG_PROD_ID     0xFD
#define STTS751_REG_MAN_ID      0xFE
#define STTS751_REG_REV_ID      0xFF

#define STTS751_0_PROD_ID       0x00
#define STTS751_1_PROD_ID       0x01
#define ST_MAN_ID               0x53

/* -------------------------------------------------------------------------- */
/* --- GLOBAL VARIABLES ----------------------------------------------------- */

/* Signal handling variables */
static int exit_sig = 0; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
static int quit_sig = 0; /* 1 -> application terminates without shutting down the hardware */

//static int i2c_dev = -1;
static int     ts_fd = -1;
static uint8_t ts_addr = 0xFF;

static char i2c_device[16] = "/dev/i2c-0";

/* -------------------------------------------------------------------------- */
/* --- SUBFUNCTIONS DECLARATION --------------------------------------------- */

static void sig_handler(int sigio);
static void usage(void);

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char ** argv)
{
    int i, err;
    static struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
    uint8_t val;
    uint8_t high_byte, low_byte;
    int8_t h;
    float temperature;
    char *pt = NULL;

    pt = getenv("I2C_DEVICE");
    if (NULL != pt) 
        strncpy(i2c_device, getenv("I2C_DEVICE"), sizeof(i2c_device));

    /* Parse command line options */
    while ((i = getopt(argc, argv, "hd:")) != -1) {
        switch (i) {
            case 'h':
                usage();
                return EXIT_SUCCESS;
                break;

            case 'd':
                if (optarg != NULL) {
                    /* TODO */
                }
                break;

            default:
                printf("ERROR: argument parsing options, use -h option for help\n");
                usage();
                return EXIT_FAILURE;
            }
    }

    /* Configure signal handling */
    sigemptyset( &sigact.sa_mask );
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction( SIGQUIT, &sigact, NULL );
    sigaction( SIGINT, &sigact, NULL );
    sigaction( SIGTERM, &sigact, NULL );

    printf( "+++ Start of I2C test program +++\n" );

    for (i = 0; i < (int)(sizeof I2C_PORT_TEMP_SENSOR); i++) {
        ts_addr = I2C_PORT_TEMP_SENSOR[i];
        err = i2c_linuxdev_open(i2c_device, ts_addr, &ts_fd);
        if (err != LGW_I2C_SUCCESS) {
            printf("ERROR: failed to open I2C for temperature sensor on port 0x%02X\n", ts_addr);
            return EXIT_FAILURE;
        }

        err = stts751_configure(ts_fd, ts_addr);
        if (err != LGW_I2C_SUCCESS) {
            //printf("INFO: no temeprature sensor found on port 0x%02X\n", ts_addr);
            i2c_linuxdev_close(ts_fd);
            ts_fd = -1;
        } else {
            printf("INFO: found temperature sensor on port 0x%02X\n", ts_addr);
            break;
        }
    }

    if (i == sizeof I2C_PORT_TEMP_SENSOR) {
        printf("ERROR: no temeprature sensor found.\n");
        return EXIT_FAILURE;
    }

    /* Terminate */
    printf( "+++ End of I2C test program +++\n" );

    //err = i2c_linuxdev_close( i2c_dev );
    err = i2c_linuxdev_close(ts_fd);
    if ( err != 0 )
    {
        printf( "ERROR: failed to close I2C device (err=%i)\n", err );
        return EXIT_FAILURE;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* --- SUBFUNCTIONS DEFINITION ---------------------------------------------- */

static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
        quit_sig = 1;
    } else if((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = 1;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void usage(void) {
    printf("~~~ Library version string~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf(" %s\n", lgw_version_info());
    printf("~~~ Available options ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf(" -h            print this help\n");
    printf(" -d <path>     use Linux I2C device driver\n");
    printf("               => default path: %s \n", i2c_device);
}

/* --- EOF ------------------------------------------------------------------ */
