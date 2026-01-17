#include "LOG.h"
#include <time.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

static log_level_t s_log_level = LOG_VERBOSE;

#define ANSI_RESET  "\033[0m"
#define ANSI_COLOR  "\033[0;%sm"

static uint32_t get_time_ms(void)
{
    return xTaskGetTickCount() * 1000 / configTICK_RATE_HZ;
}

void log_set_level(log_level_t level)
{
    s_log_level = level;
}

void log_write(log_level_t level, const char *tag, const char *format, ...)
{
    if(level > s_log_level)
    {
        return;
    }

    char level_char = ' ';
    const char *color_code = "39"; // Default

    switch(level)
    {
        case LOG_ERROR:
            level_char = 'E';
            color_code = LOG_COLOR_E;
            break;
        case LOG_WARN:
            level_char = 'W';
            color_code = LOG_COLOR_W;
            break;
        case LOG_INFO:
            level_char = 'I';
            color_code = LOG_COLOR_I;
            break;
        case LOG_DEBUG:
            level_char = 'D';
            color_code = LOG_COLOR_D;
            break;
        case LOG_VERBOSE:
            level_char = 'V';
            color_code = LOG_COLOR_V;
            break;
        default:
            break;
    }

    uint32_t timestamp = get_time_ms();

    printf(ANSI_COLOR "%c (%lu) %s: ", color_code, level_char, timestamp, tag);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf(ANSI_RESET "\n");
}
