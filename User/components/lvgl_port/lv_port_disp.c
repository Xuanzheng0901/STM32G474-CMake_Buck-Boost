// #include "lv_port_disp_template.h"
// #include <stdbool.h>
// #include "lvgl.h"
// #include "ssd1306.h"
// #include "FreeRTOS.h"
// #include "task.h"
// #include "queue.h"
// #include "semphr.h"
// #include "stdio.h"
// #include "stm32g4xx_hal.h"
//
// #define MY_DISP_HOR_RES    128
// #define MY_DISP_VER_RES    64
// #define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */
//
// volatile bool disp_flush_enabled = true;
// SemaphoreHandle_t lvgl_mutex;
//
// void disp_enable_update(void)
// {
//     disp_flush_enabled = true;
// }
//
// void disp_disable_update(void)
// {
//     disp_flush_enabled = false;
// }
//
// static void _lvgl_port_transform_monochrome(lv_display_t *display, const lv_area_t *area, uint8_t **color_map)
// {
//     lv_color16_t *src_color_buf = (lv_color16_t *)*color_map;
//     uint8_t *dest_mono_buf = *color_map;
//
//     int x1 = area->x1, x2 = area->x2;
//     int y1 = area->y1, y2 = area->y2;
//
//     for(int y = y1; y <= y2; y++)
//     {
//         for(int x = x1; x <= x2; x++)
//         {
//             lv_color16_t c = src_color_buf[MY_DISP_HOR_RES * y + x];
//             uint8_t gray = (c.red * 3 + c.green * 6 + c.blue) >> 3;
//             bool is_white = (gray > 64);
//
//             /* 垂直映射方式: 先找到字节位置，再设置对应的位 */
//             uint8_t *byte_ptr = dest_mono_buf + MY_DISP_HOR_RES * (y >> 3) + x;
//             uint8_t bit_pos = y % 8;
//
//             if(is_white)
//             {
//                 *byte_ptr &= ~(1 << bit_pos);
//             }
//             else
//             {
//                 *byte_ptr |= (1 << bit_pos);
//             }
//         }
//     }
// }
//
// static void lvgl_port_flush_callback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
// {
//     int offsetx1 = area->x1;
//     int offsetx2 = area->x2;
//     int offsety1 = area->y1;
//     int offsety2 = area->y2;
//
//     _lvgl_port_transform_monochrome(drv, area, &color_map);
//
//     ssd1306_draw(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
//
//     lv_display_flush_ready(drv);
// }
//
// static void lvgl_task_handler(void *arg)
// {
//     (void)arg;
//     while(1)
//     {
//         uint32_t sleep_ms = lv_timer_handler();
//         lv_tick_inc(sleep_ms);
//         vTaskDelay(pdMS_TO_TICKS(sleep_ms));
//         printf("timer beat");
//     }
// }
//
// void lv_log_cb(lv_log_level_t level, const char *buf)
// {
//     printf("\n");
// }
//
//
// void lvgl_lock(void)
// {
//     // xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
// }
//
// void lvgl_unlock(void)
// {
//     // xSemaphoreGive(lvgl_mutex);
// }
//
// void lv_port_disp_init(void)
// {
//     // lvgl_mutex = xSemaphoreCreateMutex();
//     lv_init();
//     lv_log_register_print_cb(lv_log_cb);
//     lv_tick_set_cb(xTaskGetTickCount);
//     lv_display_t *disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
//     lv_display_set_flush_cb(disp, lvgl_port_flush_callback);
//
//     LV_ATTRIBUTE_MEM_ALIGN
//     static uint8_t buf_2_1[MY_DISP_HOR_RES * 32 * BYTE_PER_PIXEL];
//
//     LV_ATTRIBUTE_MEM_ALIGN
//     static uint8_t buf_2_2[MY_DISP_HOR_RES * 32 * BYTE_PER_PIXEL];
//     lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
//
//     xTaskCreate(lvgl_task_handler, "LVGL", 512, NULL, 6, NULL);
//     printf("lvgl init done\n");
// }

#include "lv_port_disp.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "../stm_ssd1306/ssd1306.h"

#define DISP_HOR_RES    128
#define DISP_VER_RES    64
#define DISP_BUF_SIZE   (DISP_HOR_RES * DISP_VER_RES / 8 + 8)

static void disp_init(void);

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

static SemaphoreHandle_t xGuiSemaphore = NULL; // LVGL互斥信号量
volatile bool disp_flush_enabled = true;
static uint8_t disp_buf1[DISP_BUF_SIZE];
static uint8_t disp_buf2[DISP_BUF_SIZE];

static void disp_init(void)
{
    ssd1306_init();
}

void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/**
 * @brief 将缓冲区内容刷新到显示屏
 * @param disp 显示对象指针
 * @param area 刷新区域
 * @param px_map 像素数据指针（I1 格式）
 * @note 由于使用双全缓冲模式，area 始终是整个屏幕区域
 */
static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if(!disp_flush_enabled)
    {
        lv_display_flush_ready(disp);
        return;
    }

    uint8_t *pos = px_map + 8;
    static uint8_t ssd1306_buffer[128 * 8] = {0};
    memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));

    /* 转换格式：水平扫描 → 垂直页模式 */
    for(uint16_t y = 0; y < DISP_VER_RES; y++)
    {
        for(uint16_t x = 0; x < DISP_HOR_RES; x++)
        {
            /* 从 LVGL 缓冲区读取像素 (MSB 优先) */
            uint32_t src_byte_idx = (y * DISP_HOR_RES + x) >> 3;

            // 【修正点】根据 MSB 优先的定义来提取像素
            uint8_t src_bit_offset = x % 8;
            uint8_t pixel = (pos[src_byte_idx] >> (7 - src_bit_offset)) & 0x01;

            /* 写入 SSD1306 缓冲区（页模式）*/
            if(pixel)
            {
                // 这部分逻辑是完美正确的
                uint16_t page = y >> 3;
                uint8_t bit = y % 8;
                uint32_t dst_idx = page * DISP_HOR_RES + x;
                ssd1306_buffer[dst_idx] |= (1 << bit);
            }
        }
    }
    // memset(converter_buffer, 0, sizeof(converter_buffer));
    // for(uint8_t page = 0; page < 8; page++)
    // {
    //     for(uint8_t x = 0; x < 128; x++)
    //     {
    //         for(uint8_t y = 0; y < 8; y++)
    //         {
    //             uint16_t src_index = page * 128 + y * 16 + (x / 8);
    //             uint8_t src_mask = 0x80 >> (x % 8);
    //
    //             if(pos[src_index] & src_mask)
    //             {
    //                 converter_buffer[page * 128 + x] |= (1 << y);
    //             }
    //         }
    //     }
    // }

    ssd1306_draw(ssd1306_buffer);

    /* 通知 LVGL 刷新完成 */
    lv_display_flush_ready(disp);
}

bool lvgl_port_lock(uint32_t timeout_ms)
{
    if(xGuiSemaphore == NULL)
    {
        return false;
    }

    TickType_t ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(xGuiSemaphore, ticks) == pdTRUE;
}

void lvgl_port_unlock(void)
{
    if(xGuiSemaphore != NULL)
    {
        xSemaphoreGive(xGuiSemaphore);
    }
}

void lvgl_task(void *pvParameters)
{
    (void)pvParameters;
    uint32_t uMSToWait = 0;
    // printf("Hello LVGL\n");
    while(1)
    {
        if(lvgl_port_lock(portMAX_DELAY))
        {
            uMSToWait = lv_timer_handler();
            lvgl_port_unlock();
            vTaskDelay(uMSToWait);
        }
    }
}

void lv_port_disp_init(void)
{
    disp_init();

    xGuiSemaphore = xSemaphoreCreateMutex();
    lv_init();
    lv_display_t *disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_resolution(disp, DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(disp, disp_buf1, disp_buf2, DISP_BUF_SIZE, LV_DISPLAY_RENDER_MODE_DIRECT); // 双全缓冲模式
    lv_display_set_flush_cb(disp, disp_flush);
    xTaskCreate(lvgl_task, "LVGL", 1024, NULL, 6, NULL);
}
