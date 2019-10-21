#include <vitasdkkern.h>
#include <taihen.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "main.h"
#include "gui.h"
#include "gui_font_ter-u14b.h"
#include "gui_font_ter-u18b.h"
#include "gui_font_ter-u24b.h"
#include "perf.h"
#include "oc.h"

int vsnprintf(char *s, size_t n, const char *format, va_list arg);

static SceDisplayFrameBuf g_gui_fb = {
    .width  = 960,
    .height = 544,
    .pitch  = 960,
};
static int g_gui_fb_last_width = 960;

static rgba_t *g_gui_buffer;
static SceUID g_gui_buffer_uid = -1;

static const unsigned char *g_gui_font = FONT_TER_U24B;
static unsigned char g_gui_font_width  = 12;
static unsigned char g_gui_font_height = 24;

static float g_gui_font_scale = 1.0f;
static rgba_t g_gui_color_text = {.rgba = {255, 255, 255, 255}};
static rgba_t g_gui_color_bg   = {.rgba = {  0,   0,   0, 255}};

static uint32_t g_gui_input_buttons = 0;

static psvs_gui_menu_control_t g_gui_menu_control = PSVS_GUI_MENUCTRL_CPU;
static psvs_gui_mode_t g_gui_mode = PSVS_GUI_MODE_HIDDEN;
static bool g_gui_mode_changed = false;

#define GUI_CORNERS_XD_RADIUS 9
static const unsigned char GUI_CORNERS_XD[GUI_CORNERS_XD_RADIUS] = {9, 7, 5, 4, 3, 2, 2, 1, 1};

psvs_gui_mode_t psvs_gui_get_mode() {
    return g_gui_mode;
}

static psvs_oc_device_t _psvs_gui_get_device_from_menuctrl(psvs_gui_menu_control_t ctrl) {
    switch (ctrl) {
        case PSVS_GUI_MENUCTRL_CPU: return PSVS_OC_CPU;
        case PSVS_GUI_MENUCTRL_GPU_ES4: return PSVS_OC_GPU_ES4;
        case PSVS_GUI_MENUCTRL_GPU: return PSVS_OC_GPU;
        case PSVS_GUI_MENUCTRL_BUS: return PSVS_OC_BUS;
        case PSVS_GUI_MENUCTRL_GPU_XBAR: return PSVS_OC_GPU_XBAR;
        default: return PSVS_OC_MAX;
    }
    return PSVS_OC_MAX;
}

static psvs_oc_device_t _psvs_gui_get_selected_device() {
    return _psvs_gui_get_device_from_menuctrl(g_gui_menu_control);
}

void psvs_gui_input_check(uint32_t buttons) {
    uint32_t buttons_new = buttons & ~g_gui_input_buttons;

    // Toggle menu
    if (buttons & SCE_CTRL_SELECT) {
        if (buttons_new & SCE_CTRL_UP && g_gui_mode < PSVS_GUI_MODE_MAX - 1) {
            g_gui_mode++; // Show
            g_gui_mode_changed = true;
        } else if (buttons_new & SCE_CTRL_DOWN && g_gui_mode > 0) {
            g_gui_mode--; // Hide
            g_gui_mode_changed = true;
        }
    }
    // In full menu
    else if (g_gui_mode == PSVS_GUI_MODE_FULL) {
        // Move U/D
        if (buttons_new & SCE_CTRL_DOWN && g_gui_menu_control < PSVS_GUI_MENUCTRL_MAX - 1) {
            g_gui_menu_control++;
        } else if (buttons_new & SCE_CTRL_UP && g_gui_menu_control > 0) {
            g_gui_menu_control--;
        }

        psvs_oc_device_t device = _psvs_gui_get_selected_device();

        // In manual freq mode
        if (psvs_oc_get_mode(device) == PSVS_OC_MODE_MANUAL) {
            // Move L/R
            if (buttons_new & SCE_CTRL_RIGHT) {
                psvs_oc_change_manual(device, true);
            } else if (buttons_new & SCE_CTRL_LEFT) {
                psvs_oc_change_manual(device, false);
            }
            // Back to default
            else if (buttons_new & SCE_CTRL_CROSS) {
                psvs_oc_set_mode(device, PSVS_OC_MODE_DEFAULT);
            }
        }
        // In default freq mode
        else {
            if (buttons_new & SCE_CTRL_CROSS) {
                psvs_oc_reset_manual_for_device(device);
                psvs_oc_set_mode(device, PSVS_OC_MODE_MANUAL);
            }
        }
    }

    g_gui_input_buttons = buttons;
}

void psvs_gui_set_framebuf(const SceDisplayFrameBuf *pParam) {
    memcpy(&g_gui_fb, pParam, sizeof(SceDisplayFrameBuf));

    // Set appropriate font
    if (pParam->width <= 640) {
        // 640x368 - Terminus 8x14 Bold
        g_gui_font = FONT_TER_U14B;
        g_gui_font_width = 8;
        g_gui_font_height = 14;
    } else if (pParam->width <= 720) {
        // 720x408 - Terminus 10x18 Bold
        g_gui_font = FONT_TER_U18B;
        g_gui_font_width = 9; // <- trim last col, better scaling
        g_gui_font_height = 18;
    } else {
        // 960x544 - Terminus 12x24 Bold
        g_gui_font = FONT_TER_U24B;
        g_gui_font_width = 12;
        g_gui_font_height = 24;
    }
}

bool psvs_gui_fb_res_changed() {
    bool changed = g_gui_fb_last_width != g_gui_fb.width;
    g_gui_fb_last_width = g_gui_fb.width;
    return changed;
}

bool psvs_gui_mode_changed() {
    bool changed = g_gui_mode_changed;
    g_gui_mode_changed = false;
    return changed;
}

void psvs_gui_set_back_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    g_gui_color_bg.rgba.r = r;
    g_gui_color_bg.rgba.g = g;
    g_gui_color_bg.rgba.b = b;
    g_gui_color_bg.rgba.a = a;
}

void psvs_gui_set_text_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    g_gui_color_text.rgba.r = r;
    g_gui_color_text.rgba.g = g;
    g_gui_color_text.rgba.b = b;
    g_gui_color_text.rgba.a = a;
}

void psvs_gui_set_text_color2(rgba_t color) {
    g_gui_color_text = color;
}

void psvs_gui_set_text_scale(float scale) {
    g_gui_font_scale = scale;
}

static void _psvs_gui_dd_prchar(const char character, int x, int y) {
    rgba_t color = {.rgba = {0, 255, 0, 255}};

    for (int yy = 0; yy < g_gui_font_height * g_gui_font_scale; yy++) {
        int yy_font = yy / g_gui_font_scale;

        uint32_t displacement = x + (y + yy) * g_gui_fb.pitch;
        if (displacement >= g_gui_fb.pitch * g_gui_fb.height)
            return; // out of bounds

        rgba_t *px = (rgba_t *)g_gui_fb.base + displacement;

        for (int xx = 0; xx < g_gui_font_width * g_gui_font_scale; xx++) {
            if (x + xx >= g_gui_fb.width)
                return; // out of bounds

            // Get px 0/1 from osd_font.h
            int xx_font = xx / g_gui_font_scale;
            uint32_t charPos = character * (g_gui_font_height * (((g_gui_font_width - 1) / 8) + 1));
            uint32_t charPosH = charPos + (yy_font * (((g_gui_font_width - 1) / 8) + 1));
            uint8_t charByte = g_gui_font[charPosH + (xx_font / 8)];

            if ((charByte >> (7 - (xx_font % 8))) & 1) {
                ksceKernelMemcpyKernelToUser((uintptr_t)(px + xx), &color, sizeof(rgba_t));
            }
        }
    }
}

void psvs_gui_dd_fps() {
    char buf[3] = "";
    snprintf(buf, 3, "%d", psvs_perf_get_fps());

    for (int i = 0; i < 2; i++) {
        _psvs_gui_dd_prchar(buf[i], 10 + i * g_gui_font_width * g_gui_font_scale, 10);
    }
}

void psvs_gui_clear() {
    for (int i = 0; i < GUI_WIDTH * GUI_HEIGHT; i++)
        g_gui_buffer[i] = g_gui_color_bg;
}

static void _psvs_gui_prchar(const char character, int x, int y) {
    // Draw spaces faster
    if (character == ' ') {
        for (int yy = 0; yy < g_gui_font_height * g_gui_font_scale; yy++) {
            rgba_t *buf = &g_gui_buffer[((y + yy) * GUI_WIDTH) + x];
            for (int xx = 0; xx < g_gui_font_width * g_gui_font_scale; xx++) {
                buf[xx] = g_gui_color_bg;
            }
        }

        return;
    }

    // TODO: rewrite
    for (int yy = 0; yy < g_gui_font_height * g_gui_font_scale; yy++) {
        int yy_font = yy / g_gui_font_scale;

        uint32_t displacement = x + (y + yy) * GUI_WIDTH;
        if (displacement >= GUI_WIDTH * GUI_HEIGHT)
            return; // out of bounds

        rgba_t *px = (rgba_t *)g_gui_buffer + displacement;

        for (int xx = 0; xx < g_gui_font_width * g_gui_font_scale; xx++) {
            if (x + xx >= GUI_WIDTH)
                return; // out of bounds

            // Get px 0/1 from osd_font.h
            int xx_font = xx / g_gui_font_scale;
            uint32_t charPos = character * (g_gui_font_height * (((g_gui_font_width - 1) / 8) + 1));
            uint32_t charPosH = charPos + (yy_font * (((g_gui_font_width - 1) / 8) + 1));
            uint8_t charByte = g_gui_font[charPosH + (xx_font / 8)];

            rgba_t color = ((charByte >> (7 - (xx_font % 8))) & 1) ? g_gui_color_text : g_gui_color_bg;
            *(px + xx) = color;
        }
    }
}

void psvs_gui_print(int x, int y, const char *str) {
    x = (int)(x * (g_gui_fb.width / 960.0f));
    y = (int)(y * (g_gui_fb.height / 544.0f));

    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        _psvs_gui_prchar(str[i], x + i * g_gui_font_width * g_gui_font_scale, y);
    }
}

void psvs_gui_printf(int x, int y, const char *format, ...) {
    char buffer[256] = "";
    va_list va;

    va_start(va, format);
    vsnprintf(buffer, 256, format, va);
    va_end(va);

    psvs_gui_print(x, y, buffer);
}

const char *psvs_gui_units_from_size(int bytes) {
    if (bytes >= 1024 * 1024)
        return "MB";
    else if (bytes >= 1024)
        return "kB";
    else
        return "B";
}

int psvs_gui_value_from_size(int bytes) {
    if (bytes >= 1024 * 1024)
        return bytes / 1024 / 1024;
    else if (bytes >= 1024)
        return bytes / 1024;
    else
        return bytes;
}

rgba_t psvs_gui_scale_color(int value, int min, int max) {
    if (value < min)
        value = min;
    if (value > max)
        value = max;

    int v = ((float)(value - min) / (max - min)) * 255; // 0-255

    rgba_t color;
    color.rgba.r = (v <= 127) ? v * 2 : 255;
    color.rgba.g = (v > 127) ? 255 - ((v - 128) * 2) : 255;
    color.rgba.b = 0;
    color.rgba.a = 255;

    return color;
}

void psvs_gui_draw_osd_template() {
    psvs_gui_set_back_color(0, 0, 0, 255);
    psvs_gui_set_text_color(255, 255, 255, 255);
    psvs_gui_clear();

    // CPU / Temp
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(8, 0), "CPU:");
    psvs_gui_printf(GUI_ANCHOR_RX(10, 16), GUI_ANCHOR_TY(8, 0), "%%    %%    %%    %%");
    psvs_gui_printf(GUI_ANCHOR_RX(10, 6),  GUI_ANCHOR_TY(8, 1), "%%    C");

    // FPS
    psvs_gui_printf(GUI_ANCHOR_LX(10, 3), GUI_ANCHOR_TY(8, 1), "FPS");

    // Clocks
    //psvs_gui_printf(GUI_ANCHOR_LX(10, 11),  GUI_ANCHOR_TY(8, 1), "/");
}

void psvs_gui_draw_osd_cpu() {
    int load;

    // Draw AVG load
    for (int i = 0; i < 4; i++) {
        load = psvs_perf_get_load(i);
        psvs_gui_set_text_color2(psvs_gui_scale_color(load, 0, 100));
        psvs_gui_printf(GUI_ANCHOR_RX(10, 19 - i * 5), GUI_ANCHOR_TY(8, 0), "%3d", load);
    }

    // Draw peak load
    load = psvs_perf_get_peak();
    psvs_gui_set_text_color2(psvs_gui_scale_color(load, 0, 100));
    psvs_gui_printf(GUI_ANCHOR_RX(10, 9), GUI_ANCHOR_TY(8, 1), "%3d", psvs_perf_get_peak());

    // Draw temp
    int temp = psvs_perf_get_temp();
    psvs_gui_set_text_color2(psvs_gui_scale_color(temp, 30, 60));
    psvs_gui_printf(GUI_ANCHOR_RX(10, 3), GUI_ANCHOR_TY(8, 1), "%2d", temp);

    // Draw clocks
    /*
    int freq = kscePowerGetArmClockFrequency(), r2;
    psvs_gui_set_text_color2(psvs_gui_scale_color(500 + 41 - freq, 41, 500));
    psvs_gui_printf(GUI_ANCHOR_LX(10, 8), GUI_ANCHOR_TY(8, 1), "%3d", freq);
    kscePowerGetDeviceCount(&freq, &r2);
    psvs_gui_set_text_color2(psvs_gui_scale_color(222 + 41 - freq, 41, 222));
    psvs_gui_printf(GUI_ANCHOR_LX(10, 12), GUI_ANCHOR_TY(8, 1), "%3d", freq);
    */

    psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_osd_fps() {
    int fps = psvs_perf_get_fps();
    psvs_gui_set_text_color2(psvs_gui_scale_color(30 - fps, 0, 30));
    if (fps > 99)
        psvs_gui_printf(GUI_ANCHOR_LX(10, 0), GUI_ANCHOR_TY(8, 1), "%3d", fps);
    else
        psvs_gui_printf(GUI_ANCHOR_LX(10, 0), GUI_ANCHOR_TY(8, 1), "%2d ", fps);
    psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_template() {
    psvs_gui_set_back_color(0, 0, 0, 255);
    psvs_gui_set_text_color(255, 255, 255, 255);
    psvs_gui_clear();

    // Header
    psvs_gui_set_text_scale(0.5f);
    psvs_gui_printf(GUI_ANCHOR_CX2(13, 0.5f),     GUI_ANCHOR_TY(8, 0), "PSVshell v1.0");
    psvs_gui_printf(GUI_ANCHOR_RX2(10, 10, 0.5f), GUI_ANCHOR_TY(8, 0), "by Electry");
    psvs_gui_set_text_scale(1.0f);

    // CPU / Temp
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(32, 0), "CPU:");
    psvs_gui_printf(GUI_ANCHOR_RX(10, 16), GUI_ANCHOR_TY(32, 0), "%%    %%    %%    %%");
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(32, 1), "Peak:    %%");
    psvs_gui_printf(GUI_ANCHOR_RX(10, 9),  GUI_ANCHOR_TY(32, 1), "Temp:   C");

    // Memory
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(44, 2), "MEM:");
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(44, 3), "VMEM:");
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(44, 4), "PHY:");

    // Menu
    psvs_gui_printf(GUI_ANCHOR_CX(15),     GUI_ANCHOR_BY(10, 5), "CPU [         ]");
    psvs_gui_printf(GUI_ANCHOR_CX(15),     GUI_ANCHOR_BY(10, 4), "ES4 [         ]");
    psvs_gui_printf(GUI_ANCHOR_CX(15),     GUI_ANCHOR_BY(10, 3), "GPU [         ]");
    psvs_gui_printf(GUI_ANCHOR_CX(15),     GUI_ANCHOR_BY(10, 2), "BUS [         ]");
    psvs_gui_printf(GUI_ANCHOR_CX(15),     GUI_ANCHOR_BY(10, 1), "XBR [         ]");
}

void psvs_gui_draw_header() {
    // Draw TITLEID
    psvs_gui_set_text_scale(0.5f);
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0), GUI_ANCHOR_TY(8, 0), "%-9s", g_titleid);
    psvs_gui_set_text_scale(1.0f);
}

void psvs_gui_draw_cpu_section() {
    int load;

    // Draw AVG load
    for (int i = 0; i < 4; i++) {
        load = psvs_perf_get_load(i);
        psvs_gui_set_text_color2(psvs_gui_scale_color(load, 0, 100));
        psvs_gui_printf(GUI_ANCHOR_RX(10, 19 - (i * 5)), GUI_ANCHOR_TY(32, 0), "%3d", load);
    }

    // Draw peak load
    load = psvs_perf_get_peak();
    psvs_gui_set_text_color2(psvs_gui_scale_color(load, 0, 100));
    psvs_gui_printf(GUI_ANCHOR_LX(10, 6), GUI_ANCHOR_TY(32, 1), "%3d", psvs_perf_get_peak());

    // Draw temp
    int temp = psvs_perf_get_temp();
    psvs_gui_set_text_color2(psvs_gui_scale_color(temp, 30, 60));
    psvs_gui_printf(GUI_ANCHOR_RX(10, 3), GUI_ANCHOR_TY(32, 1), "%2d", temp);

    psvs_gui_set_text_color(255, 255, 255, 255);
}

static void _psvs_gui_draw_memory_usage(int line, int total, int free, int limit) {
    if (total <= 0 && free <= 0) {
        psvs_gui_printf(GUI_ANCHOR_RX(10, 16), GUI_ANCHOR_TY(44, line), "          unused");
    }
    else if (total <= limit && total > free) {
        psvs_gui_printf(GUI_ANCHOR_RX(10, 11), GUI_ANCHOR_TY(44, line), "%-2s /     %-2s",
                        psvs_gui_units_from_size(total - free),
                        psvs_gui_units_from_size(total));

        psvs_gui_set_text_color2(psvs_gui_scale_color(total - free, total - (total / 10), total + (total / 10)));
        psvs_gui_printf(GUI_ANCHOR_RX(10, 16), GUI_ANCHOR_TY(44, line), " %3d",
                        psvs_gui_value_from_size(total - free));
        psvs_gui_printf(GUI_ANCHOR_RX(10, 7), GUI_ANCHOR_TY(44, line), " %3d",
                        psvs_gui_value_from_size(total));

        psvs_gui_set_text_color(255, 255, 255, 255);
    }
    else {
        psvs_gui_printf(GUI_ANCHOR_RX(10, 15 + (free >= 1024 ? 1 : 0)),
                        GUI_ANCHOR_TY(44, line),
                        "         %s free",
                        psvs_gui_units_from_size(free));
        psvs_gui_set_text_color(0, 255, 0, 255);
        psvs_gui_printf(GUI_ANCHOR_RX(10, 11 + (free >= 1024 ? 1 : 0)),
                        GUI_ANCHOR_TY(44, line),
                        " %3d",
                        psvs_gui_value_from_size(free));
        psvs_gui_set_text_color(255, 255, 255, 255);
    }
}

void psvs_gui_draw_memory_section() {
    psvs_memory_t *mem = psvs_perf_get_memusage();

    _psvs_gui_draw_memory_usage(2, mem->main_total, mem->main_free, 512 * 1024 * 1024);
    _psvs_gui_draw_memory_usage(3, mem->cdram_total, mem->cdram_free, 128 * 1024 * 1024);
    _psvs_gui_draw_memory_usage(4, mem->phycont_total, mem->phycont_free, 26 * 1024 * 1024);
}

static void _psvs_gui_draw_menu_item(int lines, int clock, psvs_gui_menu_control_t menuctrl) {
    if (g_gui_menu_control == menuctrl) {
        psvs_gui_set_text_color(0, 200, 255, 255);
        psvs_gui_printf(GUI_ANCHOR_CX(19),                        GUI_ANCHOR_BY(10, lines), ">");
        psvs_gui_printf(GUI_ANCHOR_CX(19) + GUI_ANCHOR_LX(0, 18), GUI_ANCHOR_BY(10, lines), "<");
        psvs_gui_set_text_color(255, 255, 255, 255);
    } else {
        psvs_gui_printf(GUI_ANCHOR_CX(19),                        GUI_ANCHOR_BY(10, lines), " ");
        psvs_gui_printf(GUI_ANCHOR_CX(19) + GUI_ANCHOR_LX(0, 18), GUI_ANCHOR_BY(10, lines), " ");
    }

    // Highlight freq if in manual mode
    if (psvs_oc_get_mode(_psvs_gui_get_device_from_menuctrl(menuctrl)) == PSVS_OC_MODE_MANUAL) {
        psvs_gui_set_text_color(0, 200, 255, 255);
    }
    psvs_gui_printf(GUI_ANCHOR_CX(15) + GUI_ANCHOR_LX(0, 6),  GUI_ANCHOR_BY(10, lines), "%3d MHz", clock);
    psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_menu() {
    int r1, r2;
    _kscePowerGetGpuEs4ClockFrequency(&r1, &r2);

    _psvs_gui_draw_menu_item(5, _kscePowerGetArmClockFrequency(), PSVS_GUI_MENUCTRL_CPU);
    _psvs_gui_draw_menu_item(4, r1, PSVS_GUI_MENUCTRL_GPU_ES4);
    _psvs_gui_draw_menu_item(3, _kscePowerGetGpuClockFrequency(), PSVS_GUI_MENUCTRL_GPU);
    _psvs_gui_draw_menu_item(2, _kscePowerGetBusClockFrequency(), PSVS_GUI_MENUCTRL_BUS);
    _psvs_gui_draw_menu_item(1, _kscePowerGetGpuXbarClockFrequency(), PSVS_GUI_MENUCTRL_GPU_XBAR);
}

int psvs_gui_init() {
    int size = (GUI_WIDTH * GUI_HEIGHT * sizeof(rgba_t) + 0xfff) & ~0xfff;
    g_gui_buffer_uid = ksceKernelAllocMemBlock("psvs_gui", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, size, NULL);
    if (g_gui_buffer_uid < 0) {
        return g_gui_buffer_uid;
    }

    ksceKernelGetMemBlockBase(g_gui_buffer_uid, (void **)&g_gui_buffer);

    return 0;
}

void psvs_gui_deinit() {
    if (g_gui_buffer_uid >= 0)
        ksceKernelFreeMemBlock(g_gui_buffer_uid);
}

void psvs_gui_cpy() {
    int height = (g_gui_mode == PSVS_GUI_MODE_OSD) ? GUI_OSD_HEIGHT : GUI_HEIGHT;

    int w = (int)(GUI_WIDTH * (g_gui_fb.width / 960.0f));
    int h = (int)(height * (g_gui_fb.height / 544.0f));
    int x = (g_gui_mode == PSVS_GUI_MODE_OSD) ? 10 : (g_gui_fb.width / 2) - (w / 2);
    int y = (g_gui_mode == PSVS_GUI_MODE_OSD) ? 10 : (g_gui_fb.height / 2) - (h / 2);

    for (int line = 0; line < h; line++) {
        int xd = 0;
        int xd_line = line * (544.0f / g_gui_fb.height);

        // Top corners
        if (xd_line < GUI_CORNERS_XD_RADIUS) {
            xd = GUI_CORNERS_XD[xd_line] * (g_gui_fb.width / 960.0f);
        }

        // Bottom corners
        if (xd_line >= height - GUI_CORNERS_XD_RADIUS) {
            xd = GUI_CORNERS_XD[height - xd_line - 1] * (g_gui_fb.width / 960.0f);
        }

        int off = ((line + y) * g_gui_fb.pitch + x + xd);
        ksceKernelMemcpyKernelToUser(
            (uintptr_t)&((rgba_t *)g_gui_fb.base)[off],
            &((rgba_t *)g_gui_buffer)[line * GUI_WIDTH + xd],
            sizeof(rgba_t) * (w - xd*2));
    }
}
