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
#include "profile.h"
#include "power.h"

int vsnprintf(char *s, size_t n, const char *format, va_list arg);

static SceDisplayFrameBuf g_gui_fb = {
    .width  = 960,
    .height = 544,
    .pitch  = 960,
};
static int g_gui_fb_last_width = 960;
static float g_gui_fb_w_ratio = 1.0f;
static float g_gui_fb_h_ratio = 1.0f;

static int g_gui_width = GUI_WIDTH;
static int g_gui_height = GUI_HEIGHT;

static uint8_t burn_off_x = 0;
static uint8_t burn_off_y = 0;

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

static bool g_gui_lazydraw_batt = false;
static bool g_gui_lazydraw_memusage = false;

#define GUI_CORNERS_XD_RADIUS 9
static const unsigned char GUI_CORNERS_XD[GUI_CORNERS_XD_RADIUS] = {9, 7, 5, 4, 3, 2, 2, 1, 1};

static const rgba_t WHITE = {.rgba = {.r = 255, .g = 255, .b = 255, .a = 255}};
static const rgba_t BLACK = {.rgba = {.r = 0, .g = 0, .b = 0, .a = 255}};
static const rgba_t FPS_COLOR = {.rgba = {.r = 0, .g = 255, .b = 0, .a = 255}};

psvs_gui_mode_t psvs_gui_get_mode() {
    return g_gui_mode;
}

static psvs_oc_device_t _psvs_gui_get_device_from_menuctrl(psvs_gui_menu_control_t ctrl) {
    switch (ctrl) {
        case PSVS_GUI_MENUCTRL_CPU:      return PSVS_OC_DEVICE_CPU;
        case PSVS_GUI_MENUCTRL_GPU_ES4:  return PSVS_OC_DEVICE_GPU_ES4;
        case PSVS_GUI_MENUCTRL_BUS:      return PSVS_OC_DEVICE_BUS;
        case PSVS_GUI_MENUCTRL_GPU_XBAR: return PSVS_OC_DEVICE_GPU_XBAR;
        default: return PSVS_OC_DEVICE_MAX;
    }
    return PSVS_OC_DEVICE_MAX;
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

        switch (g_gui_mode)
        {
        case PSVS_GUI_MODE_OSD:
            g_gui_width = GUI_WIDTH;
            g_gui_height = GUI_OSD_HEIGHT;
            break;
        case PSVS_GUI_MODE_OSD2:
            g_gui_width = GUI_OSD2_WIDTH;
            g_gui_height = GUI_OSD2_HEIGHT;
            break;
        default:
            g_gui_width = GUI_WIDTH;
            g_gui_height = GUI_HEIGHT;
            break;
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

        // Profile label
        if (g_gui_menu_control == PSVS_GUI_MENUCTRL_PROFILE) {
            if (buttons_new & SCE_CTRL_CROSS) {
                bool global = buttons & GUI_GLOBAL_PROFILE_BUTTON_MOD;
                if ((!global && psvs_oc_has_changed()) || !psvs_profile_exists(global)) {
                    psvs_profile_save(global);
                } else {
                    psvs_profile_delete(global);
                }
            }
        }
        // Freq change
        else {
            psvs_oc_device_t device = _psvs_gui_get_selected_device();

            // In manual freq mode
            if (psvs_oc_get_mode(device) == PSVS_OC_MODE_MANUAL) {
                // Move L/R
                if (buttons_new & SCE_CTRL_RIGHT) {
                    psvs_oc_change(device, true);
                } else if (buttons_new & SCE_CTRL_LEFT) {
                    psvs_oc_change(device, false);
                }
                // Back to default
                else if (buttons_new & SCE_CTRL_CROSS) {
                    psvs_oc_set_mode(device, PSVS_OC_MODE_DEFAULT);
                }
                // Enable auto freq for CPU
                else if (device == PSVS_OC_DEVICE_CPU && buttons_new & SCE_CTRL_CIRCLE) {
                    psvs_oc_set_mode(device, PSVS_OC_MODE_AUTO);
                }
            }
            // In auto freq mode
            else if (psvs_oc_get_mode(device) == PSVS_OC_MODE_AUTO) {
                // Move L/R
                if (buttons_new & SCE_CTRL_RIGHT) {
                    psvs_oc_change_max_freq(device, true);
                } else if (buttons_new & SCE_CTRL_LEFT) {
                    psvs_oc_change_max_freq(device, false);
                }
                // Change power plan
                if (buttons_new & SCE_CTRL_LTRIGGER) {
                    psvs_oc_raise_power_plan(false, device);
                }
                else if (buttons_new & SCE_CTRL_RTRIGGER) {
                    psvs_oc_raise_power_plan(true, device);
                }
                // Back to default
                else if (buttons_new & SCE_CTRL_CROSS) {
                    psvs_oc_set_mode(device, PSVS_OC_MODE_DEFAULT);
                }
            }
            // In default freq mode
            else {
                if (buttons_new & SCE_CTRL_CROSS) {
                    psvs_oc_reset(device);
                    psvs_oc_set_mode(device, PSVS_OC_MODE_MANUAL);                  
                }
                if (buttons_new & SCE_CTRL_CIRCLE && device == PSVS_OC_DEVICE_CPU) {
                    psvs_oc_reset(device);
                    psvs_oc_set_mode(device, PSVS_OC_MODE_AUTO);                  
                }
            }
        }
    }

    g_gui_input_buttons = buttons;
}

void psvs_gui_set_framebuf(const SceDisplayFrameBuf *pParam) {
    memcpy(&g_gui_fb, pParam, sizeof(SceDisplayFrameBuf));

    g_gui_fb_w_ratio = pParam->width / 960.0f;
    g_gui_fb_h_ratio = pParam->height / 544.0f;

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
        // 960x544 or more - Terminus 12x24 Bold
        g_gui_font = FONT_TER_U24B;
        g_gui_font_width = 12;
        g_gui_font_height = 24;
    }
}

bool psvs_gui_fb_res_changed() {
    bool changed = g_gui_fb_last_width != g_gui_fb.width;
    if (changed) {
        g_gui_lazydraw_batt = true;
        g_gui_lazydraw_memusage = true;
    }

    g_gui_fb_last_width = g_gui_fb.width;
    return changed;
}

bool psvs_gui_mode_changed() {
    bool changed = g_gui_mode_changed;
    if (changed) {
        g_gui_lazydraw_batt = true;
        g_gui_lazydraw_memusage = true;
    }

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
                *(px + xx) = FPS_COLOR;
            }
        }
    }
}

void psvs_gui_dd_fps() {
    char buf[4] = "";
    snprintf(buf, 4, "%d", psvs_perf_get_fps());
    size_t len = strlen(buf);

    uint32_t dacr;
    DACR_UNRESTRICT(dacr);

    for (int i = 0; i < len; i++) {
        _psvs_gui_dd_prchar(buf[i], 10 + i * g_gui_font_width * g_gui_font_scale, 10);
    }

    DACR_RESET(dacr);
}

void psvs_gui_clear() {
    for (int i = 0; i < g_gui_width * g_gui_height; i++)
        g_gui_buffer[i] = g_gui_color_bg;
}

static void _psvs_gui_prchar(const char character, int x, int y) {
    // Draw spaces faster
    if (character == ' ') {
        for (int yy = 0; yy < g_gui_font_height * g_gui_font_scale; yy++) {
            rgba_t *buf = &g_gui_buffer[((y + yy) * g_gui_width) + x];
            for (int xx = 0; xx < g_gui_font_width * g_gui_font_scale; xx++) {
                buf[xx] = g_gui_color_bg;
            }
        }

        return;
    }

    // TODO: rewrite
    for (int yy = 0; yy < g_gui_font_height * g_gui_font_scale; yy++) {
        int yy_font = yy / g_gui_font_scale;

        uint32_t displacement = x + (y + yy) * g_gui_width;
        if (displacement >= g_gui_width * g_gui_height)
            return; // out of bounds

        rgba_t *px = (rgba_t *)g_gui_buffer + displacement;

        for (int xx = 0; xx < g_gui_font_width * g_gui_font_scale; xx++) {
            if (x + xx >= g_gui_width)
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
    x = GUI_RESCALE_X(x);
    y = GUI_RESCALE_Y(y);

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

static void _psvs_gui_draw_battery_template(int x, int y) {
    x = GUI_RESCALE_X(x);
    y = GUI_RESCALE_Y(y);
    int w = GUI_RESCALE_X(GUI_BATT_SIZE_W);
    int h = GUI_RESCALE_Y(GUI_BATT_SIZE_H);

    rgba_t *px = (rgba_t *)g_gui_buffer + (y * g_gui_width) + x;
    int xx, yy;

    for (xx = 0; xx < w; xx++) {
        // top
        *(px + xx)
            = *(px + g_gui_width + xx) = WHITE;
        // bottom
        *(px + (h * g_gui_width) + xx)
            = *(px + ((h - 1) * g_gui_width) + xx) = WHITE;
    }

    for (yy = 0; yy < h; yy++) {
        // left
        *(px + (yy * g_gui_width))
            = *(px + (yy * g_gui_width) + 1) = WHITE;
        // right
        if (yy < h / 3 || yy > h - (h / 3)) {
            *(px + (yy * g_gui_width) + (w - 1))
                = *(px + (yy * g_gui_width) + (w - 2)) = WHITE;
        } else {
            *(px + (yy * g_gui_width) + (w - 1) + (h / 5))
                = *(px + (yy * g_gui_width) + (w - 2) + (h / 5)) = WHITE;
        }
    }

    // dzindzik
    for (xx = 0; xx < (h / 5) + 2; xx++) {
        // top
        *(px + (g_gui_width * (h / 3 - 1)) + (w - 2) + xx)
            = *(px + (g_gui_width * (h / 3)) + (w - 2) + xx) = WHITE;
        // bottom
        *(px + (g_gui_width * (h - (h / 3))) + (w - 2) + xx)
            = *(px + (g_gui_width * (h - (h / 3) + 1)) + (w - 2) + xx) = WHITE;
    }
}

static void _psvs_gui_draw_battery(int x, int y, int state, bool is_charging, rgba_t color) {
    x = GUI_RESCALE_X(x);
    y = GUI_RESCALE_Y(y);
    int w = GUI_RESCALE_X(GUI_BATT_SIZE_W);
    int h = GUI_RESCALE_Y(GUI_BATT_SIZE_H);

    rgba_t *px = (rgba_t *)g_gui_buffer + (y * g_gui_width) + x;
    int state_x = state > 95 ? w : (state * (w - 4) / 95) + 1;
    int xx, yy;

    for (xx = 2; xx < (w - 2); xx++) {
        for (yy = 2; yy < (h - 1); yy++) {
            if (xx <= state_x) {
                *(px + (g_gui_width * yy) + xx) = color;
            } else {
                *(px + (g_gui_width * yy) + xx) = BLACK;
            }
        }
    }

    // dzindzik
    for (xx = 0; xx < (h / 5); xx++) {
        for (yy = 2; yy < (h / 3) + 2; yy++) {
            if (state > 95) {
                *(px + (g_gui_width * ((h / 3 - 1) + yy)) + w + xx - 2) = color;
            } else {
                *(px + (g_gui_width * ((h / 3 - 1) + yy)) + w + xx - 2) = BLACK;
            }
        }
    }

    if (is_charging) {
        for (xx = 0; xx < (h / 3); xx++)
            *(px + (g_gui_width * (h / 2)) + (w / 5) + xx) = WHITE;
        for (yy = 0; yy < (h / 3); yy++)
            *(px + (g_gui_width * ((h / 3) + yy + 1)) + (w / 5) + (h / 6)) = WHITE;
    }
}

// OSD mode

void psvs_gui_draw_osd_template() {
    psvs_gui_set_back_color(0, 0, 0, 255);
    psvs_gui_set_text_color(255, 255, 255, 255);
    psvs_gui_clear();

    // CPU
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(8, 0), "CPU:");
    psvs_gui_printf(GUI_ANCHOR_RX(10, 16), GUI_ANCHOR_TY(8, 0), "%%    %%    %%    %%");
    //psvs_gui_printf(GUI_ANCHOR_LX(10, 10), GUI_ANCHOR_TY(10, 1), "%%");
    psvs_gui_printf(GUI_ANCHOR_LX(10, 10), GUI_ANCHOR_TY(10, 1), "MHz");

    // FPS
    psvs_gui_printf(GUI_ANCHOR_LX(10, 3), GUI_ANCHOR_TY(10, 1), "FPS");

    // Battery
    psvs_gui_printf(GUI_ANCHOR_RX(20 + GUI_BATT_SIZE_W, 1), GUI_ANCHOR_TY(10, 1), "%%");
    _psvs_gui_draw_battery_template(GUI_ANCHOR_RX(14 + GUI_BATT_SIZE_W, 0), GUI_ANCHOR_TY(13, 1));

    // System Consumption
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(12, 2), "Consumption:");
    psvs_gui_printf(GUI_ANCHOR_RX(10, 2),  GUI_ANCHOR_TY(12, 2), "mW");
}

void psvs_gui_draw_osd_cpu() {
    int val;

    // Draw AVG load
    for (int i = 0; i < 4; i++) {
        val = psvs_perf_get_load(i);
        psvs_gui_set_text_color2(psvs_gui_scale_color(val, 0, 100));
        psvs_gui_printf(GUI_ANCHOR_RX(10, 19 - i * 5), GUI_ANCHOR_TY(8, 0), "%3d", val);
    }

    // Draw peak load
    //val = psvs_perf_get_peak();
    // Draw cpu freq
    val = psvs_oc_get_freq(PSVS_OC_DEVICE_CPU);
    //psvs_gui_set_text_color2(psvs_gui_scale_color(val, 0, 100));
    psvs_gui_set_text_color2(psvs_gui_scale_color(val, 41, 500));
    psvs_gui_printf(GUI_ANCHOR_LX(8, 7), GUI_ANCHOR_TY(10, 1), "%3d", val);

    psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_osd_batt() {
    psvs_battery_t *batt = psvs_perf_get_batt();
    if (!batt->_has_changed && !g_gui_lazydraw_batt)
        return;

    batt->_has_changed = false;
    g_gui_lazydraw_batt = false;

    // Draw battery percentage
    rgba_t color = psvs_gui_scale_color(60 - batt->percent, 0, 100);
    psvs_gui_set_text_color2(color);
    psvs_gui_printf(GUI_ANCHOR_RX(20 + GUI_BATT_SIZE_W, 4), GUI_ANCHOR_TY(10, 1), "%3d", batt->percent);

    // Draw battery indicator
    color.rgba.r = (int)(color.rgba.r * 0.75f);
    color.rgba.g = (int)(color.rgba.g * 0.75f);
    color.rgba.b = (int)(color.rgba.b * 0.75f);
    _psvs_gui_draw_battery(GUI_ANCHOR_RX(14 + GUI_BATT_SIZE_W, 0), GUI_ANCHOR_TY(13, 1), batt->percent, batt->is_charging, color);

    // Draw system power consumption
    color = psvs_gui_scale_color(batt->power_cons, 500, 5000);
    psvs_gui_set_text_color2(color);

    if (abs(batt->power_cons) < 10000) {
        int len = 6 + (batt->power_cons < 0 ? 1 : 0);

        psvs_gui_printf(GUI_ANCHOR_RX(12, len), GUI_ANCHOR_TY(12, 2), "%4d", batt->power_cons);
    }
    psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_osd_fps() {
    int fps = psvs_perf_get_fps();

    psvs_gui_set_text_color2(psvs_gui_scale_color(30 - fps, 0, 30));
    if (fps > 99)
        psvs_gui_printf(GUI_ANCHOR_LX(10, 0), GUI_ANCHOR_TY(10, 1), "%3d", fps);
    else
        psvs_gui_printf(GUI_ANCHOR_LX(10, 0), GUI_ANCHOR_TY(10, 1), "%2d ", fps);

    psvs_gui_set_text_color(255, 255, 255, 255);
}

// OSD2 mode

void psvs_gui_draw_osd2_template() {
    //psvs_gui_set_back_color(50, 50, 50, 255);
    psvs_gui_set_back_color(0, 0, 0, 255);  // Black background to avoid OLED burn
    psvs_gui_set_text_color(255, 255, 255, 255);
    psvs_gui_clear();

    // Battery and System Consumption
    psvs_gui_set_text_color(255, 155, 135, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(4, 0), "BATT:");
    psvs_gui_set_text_color(255, 255, 255, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(10, 9), GUI_ANCHOR_TY(4, 0), "%%");
    psvs_gui_printf(GUI_ANCHOR_LX(12, 15), GUI_ANCHOR_TY(4, 0), "W");
    psvs_gui_set_text_color(196, 155, 207, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(12, 16), GUI_ANCHOR_TY(4, 0), " | ");
    psvs_gui_set_text_color(255, 255, 255, 255);

    // GPU
    psvs_gui_set_text_color(74, 232, 130, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(10, 19), GUI_ANCHOR_TY(4, 0), "GPU:");
    psvs_gui_set_text_color(255, 255, 255, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(12, 27), GUI_ANCHOR_TY(4, 0), "MHz");
    psvs_gui_printf(GUI_ANCHOR_LX(12, 34), GUI_ANCHOR_TY(4, 0), "MB");
    psvs_gui_set_text_color(196, 155, 207, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(12, 36), GUI_ANCHOR_TY(4, 0), " | ");
    psvs_gui_set_text_color(255, 255, 255, 255);

    // CPU
    psvs_gui_set_text_color(116, 217, 252, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(10, 39), GUI_ANCHOR_TY(4, 0), "CPU:");
    psvs_gui_set_text_color(255, 255, 255, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(12, 47), GUI_ANCHOR_TY(4, 0), "MHz");
    psvs_gui_printf(GUI_ANCHOR_LX(10, 54), GUI_ANCHOR_TY(4, 0), "%%");
    psvs_gui_set_text_color(196, 155, 207, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(10, 55), GUI_ANCHOR_TY(4, 0), " | ");
    psvs_gui_set_text_color(255, 255, 255, 255);

    // RAM:
    psvs_gui_set_text_color(255, 184, 218, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(10, 58), GUI_ANCHOR_TY(4, 0), "RAM:");
    psvs_gui_set_text_color(255, 255, 255, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(12, 66), GUI_ANCHOR_TY(4, 0), "MB");
    psvs_gui_set_text_color(196, 155, 207, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(12, 68), GUI_ANCHOR_TY(4, 0), " | ");
    psvs_gui_set_text_color(255, 255, 255, 255);

    // FPS
    psvs_gui_set_text_color(255, 172, 128, 255);
    psvs_gui_printf(GUI_ANCHOR_LX(10, 71), GUI_ANCHOR_TY(4, 0), "FPS:");
    psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_osd2_cpu() {
    int val;

    // Draw peak load
    val = psvs_perf_get_peak();
    //psvs_gui_set_text_color2(psvs_gui_scale_color(val, 0, 100));
    psvs_gui_printf(GUI_ANCHOR_LX(10, 51), GUI_ANCHOR_TY(4, 0), "%3d", val);

    // Draw cpu freq
    val = psvs_oc_get_freq(PSVS_OC_DEVICE_CPU);
    //psvs_gui_set_text_color2(psvs_gui_scale_color(val, 41, 500));
    psvs_gui_printf(GUI_ANCHOR_LX(10, 44), GUI_ANCHOR_TY(4, 0), "%3d", val);

    //psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_osd2_gpu() {
    int val = psvs_oc_get_freq(PSVS_OC_DEVICE_GPU_ES4);
    //psvs_gui_set_text_color2(psvs_gui_scale_color(val, 41, 222));
    psvs_gui_printf(GUI_ANCHOR_LX(10, 24), GUI_ANCHOR_TY(4, 0), "%3d", val);
    //psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_osd2_batt() {
    psvs_battery_t *batt = psvs_perf_get_batt();
    if (!batt->_has_changed && !g_gui_lazydraw_batt)
        return;

    batt->_has_changed = false;
    g_gui_lazydraw_batt = false;

    // Draw battery percentage
    //rgba_t color = psvs_gui_scale_color(60 - batt->percent, 0, 100);
    //psvs_gui_set_text_color2(color);
    psvs_gui_printf(GUI_ANCHOR_LX(10, 6), GUI_ANCHOR_TY(4, 0), "%3d", batt->percent);
    
    //color = psvs_gui_scale_color(batt->power_cons, 500, 5000);
    //psvs_gui_set_text_color2(color);

    int watts = batt->power_cons / 1000;
    int watts_decimal = (abs(batt->power_cons) - watts * 1000) / 100;

    // Draw system power consumption
    if (abs(watts) < 10) {
        int len = 3 + (watts >= 10);

        psvs_gui_printf(GUI_ANCHOR_LX(10, 14 - len), GUI_ANCHOR_TY(4, 0), "%2d.%d", watts, watts_decimal);
    }
    else if (abs(watts) < 100) {
        psvs_gui_printf(GUI_ANCHOR_LX(10, 11), GUI_ANCHOR_TY(4, 0), "%3d", watts);
    }
    //psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_osd2_fps() {
    int fps = psvs_perf_get_fps();

    //psvs_gui_set_text_color2(psvs_gui_scale_color(30 - fps, 0, 30));
    psvs_gui_printf(GUI_ANCHOR_LX(10, 75), GUI_ANCHOR_TY(4, 0), "%3d", fps);

    //psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_osd2_mem() {
    psvs_memory_t *mem = psvs_perf_get_memusage();
    if (!mem->_has_changed && !g_gui_lazydraw_memusage)
        return;

    mem->_has_changed = false;
    g_gui_lazydraw_memusage = false;

    int used_ram = (mem->main_total - mem->main_free) / (1024 * 1024);
    int used_vram = (mem->cdram_total - mem->cdram_free) / (1024 * 1024);

    //psvs_gui_set_text_color2(psvs_gui_scale_color(used_vram, 0, 128));
    psvs_gui_printf(GUI_ANCHOR_LX(10, 31), GUI_ANCHOR_TY(4, 0), "%3d", used_vram);
    //psvs_gui_set_text_color2(psvs_gui_scale_color(used_ram, 0, 512));
    psvs_gui_printf(GUI_ANCHOR_LX(10, 63), GUI_ANCHOR_TY(4, 0), "%3d", used_ram);
    //psvs_gui_set_text_color(255, 255, 255, 255);
}

// Full mode

void psvs_gui_draw_template() {
    psvs_gui_set_back_color(0, 0, 0, 255);
    psvs_gui_set_text_color(255, 255, 255, 255);
    psvs_gui_clear();

    // Header
    psvs_gui_set_text_scale(0.5f);
    psvs_gui_printf(GUI_ANCHOR_CX2(18, 0.5f),     GUI_ANCHOR_TY(8, 0), PSVS_VERSION_STRING);
    psvs_gui_printf(GUI_ANCHOR_RX2(10, 10, 0.5f), GUI_ANCHOR_TY(8, 0), "by Electry");
    psvs_gui_set_text_scale(1.0f);

    // Batt
    psvs_gui_printf(GUI_ANCHOR_RX(10, 9),  GUI_ANCHOR_TY(32, 0), "C");
    psvs_gui_printf(GUI_ANCHOR_RX(14 + 6 + GUI_BATT_SIZE_W, 1), GUI_ANCHOR_TY(32, 0), "%%");
    _psvs_gui_draw_battery_template(GUI_ANCHOR_RX(14 + GUI_BATT_SIZE_W, 0), GUI_ANCHOR_TY(35, 0));

    // CPU
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(44, 1), "CPU:");
    psvs_gui_printf(GUI_ANCHOR_RX(10, 16), GUI_ANCHOR_TY(44, 1), "%%    %%    %%    %%");
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(44, 2), "Peak:");
    psvs_gui_printf(GUI_ANCHOR_RX(10, 1),  GUI_ANCHOR_TY(44, 2), "%%");

    // Memory
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(56, 3), "MEM:");
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(56, 4), "VMEM:");
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(56, 5), "PHY:");

    // Menu
    psvs_gui_printf(GUI_ANCHOR_CX(15),     GUI_ANCHOR_BY(10, 5), "CPU [         ]");
    psvs_gui_printf(GUI_ANCHOR_CX(15),     GUI_ANCHOR_BY(10, 4), "ES4 [         ]");
    psvs_gui_printf(GUI_ANCHOR_CX(15),     GUI_ANCHOR_BY(10, 3), "BUS [         ]");
    psvs_gui_printf(GUI_ANCHOR_CX(15),     GUI_ANCHOR_BY(10, 2), "XBR [         ]");
}

void psvs_gui_draw_header() {
    // Draw TITLEID
    psvs_gui_set_text_scale(0.5f);
    psvs_gui_printf(GUI_ANCHOR_LX(10, 0), GUI_ANCHOR_TY(8, 0), "%-9s", g_titleid);
    psvs_gui_set_text_scale(1.0f);
}

void psvs_gui_draw_batt_section() {
    // PSTV
    if (g_is_dolce) {
        psvs_gui_printf(GUI_ANCHOR_LX(10, 0), GUI_ANCHOR_TY(32, 0), "A/C");
        return;
    }

    psvs_battery_t *batt = psvs_perf_get_batt();
    if (!batt->_has_changed && !g_gui_lazydraw_batt)
        return;

    batt->_has_changed = false;
    g_gui_lazydraw_batt = false;

    // Draw temp
    psvs_gui_set_text_color2(psvs_gui_scale_color(batt->temp, 30, 60));
    psvs_gui_printf(GUI_ANCHOR_RX(10, 11), GUI_ANCHOR_TY(32, 0), "%2d", batt->temp);

    psvs_gui_set_text_color(255, 255, 255, 255);

    // Draw battery lifetime
    if (batt->is_charging) {
        psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(32, 0), "Charging   ");
    } else if (batt->lt_hours == 0 && batt->lt_minutes <= 1) {
        psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(32, 0), "Calibrating");
    } else {
        psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(32, 0), "  %s h   m   ", batt->lt_hours >= 10 ? " " : "");
        psvs_gui_printf(GUI_ANCHOR_LX(10, 0),  GUI_ANCHOR_TY(42, 0), "~");

        psvs_gui_set_text_color2(psvs_gui_scale_color((2 * 60) - ((batt->lt_hours * 60) + batt->lt_minutes), 0, 2 * 60));
        psvs_gui_printf(GUI_ANCHOR_LX(10, 2),  GUI_ANCHOR_TY(32, 0), "%d", batt->lt_hours);
        psvs_gui_printf(GUI_ANCHOR_LX(10, batt->lt_hours >= 10 ? 6 : 5),  GUI_ANCHOR_TY(32, 0), "%02d", batt->lt_minutes);
    }

    // Draw battery percentage
    rgba_t color = psvs_gui_scale_color(60 - batt->percent, 0, 100);
    psvs_gui_set_text_color2(color);
    psvs_gui_printf(GUI_ANCHOR_RX(20 + GUI_BATT_SIZE_W, 4), GUI_ANCHOR_TY(32, 0), "%3d", batt->percent);

    // Draw battery indicator
    color.rgba.r = (int)(color.rgba.r * 0.75f);
    color.rgba.g = (int)(color.rgba.g * 0.75f);
    color.rgba.b = (int)(color.rgba.b * 0.75f);
    _psvs_gui_draw_battery(GUI_ANCHOR_RX(14 + GUI_BATT_SIZE_W, 0), GUI_ANCHOR_TY(35, 0), batt->percent, batt->is_charging, color);

}

void psvs_gui_draw_cpu_section() {
    int load;

    // Draw AVG load
    for (int i = 0; i < 4; i++) {
        load = psvs_perf_get_load(i);
        psvs_gui_set_text_color2(psvs_gui_scale_color(load, 0, 100));
        psvs_gui_printf(GUI_ANCHOR_RX(10, 19 - (i * 5)), GUI_ANCHOR_TY(44, 1), "%3d", load);
    }

    // Draw peak load
    load = psvs_perf_get_peak();
    psvs_gui_set_text_color2(psvs_gui_scale_color(load, 0, 100));
    psvs_gui_printf(GUI_ANCHOR_RX(10, 4), GUI_ANCHOR_TY(44, 2), "%3d", load);

    psvs_gui_set_text_color(255, 255, 255, 255);
}

static void _psvs_gui_draw_memory_usage(int line, int total, int free, int limit) {
    if (total <= 0 && free <= 0) {
        psvs_gui_printf(GUI_ANCHOR_RX(10, 16), GUI_ANCHOR_TY(56, line), "          unused");
    }
    else if (total <= limit && total > free) {
        psvs_gui_printf(GUI_ANCHOR_RX(10, 11), GUI_ANCHOR_TY(56, line), "%-2s /     %-2s",
                        psvs_gui_units_from_size(total - free),
                        psvs_gui_units_from_size(total));

        psvs_gui_set_text_color2(psvs_gui_scale_color(total - free, total - (total / 10), total + (total / 10)));
        psvs_gui_printf(GUI_ANCHOR_RX(10, 16), GUI_ANCHOR_TY(56, line), " %3d",
                        psvs_gui_value_from_size(total - free));
        psvs_gui_printf(GUI_ANCHOR_RX(10, 7), GUI_ANCHOR_TY(56, line), " %3d",
                        psvs_gui_value_from_size(total));

        psvs_gui_set_text_color(255, 255, 255, 255);
    }
    else {
        psvs_gui_printf(GUI_ANCHOR_RX(10, 15 + (free >= 1024 ? 1 : 0)),
                        GUI_ANCHOR_TY(56, line),
                        "         %s free",
                        psvs_gui_units_from_size(free));
        psvs_gui_set_text_color(0, 255, 0, 255);
        psvs_gui_printf(GUI_ANCHOR_RX(10, 11 + (free >= 1024 ? 1 : 0)),
                        GUI_ANCHOR_TY(56, line),
                        " %3d",
                        psvs_gui_value_from_size(free));
        psvs_gui_set_text_color(255, 255, 255, 255);
    }
}

void psvs_gui_draw_memory_section() {
    psvs_memory_t *mem = psvs_perf_get_memusage();
    if (!mem->_has_changed && !g_gui_lazydraw_memusage)
        return;

    mem->_has_changed = false;
    g_gui_lazydraw_memusage = false;

    _psvs_gui_draw_memory_usage(3, mem->main_total, mem->main_free, 512 * 1024 * 1024);
    _psvs_gui_draw_memory_usage(4, mem->cdram_total, mem->cdram_free, 128 * 1024 * 1024);
    _psvs_gui_draw_memory_usage(5, mem->phycont_total, mem->phycont_free, 26 * 1024 * 1024);
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

    // Highlight freq if in manual mode (blue)
    if (psvs_oc_get_mode(_psvs_gui_get_device_from_menuctrl(menuctrl)) == PSVS_OC_MODE_MANUAL) {
        psvs_gui_set_text_color(0, 200, 255, 255);
    }
    // Highlight freq if in auto mode (red)
    else if (psvs_oc_get_mode(_psvs_gui_get_device_from_menuctrl(menuctrl)) == PSVS_OC_MODE_AUTO) {
        switch (psvs_oc_get_power_plan(_psvs_gui_get_device_from_menuctrl(menuctrl)))
        {
            case PSVS_POWER_PLAN_SAVER:
                psvs_gui_set_text_color(51, 204, 51, 255);
                break;
            case PSVS_POWER_PLAN_BALANCED:
                psvs_gui_set_text_color(255, 213, 0, 255);
                break;
            case PSVS_POWER_PLAN_PERFORMANCE:
                psvs_gui_set_text_color(255, 0, 0, 255);
                break;
            
            default:
                break;
        }
    }
    psvs_gui_printf(GUI_ANCHOR_CX(15) + GUI_ANCHOR_LX(0, 6),  GUI_ANCHOR_BY(10, lines), "%3d MHz", clock);
    psvs_gui_set_text_color(255, 255, 255, 255);
}

void psvs_gui_draw_menu() {
    _psvs_gui_draw_menu_item(5, psvs_oc_get_max_freq(PSVS_OC_DEVICE_CPU), PSVS_GUI_MENUCTRL_CPU);
    _psvs_gui_draw_menu_item(4, psvs_oc_get_freq(PSVS_OC_DEVICE_GPU_ES4), PSVS_GUI_MENUCTRL_GPU_ES4);
    _psvs_gui_draw_menu_item(3, psvs_oc_get_freq(PSVS_OC_DEVICE_BUS), PSVS_GUI_MENUCTRL_BUS);
    _psvs_gui_draw_menu_item(2, psvs_oc_get_freq(PSVS_OC_DEVICE_GPU_XBAR), PSVS_GUI_MENUCTRL_GPU_XBAR);

    // Draw profile label separately
    bool show_global = g_gui_input_buttons & GUI_GLOBAL_PROFILE_BUTTON_MOD;
    bool save = (!show_global && psvs_oc_has_changed()) || !psvs_profile_exists(show_global);

    if (save) {
        if (show_global)
            psvs_gui_printf(GUI_ANCHOR_CX(18), GUI_ANCHOR_BY(10, 1), "   save default   ");
        else
            psvs_gui_printf(GUI_ANCHOR_CX(18), GUI_ANCHOR_BY(10, 1), "   save profile   ");
    } else {
        if (show_global)
            psvs_gui_printf(GUI_ANCHOR_CX(18), GUI_ANCHOR_BY(10, 1), "  delete default  ");
        else
            psvs_gui_printf(GUI_ANCHOR_CX(18), GUI_ANCHOR_BY(10, 1), "  delete profile  ");
    }
    if (g_gui_menu_control == PSVS_GUI_MENUCTRL_PROFILE) {
        psvs_gui_set_text_color(0, 200, 255, 255);
        psvs_gui_printf(GUI_ANCHOR_CX(save ? 16 : 18), GUI_ANCHOR_BY(10, 1), ">");
        psvs_gui_printf(GUI_ANCHOR_CX(save ? 16 : 18) + GUI_ANCHOR_LX(0, save ? 15 : 17),
                        GUI_ANCHOR_BY(10, 1), "<");
        psvs_gui_set_text_color(255, 255, 255, 255);
    } else {
        psvs_gui_printf(GUI_ANCHOR_CX(save ? 16 : 18), GUI_ANCHOR_BY(10, 1), " ");
        psvs_gui_printf(GUI_ANCHOR_CX(save ? 16 : 18) + GUI_ANCHOR_LX(0, save ? 15 : 17),
                        GUI_ANCHOR_BY(10, 1), " ");
    }
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
    int w = GUI_RESCALE_X(g_gui_width);
    int h = GUI_RESCALE_Y(g_gui_height);
    int x, y;

    switch (g_gui_mode)
    {
    case PSVS_GUI_MODE_OSD:
        x = 10;
        y = 10;
        break;
    case PSVS_GUI_MODE_OSD2:
        x = 0;
        y = 0;
        break;
    default:
        x = (g_gui_fb.width / 2) - (w / 2);
        y = (g_gui_fb.height / 2) - (h / 2);
        break;
    }

    uint32_t dacr;
    DACR_UNRESTRICT(dacr);

    for (int line = 0; line < h; line++) {
        int xd = 0;
        int xd_line = line;
        if (g_gui_fb.height < 544.0f)
            xd_line = xd_line * (544.0f / g_gui_fb.height);

        // Top corners
        if (xd_line < GUI_CORNERS_XD_RADIUS) {
            xd = GUI_RESCALE_X(GUI_CORNERS_XD[xd_line]);
        }

        // Bottom corners
        else if (xd_line >= g_gui_height - GUI_CORNERS_XD_RADIUS) {
            xd = GUI_RESCALE_X(GUI_CORNERS_XD[g_gui_height - xd_line - 1]);
        }

        int off = ((line + y) * g_gui_fb.pitch + x + xd);

        void *src = &((rgba_t *)g_gui_buffer)[line * g_gui_width + xd];
        void *dest = &((rgba_t *)g_gui_fb.base)[off];
        int size = sizeof(rgba_t) * (w - xd*2);

        memcpy(dest, src, size);
    }

    DACR_RESET(dacr);
}

void psvs_gui_change_bunr_off()
{
    burn_off_y = (burn_off_x + burn_off_y) % (GUI_BURN_OFF * 2);
    burn_off_x = GUI_BURN_OFF - burn_off_x;

    g_gui_lazydraw_memusage = true;
    g_gui_lazydraw_batt = true;
}
