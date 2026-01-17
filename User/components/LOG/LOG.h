//
// Created by Pluviophile on 2026/1/16.
// Thanks Espressif.
//


#ifndef G474_1_LOG_H
#define G474_1_LOG_H

#include <stdio.h>
#include <stdarg.h>

typedef enum {
    LOG_NONE, /* 不输出 */
    LOG_ERROR, /* 错误 (Red) */
    LOG_WARN, /* 警告 (Yellow) */
    LOG_INFO, /* 信息 (Green) */
    LOG_DEBUG, /* 调试 (Default/White) */
    LOG_VERBOSE /* 详细 (Gray) */
} log_level_t;

void log_set_level(log_level_t level);

void log_write(log_level_t level, const char *tag, const char *format, ...);

#define LOG_COLOR_E "31"  /* Red */
#define LOG_COLOR_W "33"  /* Yellow */
#define LOG_COLOR_I "32"  /* Green */
#define LOG_COLOR_D "39"  /* Default */
#define LOG_COLOR_V "90"  /* Gray */

/*
 *  设置编译时的日志过滤级别。
 */
#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL LOG_VERBOSE
#endif

#define LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter, format

#define LOGE(tag, format, ...) do { if (LOG_LOCAL_LEVEL >= LOG_ERROR) log_write(LOG_ERROR, tag, format, ##__VA_ARGS__); } while(0)
#define LOGW(tag, format, ...) do { if (LOG_LOCAL_LEVEL >= LOG_WARN)  log_write(LOG_WARN,  tag, format, ##__VA_ARGS__); } while(0)
#define LOGI(tag, format, ...) do { if (LOG_LOCAL_LEVEL >= LOG_INFO)  log_write(LOG_INFO,  tag, format, ##__VA_ARGS__); } while(0)
#define LOGD(tag, format, ...) do { if (LOG_LOCAL_LEVEL >= LOG_DEBUG) log_write(LOG_DEBUG, tag, format, ##__VA_ARGS__); } while(0)
#define LOGV(tag, format, ...) do { if (LOG_LOCAL_LEVEL >= LOG_VERBOSE) log_write(LOG_VERBOSE, tag, format, ##__VA_ARGS__); } while(0)


#endif //G474_1_LOG_H
