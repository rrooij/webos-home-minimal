/* Minimal LVGL 9 configuration. Anything not set here uses LVGL's defaults. */
#ifndef LV_CONF_H
#define LV_CONF_H

/* 32 bit XRGB8888: byte order matches SDL_PIXELFORMAT_ARGB8888 on little-endian ARM/x86. */
#define LV_COLOR_DEPTH 32

#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_USE_OS LV_OS_NONE

#define LV_DEF_REFR_PERIOD 16 /* ~60 fps while animating */
#define LV_DPI_DEF 130

/* Keep decoded icons around instead of re-decoding PNGs on every redraw. */
#define LV_CACHE_DEF_SIZE (48 * 1024 * 1024)
#define LV_IMAGE_HEADER_CACHE_DEF_CNT 128

/* Icons are read from absolute paths: "A:/usr/palm/applications/..." */
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'A'
#define LV_FS_STDIO_PATH ""
#define LV_FS_STDIO_CACHE_SIZE 0

#define LV_USE_LODEPNG 1

#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_24

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
