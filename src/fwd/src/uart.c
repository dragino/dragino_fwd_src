#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>

#include "fwd.h"
#include "uart.h"

static int safe_read(int fd, char *vptr, int len);

int uart_open(const char* path)
{
    int fd;
    if (path[0] == 0) return -1;
    fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
    //fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd == -1) {
        //lgw_log(LOG_ERROR, "[TTY] uart open %s failed!", path);
        return -1;
    }

    /*!>
    if(fcntl(fd, F_SETFL, 0) < 0) {        
        lgw_log(LOG_ERROR, "[LBT-TTY] fcntl setfl(0) failed!");
        return -1;
    }
    */

    return fd;
}

int uart_config(int fd, int baude, int c_flow, int bits, int parity, int stop)
{
    struct termios uart;

    if(tcgetattr(fd, &uart) != 0) {
        fprintf(stderr, "[LBT-TTY] tcgetattr failed!(fd=%d)", fd);
        return -1;
    }

    switch(baude) {
        case 9600:
            cfsetispeed(&uart,B9600);
            cfsetospeed(&uart,B9600);
            break;
        case 19200:
            cfsetispeed(&uart,B19200);
            cfsetospeed(&uart,B19200);
            break;
        case 38400:
            cfsetispeed(&uart,B38400);
            cfsetospeed(&uart,B38400);
            break;
        case 115200:
            cfsetispeed(&uart,B115200);
            cfsetospeed(&uart,B115200);
            break;
        default:
            cfsetispeed(&uart,B9600);
            cfsetospeed(&uart,B9600);
            break;
    }


    switch(c_flow) {
        case 0:     //不进行硬件流控制
            uart.c_cflag &= ~CRTSCTS;          
            break;
        case 1:     //进行硬件流控制
            uart.c_cflag |= CRTSCTS;          
            break;
        case 2:     //进行软件流控制
            uart.c_cflag |= (IXON | IXOFF | IXANY);        
            break;
        default:   // none
            break;
    }

    switch(bits) {
        case 5:
            uart.c_cflag &= ~CSIZE;     //屏蔽其他标志位
            uart.c_cflag |= CS5;        //数据位为5位
            break;
        case 6:
            uart.c_cflag &= ~CSIZE;
            uart.c_cflag |= CS6;
            break;
        case 7:
            uart.c_cflag &= ~CSIZE;
            uart.c_cflag |= CS7;
            break;
        case 8:
            uart.c_cflag &= ~CSIZE;
            uart.c_cflag |= CS8;
            break;
        default:  // 8bit
            uart.c_cflag &= ~CSIZE;
            uart.c_cflag |= CS8;
            break;
            break;
    }

    switch(parity) {
        case 0:
            uart.c_cflag &= ~PARENB;    //PARENB：产生奇偶校验
            uart.c_cflag &= ~INPCK;     //INPCK：使奇偶校验起作用
            break;
        case 1:
            uart.c_cflag &= ~PARENB;
            uart.c_cflag &= ~CSTOPB;        //使用两位停止位
            break;
        case 2:
            uart.c_cflag |= PARENB;
            uart.c_cflag |= PARODD;         //使用奇校验
            uart.c_cflag |= INPCK;
            uart.c_cflag |= ISTRIP;         //使字符串剥离第八个字符，即校验位
            break;
        case 3:
            uart.c_cflag |= PARENB;
            uart.c_cflag &= ~PARODD;        //非奇校验，即偶校验
            uart.c_cflag |= INPCK;
            uart.c_cflag |= ISTRIP;
            break;
        default: //none
            break;
    }

    switch(stop) {
        case 1:
            uart.c_cflag &= ~CSTOPB;    //CSTOPB：使用一位停止位
            break;
        case 2:
            uart.c_cflag |= CSTOPB;
            break;
        default:
            uart.c_cflag &= ~CSTOPB;    //CSTOPB：使用一位停止位
            break;
            break;
    }

    uart.c_oflag &= ~OPOST;             //OPOST:表示数据经过处理后输出
     
    uart.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG );   //使串口工作在原始模式下
    uart.c_cc[VTIME] = 0;                               //设置等待时间为0
    uart.c_cc[VMIN] = 1;                                //设置最小接受字符为1
    tcflush(fd, TCIFLUSH);                              //清空输入缓冲区

    if (tcsetattr(fd, TCSANOW, &uart) != 0) {            //激活配置
        fprintf(stderr, "[LBT-TTY] tcgetattr failed!(fd=%d)", fd);
        return -1;
    }

    return 0;
}

static int safe_read(int fd, char* vptr, int len)
{
    size_t left;
    left = len;
    ssize_t nread;
    char *ptr;
    ptr = vptr;

    while(left > 0) {
        if ((nread = read(fd, ptr, left)) < 0) {
            if (errno == EINTR) {
                nread = 0;
            } else if(nread == 0) {
                break;
            } else if (nread < 0) {
                //lgw_log(LOG_DEBUG, "DEBUG~ [TTY] read from uart (%s)\n", strerror(errno));
                break;
            }
            //lgw_log(LOG_DEBUG, "DEBUG~ [TTY] read from uart (%s)\n", strerror(errno));
        }
        left -= nread;
        ptr += nread;
    }
    return (len - left);
}

int uart_read(int fd, char* r_buf, int lenth, int timeout_ms)
{
    int ret;
    fd_set rfds;
    struct timeval tv;
    ssize_t cnt = 0;

    /*!>将读文件描述符加入描述符集合*/
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    /*!>设置超时为timout_ms, 默认 1s */
    tv.tv_sec = timeout_ms/1000;
    tv.tv_usec = (timeout_ms - tv.tv_sec * 1000) * 1000;
    
    ret = select(fd + 1, &rfds, NULL, NULL, &tv);
    switch (ret) {
        case -1:
            fprintf(stderr, "~DEBUG [TTY-UART] select error!\n");
            break;
        case 0:
            fprintf(stderr, "~DEBUG [TTY-UART] select time over!\n");
            break;
        default:
            cnt = safe_read(fd, r_buf, lenth);
            return cnt;
    }

    tcflush(fd, TCOFLUSH);

    return -1;
}

int uart_send(int fd, char* data, int len)
{

    if ((fd == -1) || (NULL == data)) return -1;

    int remain = len;
    int writed = 0;

    tcflush(fd, TCIFLUSH);

    while (remain > 0) {
        len = write(fd, data + writed, remain);
        if (len == 0) return -1;
        remain -= len;
        writed += len;
    }

    return writed;
}

int uart_read_bak(int fd, char* buf, int len, int timeout_ms)
{
    int ret;
    size_t  rsum = 0;
    ret = 0;
    fd_set rset;
    struct timeval tv;

    while (rsum < len)
    {
        tv.tv_sec = timeout_ms/1000;
        tv.tv_usec = (timeout_ms - tv.tv_sec * 1000) * 1000;
        FD_ZERO(&rset);
        FD_SET(fd, &rset);
        ret = select(fd + 1, &rset, NULL, NULL, &tv);
        if (ret <= 0) {
            if (ret == 0) {
                fprintf(stderr, "DEBUG~ [TTY] read from uart timeout\n");
                return -1;
            }

            if (errno == EINTR) {
                //printf(LOG_DEBUG, "~DEBUG [TTY] read from uart (%s)\n", strerror(errno));
                continue;
            }

            //printf(LOG_DEBUG, "~DEBUG [TTY] read from uart (%s)\n", strerror(errno));
            return -errno;

        } else {
            ret = read(fd, buf + rsum, len - rsum);
            if (ret < 0) {
                return ret;
            } else {
                rsum += ret;
            }
        }
    }

    return rsum;
}


int uart_close(int fd)
{
    if (fd != -1)
        close(fd);
    return 0;
}

