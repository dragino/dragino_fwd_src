#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>   /* clock_nanosleep */
#include "logger.h"
#include "uart.h"

/* --- PUBLIC DECLARATION ---------------------------------------- */

static void wait_ms(long a);
static double difftimespec(struct timespec end, struct timespec beginning);

int main(int argc, char* argv[])
{
    int fd, ret, i = 10, len;
    int baude = 9600;
    int timeout = 1000;
    int flow = 0;
    int step = 0;
    uint32_t freq_hz = 923200000U;
    double arg_d = 0.0;
    char buffer[32] = {'\0'};
    char msg[48] = {'\0'};
    long waitms = 1;

    struct timespec s_time;
    struct timespec e_time;

    printf("LBT test utility start...\r\n");

    if (argc < 2) { 
        printf("Need argument, tty_path and tty command\n");
        printf("Usage: %s tty_path freq_hz timeout flow baude waitms step\r\n", argv[0]);
        printf("       %s /dev/ttyUSB4 923200000 1000 0 9600 10 0\r\n", argv[0]);
        printf("       %s /dev/ttyUSB4 \r\n", argv[0]);
        return -1;
    }
    
    if (argc > 2) {
       sscanf(argv[2], "%lf", &arg_d);
       freq_hz = (uint32_t)((arg_d*1e6) + 0.5);
    }

    if (argc > 3) {
       timeout = atoi(argv[3]);
    }

    if (argc > 4) {
       flow = atoi(argv[4]);
    }

    if (argc > 5) {
       baude = atoi(argv[5]);
    }

    if (argc > 6) {
       waitms = atol(argv[6]);
    }

    if (argc > 7) {
       step = atol(argv[7]);
    }

    printf("Timeout set to %d\n", timeout);
    printf("Flow set to %d\n", flow);
    printf("Baude set to %d\n", baude);

    if ((fd = uart_open(argv[1])) < 0) {
        printf("open %s is failed\n",  argv[1]);
    } else {
        printf("open %s is success, fd=%d\n", argv[1], fd);
        ret = uart_config(fd, baude, flow, 9, 9, 9); 
        printf("uart_config retval=%d\n", ret);
        while (i--) {
            printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
            clock_gettime(CLOCK_MONOTONIC, &s_time);
            if (step > 0) 
                freq_hz = freq_hz + i * step;
            len =  snprintf(msg, sizeof(msg), "AT+GETCHANSTAT=%u,-85,6\r\n", freq_hz);
            ret = uart_send(fd, msg, len);
            if (ret < 0)
                printf("(%d) write failed\n", i);
            else 
                printf("(%d) SEND %d bytes => %s", i, len, msg);

            memset(buffer, 0, sizeof(buffer));

            ret = uart_read(fd, buffer, sizeof(buffer), timeout); 

            clock_gettime(CLOCK_MONOTONIC, &e_time);
            printf("(%d) LBT scan use %i ms\n", i, (int)(1000 * difftimespec(e_time, s_time)));

            if (ret > 0) {  
                //printf("read uart buffer: %s \n", buffer);
                if (buffer[0] == 'F')
                    printf("(%d) LBT_DETECT: CHANNEL is FREE \n", i);
                else
                    printf("(%d) LBT_DETECT: CHANNEL is BUSY \n", i);
            }

            
            printf("(%d) Wait %ld ms for next LBT detect\n", i, waitms);
            wait_ms(waitms);
            clock_gettime(CLOCK_MONOTONIC, &e_time);
            printf("(%d) Use total time %d(ms)\n", i, (int)(1000 * difftimespec(e_time, s_time)));
            printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
            printf("\n");
        }
    }
    uart_close(fd);
}

static void wait_ms(long a) {
    struct timespec dly;
    struct timespec rem;

    dly.tv_sec = a / 1000;
    dly.tv_nsec = (a % 1000) * 1000000;

    //DEBUG_PRINTF("NOTE dly: %ld sec %ld ns\n", dly.tv_sec, dly.tv_nsec);

    if((dly.tv_sec > 0) || ((dly.tv_sec == 0) && (dly.tv_nsec > 100000))) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &dly, &rem);
        //DEBUG_PRINTF("NOTE remain: %ld sec %ld ns\n", rem.tv_sec, rem.tv_nsec);
    }
    return;
}

static double difftimespec(struct timespec end, struct timespec beginning) {
    double x;

    x = 1E-9 * (double)(end.tv_nsec - beginning.tv_nsec);
    x += (double)(end.tv_sec - beginning.tv_sec);
    return x;
}
