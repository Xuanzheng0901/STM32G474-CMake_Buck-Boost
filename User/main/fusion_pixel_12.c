/*******************************************************************************
 * Size: 12 px
 * Bpp: 1
 * Opts: --bpp 1 --size 12 --no-compress --stride 1 --align 1 --font fusion-pixel-12px-monospaced-zh_hans.ttf --symbols 电压流功率开机中关过热保护你好设定实际 --range 32-127 --format lvgl -o fusion_pixel_12.c
 ******************************************************************************/

#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif


#ifndef FUSION_PIXEL_12
#define FUSION_PIXEL_12 1
#endif

#if FUSION_PIXEL_12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xfd,

    /* U+0022 "\"" */
    0xb6, 0x80,

    /* U+0023 "#" */
    0x52, 0xbe, 0xa5, 0x7d, 0x4a,

    /* U+0024 "$" */
    0x23, 0xab, 0x46, 0x18, 0xb5, 0x71, 0x0,

    /* U+0025 "%" */
    0x4d, 0x54, 0x42, 0x2a, 0xb2,

    /* U+0026 "&" */
    0x64, 0xa5, 0x44, 0xd6, 0x4d,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x29, 0x49, 0x22, 0x44,

    /* U+0029 ")" */
    0x89, 0x12, 0x4a, 0x50,

    /* U+002A "*" */
    0x25, 0x5d, 0x52, 0x0,

    /* U+002B "+" */
    0x21, 0x3e, 0x42, 0x0,

    /* U+002C "," */
    0x58,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x8, 0x44, 0x22, 0x11, 0x8, 0x84, 0x0,

    /* U+0030 "0" */
    0x74, 0x67, 0x5a, 0xe6, 0x2e,

    /* U+0031 "1" */
    0x27, 0x8, 0x42, 0x10, 0x9f,

    /* U+0032 "2" */
    0x74, 0x42, 0x22, 0x22, 0x1f,

    /* U+0033 "3" */
    0x74, 0x42, 0x60, 0x86, 0x2e,

    /* U+0034 "4" */
    0x32, 0x95, 0x29, 0x7c, 0x42,

    /* U+0035 "5" */
    0xfc, 0x21, 0xe0, 0x86, 0x2e,

    /* U+0036 "6" */
    0x74, 0x61, 0xe8, 0xc6, 0x2e,

    /* U+0037 "7" */
    0xf8, 0x42, 0x21, 0x10, 0x84,

    /* U+0038 "8" */
    0x74, 0x62, 0xe8, 0xc6, 0x2e,

    /* U+0039 "9" */
    0x74, 0x63, 0x17, 0x86, 0x2e,

    /* U+003A ":" */
    0x84,

    /* U+003B ";" */
    0x40, 0x16,

    /* U+003C "<" */
    0x12, 0x48, 0x42, 0x10,

    /* U+003D "=" */
    0xf8, 0x3e,

    /* U+003E ">" */
    0x84, 0x21, 0x24, 0x80,

    /* U+003F "?" */
    0x74, 0x42, 0x22, 0x10, 0x4,

    /* U+0040 "@" */
    0x74, 0x67, 0x5a, 0xd6, 0xf0, 0x70,

    /* U+0041 "A" */
    0x21, 0x14, 0xa7, 0x46, 0x31,

    /* U+0042 "B" */
    0xf4, 0x63, 0xe8, 0xc6, 0x3e,

    /* U+0043 "C" */
    0x74, 0x61, 0x8, 0x42, 0x2e,

    /* U+0044 "D" */
    0xf4, 0x63, 0x18, 0xc6, 0x3e,

    /* U+0045 "E" */
    0xfc, 0x21, 0xe8, 0x42, 0x1f,

    /* U+0046 "F" */
    0xfc, 0x21, 0xe8, 0x42, 0x10,

    /* U+0047 "G" */
    0x74, 0x61, 0x9, 0xc6, 0x2f,

    /* U+0048 "H" */
    0x8c, 0x63, 0xf8, 0xc6, 0x31,

    /* U+0049 "I" */
    0xf9, 0x8, 0x42, 0x10, 0x9f,

    /* U+004A "J" */
    0x8, 0x42, 0x10, 0xc6, 0x2e,

    /* U+004B "K" */
    0x8c, 0xa9, 0x8c, 0x52, 0x51,

    /* U+004C "L" */
    0x84, 0x21, 0x8, 0x42, 0x1f,

    /* U+004D "M" */
    0x8c, 0x77, 0xba, 0xd6, 0x31,

    /* U+004E "N" */
    0x8e, 0x73, 0x5a, 0xce, 0x71,

    /* U+004F "O" */
    0x74, 0x63, 0x18, 0xc6, 0x2e,

    /* U+0050 "P" */
    0xf4, 0x63, 0x1f, 0x42, 0x10,

    /* U+0051 "Q" */
    0x74, 0x63, 0x18, 0xd6, 0x4d,

    /* U+0052 "R" */
    0xf4, 0x63, 0x1f, 0x4a, 0x31,

    /* U+0053 "S" */
    0x74, 0x60, 0xc1, 0x6, 0x2e,

    /* U+0054 "T" */
    0xf9, 0x8, 0x42, 0x10, 0x84,

    /* U+0055 "U" */
    0x8c, 0x63, 0x18, 0xc6, 0x2e,

    /* U+0056 "V" */
    0x8c, 0x62, 0xa5, 0x28, 0x84,

    /* U+0057 "W" */
    0xad, 0x6b, 0x5a, 0xa9, 0x4a,

    /* U+0058 "X" */
    0x8c, 0x54, 0x42, 0x2a, 0x31,

    /* U+0059 "Y" */
    0x8c, 0x54, 0xa2, 0x10, 0x84,

    /* U+005A "Z" */
    0xf8, 0x44, 0x42, 0x22, 0x1f,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0x9c,

    /* U+005C "\\" */
    0x84, 0x10, 0x82, 0x10, 0x42, 0x8, 0x40,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0x3c,

    /* U+005E "^" */
    0x22, 0xa2,

    /* U+005F "_" */
    0xf8,

    /* U+0060 "`" */
    0x90,

    /* U+0061 "a" */
    0x70, 0x5f, 0x18, 0xbc,

    /* U+0062 "b" */
    0x84, 0x3d, 0x18, 0xc6, 0x3e,

    /* U+0063 "c" */
    0x74, 0x61, 0x8, 0xb8,

    /* U+0064 "d" */
    0x8, 0x5f, 0x18, 0xc6, 0x2f,

    /* U+0065 "e" */
    0x74, 0x7f, 0x8, 0xb8,

    /* U+0066 "f" */
    0x19, 0x3e, 0x42, 0x10, 0x84,

    /* U+0067 "g" */
    0x7c, 0x63, 0x18, 0xbc, 0x2e,

    /* U+0068 "h" */
    0x84, 0x2d, 0x98, 0xc6, 0x31,

    /* U+0069 "i" */
    0x20, 0x38, 0x42, 0x10, 0x9f,

    /* U+006A "j" */
    0x10, 0xf1, 0x11, 0x11, 0x1e,

    /* U+006B "k" */
    0x84, 0x23, 0x6c, 0x52, 0x51,

    /* U+006C "l" */
    0xe1, 0x8, 0x42, 0x10, 0x83,

    /* U+006D "m" */
    0xd5, 0x6b, 0x5a, 0xd4,

    /* U+006E "n" */
    0xb6, 0x63, 0x18, 0xc4,

    /* U+006F "o" */
    0x74, 0x63, 0x18, 0xb8,

    /* U+0070 "p" */
    0xf4, 0x63, 0x18, 0xfa, 0x10,

    /* U+0071 "q" */
    0x7c, 0x63, 0x18, 0xbc, 0x21,

    /* U+0072 "r" */
    0xb6, 0x61, 0x8, 0x40,

    /* U+0073 "s" */
    0x74, 0x58, 0x28, 0xb8,

    /* U+0074 "t" */
    0x21, 0x3e, 0x42, 0x10, 0x83,

    /* U+0075 "u" */
    0x8c, 0x63, 0x19, 0xb4,

    /* U+0076 "v" */
    0x8c, 0x54, 0xa2, 0x10,

    /* U+0077 "w" */
    0xad, 0x6b, 0x55, 0x28,

    /* U+0078 "x" */
    0x8a, 0x88, 0x45, 0x44,

    /* U+0079 "y" */
    0x8c, 0x62, 0xa5, 0x10, 0x98,

    /* U+007A "z" */
    0xf8, 0x88, 0x88, 0x7c,

    /* U+007B "{" */
    0x34, 0x44, 0x84, 0x44, 0x43,

    /* U+007C "|" */
    0xff, 0xc0,

    /* U+007D "}" */
    0xc2, 0x22, 0x12, 0x22, 0x2c,

    /* U+007E "~" */
    0x45, 0x44,

    /* U+4E2D "中" */
    0x4, 0x0, 0x83, 0xff, 0xc2, 0x18, 0x43, 0x8,
    0x7f, 0xf8, 0x20, 0x4, 0x0, 0x80, 0x10, 0x0,

    /* U+4F60 "你" */
    0x28, 0x5, 0xfd, 0x20, 0xa8, 0x9c, 0x10, 0x8a,
    0x91, 0x52, 0x49, 0x51, 0x28, 0x21, 0xc, 0x0,

    /* U+4FDD "保" */
    0x2f, 0xc5, 0x9, 0x21, 0x27, 0xec, 0x10, 0x82,
    0x17, 0xfa, 0x1c, 0x45, 0x4b, 0x25, 0x4, 0x0,

    /* U+5173 "关" */
    0x20, 0x82, 0x21, 0xff, 0x2, 0x0, 0x40, 0x8,
    0x3f, 0xf8, 0x50, 0x11, 0x4, 0x13, 0x1, 0x80,

    /* U+529F "功" */
    0x1, 0x1e, 0x20, 0xbf, 0x90, 0x92, 0x12, 0x42,
    0x48, 0x49, 0x91, 0xc2, 0x20, 0x84, 0x63, 0x0,

    /* U+538B "压" */
    0x7f, 0xe8, 0x1, 0x8, 0x21, 0x4, 0x20, 0xbf,
    0x90, 0x82, 0x14, 0x42, 0x48, 0x42, 0xff, 0x80,

    /* U+597D "好" */
    0x27, 0xc4, 0xb, 0xc2, 0x28, 0x85, 0x10, 0xbf,
    0xe4, 0x42, 0x88, 0x21, 0xa, 0x22, 0x1c, 0x0,

    /* U+5B9A "定" */
    0x4, 0x1f, 0xfe, 0x0, 0x80, 0x7, 0xfc, 0x8,
    0x9, 0x1, 0x3e, 0x24, 0xb, 0x82, 0xf, 0x80,

    /* U+5B9E "实" */
    0x4, 0x1f, 0xfe, 0x0, 0xd1, 0x11, 0x20, 0x84,
    0x8, 0x87, 0xff, 0x5, 0x3, 0x13, 0x81, 0x80,

    /* U+5F00 "开" */
    0xff, 0xe4, 0x10, 0x82, 0x10, 0x42, 0x9, 0xff,
    0xc8, 0x21, 0x4, 0x40, 0x88, 0x12, 0x2, 0x0,

    /* U+62A4 "护" */
    0x21, 0x4, 0x13, 0xdf, 0x92, 0x12, 0x42, 0x6f,
    0xf9, 0x1, 0x20, 0x24, 0x4, 0x83, 0xa0, 0x0,

    /* U+673A "机" */
    0x27, 0x84, 0x93, 0xd2, 0x12, 0x42, 0x48, 0xc9,
    0x1d, 0x25, 0x24, 0x24, 0xa4, 0x94, 0xa3, 0x80,

    /* U+6D41 "流" */
    0x82, 0xb, 0xfc, 0x10, 0x44, 0x25, 0xfa, 0x0,
    0x2, 0xa2, 0x54, 0x4a, 0xb1, 0x56, 0x4b, 0x80,

    /* U+70ED "热" */
    0x21, 0x1e, 0xf8, 0x85, 0x10, 0xa3, 0x65, 0xc4,
    0x89, 0x4b, 0x25, 0x0, 0xa, 0x4a, 0x24, 0x80,

    /* U+7387 "率" */
    0x4, 0x1f, 0xfd, 0x11, 0x15, 0x40, 0x40, 0x94,
    0xa7, 0xc8, 0x20, 0xff, 0xe0, 0x80, 0x10, 0x0,

    /* U+7535 "电" */
    0x8, 0x1f, 0xf2, 0x22, 0x44, 0x4f, 0xf9, 0x11,
    0x22, 0x27, 0xfc, 0x8, 0x21, 0x4, 0x1f, 0x80,

    /* U+8BBE "设" */
    0x87, 0x88, 0x90, 0x12, 0x2, 0x4d, 0x8e, 0x80,
    0x13, 0xf2, 0x42, 0x44, 0x8c, 0x61, 0x73, 0x80,

    /* U+8FC7 "过" */
    0x80, 0x88, 0x10, 0x7f, 0x80, 0x4c, 0x88, 0x89,
    0x11, 0x22, 0x4, 0x43, 0x94, 0x2, 0x7f, 0x80,

    /* U+9645 "际" */
    0xe7, 0xd4, 0x2, 0x80, 0x57, 0xfc, 0x11, 0x42,
    0x2a, 0x55, 0x49, 0xe9, 0x32, 0x26, 0xc, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 96, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 96, .box_w = 1, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 96, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 4, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 9, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 16, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 21, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 26, .adv_w = 96, .box_w = 1, .box_h = 3, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 27, .adv_w = 96, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 31, .adv_w = 96, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 35, .adv_w = 96, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 39, .adv_w = 96, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 43, .adv_w = 96, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 44, .adv_w = 96, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 45, .adv_w = 96, .box_w = 1, .box_h = 1, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 46, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 53, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 58, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 68, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 73, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 83, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 88, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 98, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 103, .adv_w = 96, .box_w = 1, .box_h = 6, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 104, .adv_w = 96, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 106, .adv_w = 96, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 110, .adv_w = 96, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 112, .adv_w = 96, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 121, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 127, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 137, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 147, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 157, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 162, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 167, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 172, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 177, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 187, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 192, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 197, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 202, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 212, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 217, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 222, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 227, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 232, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 242, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 247, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 252, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 257, .adv_w = 96, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 261, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 268, .adv_w = 96, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 272, .adv_w = 96, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 274, .adv_w = 96, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 275, .adv_w = 96, .box_w = 2, .box_h = 2, .ofs_x = 2, .ofs_y = 6},
    {.bitmap_index = 276, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 280, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 285, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 294, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 298, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 303, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 308, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 313, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 318, .adv_w = 96, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 323, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 328, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 333, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 337, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 341, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 345, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 350, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 355, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 359, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 363, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 368, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 372, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 376, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 380, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 389, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 393, .adv_w = 96, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 398, .adv_w = 96, .box_w = 1, .box_h = 10, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 400, .adv_w = 96, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 405, .adv_w = 96, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 407, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 423, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 439, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 455, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 471, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 487, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 503, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 519, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 535, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 551, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 567, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 583, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 599, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 615, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 631, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 647, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 663, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 679, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 695, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0x133, 0x1b0, 0x346, 0x472, 0x55e, 0xb50, 0xd6d,
    0xd71, 0x10d3, 0x1477, 0x190d, 0x1f14, 0x22c0, 0x255a, 0x2708,
    0x3d91, 0x419a, 0x4818
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 20013, .range_length = 18457, .glyph_id_start = 96,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 19,
        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};


/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};

extern const lv_font_t fusion_pixel_12;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t fusion_pixel_12 = {
#else
lv_font_t fusion_pixel_12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt, /*Function pointer to get glyph's bitmap*/
    .line_height = 12, /*The maximum line height required by the font*/
    .base_line = 2, /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .static_bitmap = 0,
    .dsc = &font_dsc, /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &fusion_pixel_12,
#endif
    .user_data = NULL,
};


#endif /*#if FUSION_PIXEL_12*/
