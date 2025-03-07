#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "logger.h"
#include "gwcfg.h"

#define LOG_BUFFER_SIZE 4096
#define LOG_TIME_SIZE 32
#define LOG_FILE_PATH_MAX 256

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE* log_file = NULL;
static char log_buffer[LOG_BUFFER_SIZE];
static int log_level = LOG_INFO;  // 默认日志级别
static char log_file_path[LOG_FILE_PATH_MAX] = {0};

// 日志级别字符串
static const char* log_level_str[] = {
    "EMERG",
    "ALERT", 
    "CRIT",
    "ERROR",
    "WARN",
    "NOTICE",
    "INFO",
    "DEBUG"
};

DECLARE_GW;

// 初始化日志系统
int lgw_log_init(const char* file_path, int level) {
    if (file_path) {
        strncpy(log_file_path, file_path, LOG_FILE_PATH_MAX - 1);
        log_file = fopen(file_path, "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", file_path);
            return -1;
        }
    }
    
    log_level = level;
    return 0;
}

// 关闭日志系统
void lgw_log_cleanup(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

// 获取当前时间字符串
static void get_time_str(char* time_str, size_t size) {
    time_t now;
    struct tm* tm_info;
    
    time(&now);
    tm_info = localtime(&now);
    strftime(time_str, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// 主日志函数
void lgw_log(int flag, const char *format, ...) {
    if (!format || !(flag & GW.log.debug_mask)) {
        return;
    }

    pthread_mutex_lock(&log_mutex);

    char time_str[LOG_TIME_SIZE];
    va_list args;
    int log_level = flag & LOG_LEVEL_MASK;
    
    // 获取时间
    get_time_str(time_str, sizeof(time_str));
    
    // 格式化日志头
    int header_len = snprintf(log_buffer, LOG_BUFFER_SIZE,
                            "[%s][%s][%lu] ",
                            time_str,
                            log_level_str[log_level],
                            (unsigned long)pthread_self());
                            
    if (header_len < 0 || header_len >= LOG_BUFFER_SIZE) {
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    
    // 格式化日志消息
    va_start(args, format);
    int msg_len = vsnprintf(log_buffer + header_len,
                           LOG_BUFFER_SIZE - header_len,
                           format,
                           args);
    va_end(args);
    
    if (msg_len < 0) {
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    
    // 确保字符串以换行符结束
    int total_len = header_len + msg_len;
    if (total_len >= LOG_BUFFER_SIZE) {
        total_len = LOG_BUFFER_SIZE - 1;
    }
    
    if (total_len > 0 && log_buffer[total_len - 1] != '\n') {
        if (total_len < LOG_BUFFER_SIZE - 1) {
            log_buffer[total_len] = '\n';
            log_buffer[total_len + 1] = '\0';
        } else {
            log_buffer[LOG_BUFFER_SIZE - 2] = '\n';
            log_buffer[LOG_BUFFER_SIZE - 1] = '\0';
        }
    }
    
    // 输出到控制台
    if (flag >= log_level) {
        fputs(log_buffer, stdout);
        fflush(stdout);
    }
    
    // 输出到日志文件
    if (log_file && flag >= log_level) {
        fputs(log_buffer, log_file);
        fflush(log_file);
    }
    
    pthread_mutex_unlock(&log_mutex);
}

// 设置日志级别
void lgw_log_set_level(int level) {
    log_level = level;
}

// 获取当前日志级别
int lgw_log_get_level(void) {
    return log_level;
}  

