#ifndef _GUI_H_
#define _GUI_H_

// scaling done internally
#define GUI_WIDTH  308
#define GUI_HEIGHT 344

#define GUI_OSD_HEIGHT 64

#define GUI_BATT_SIZE_W 32
#define GUI_BATT_SIZE_H 16

#define GUI_FONT_W 12
#define GUI_FONT_H 24

#define GUI_ANCHOR_LX(off, len) (off + (len) * GUI_FONT_W)
#define GUI_ANCHOR_RX(off, len) (GUI_WIDTH - (off) - (len) * GUI_FONT_W)
#define GUI_ANCHOR_RX2(off, len, scale) (GUI_WIDTH - (off) - (len) * GUI_FONT_W * (scale))

#define GUI_ANCHOR_TY(off, lines) (off + (lines) * GUI_FONT_H)
#define GUI_ANCHOR_BY(off, lines) (GUI_HEIGHT - (off) - (lines) * GUI_FONT_H)
#define GUI_ANCHOR_BY2(off, lines, scale) (GUI_HEIGHT - (off) - (lines) * GUI_FONT_H * (scale))

#define GUI_ANCHOR_CX(len) (GUI_WIDTH / 2 - ((len) * GUI_FONT_W) / 2)
#define GUI_ANCHOR_CX2(len, scale) (GUI_WIDTH / 2 - ((len) * GUI_FONT_W * (scale)) / 2)
#define GUI_ANCHOR_CY(lines) (GUI_HEIGHT / 2 - ((lines) * GUI_FONT_H) / 2)

#define GUI_RESCALE_X(x) (int)((x) * g_gui_fb_w_ratio)
#define GUI_RESCALE_Y(y) (int)((y) * g_gui_fb_h_ratio)

typedef union {
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    } rgba;
    uint32_t uint32;
} rgba_t;

typedef enum {
    PSVS_GUI_MODE_HIDDEN,
    PSVS_GUI_MODE_FPS,
    PSVS_GUI_MODE_OSD,
    PSVS_GUI_MODE_FULL,
    PSVS_GUI_MODE_MAX
} psvs_gui_mode_t;

typedef enum {
    PSVS_GUI_MENUCTRL_CPU,
    PSVS_GUI_MENUCTRL_GPU_ES4,
    PSVS_GUI_MENUCTRL_BUS,
    PSVS_GUI_MENUCTRL_GPU_XBAR,
    PSVS_GUI_MENUCTRL_PROFILE,
    PSVS_GUI_MENUCTRL_MAX
} psvs_gui_menu_control_t;

psvs_gui_mode_t psvs_gui_get_mode();

void psvs_gui_input_check(uint32_t buttons);

void psvs_gui_set_framebuf(const SceDisplayFrameBuf *pParam);
bool psvs_gui_fb_res_changed();
bool psvs_gui_mode_changed();

void psvs_gui_set_back_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void psvs_gui_set_text_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void psvs_gui_set_text_color2(rgba_t color);
void psvs_gui_set_text_scale(float scale);

void psvs_gui_dd_fps();

void psvs_gui_clear();
void psvs_gui_print(int x, int y, const char *str);
void psvs_gui_printf(int x, int y, const char *format, ...);

const char *psvs_gui_units_from_size(int bytes);
int psvs_gui_value_from_size(int bytes);
rgba_t psvs_gui_scale_color(int value, int min, int max);

void psvs_gui_draw_osd_template();
void psvs_gui_draw_osd_cpu();
void psvs_gui_draw_osd_fps();
void psvs_gui_draw_osd_batt();

void psvs_gui_draw_template();
void psvs_gui_draw_header();
void psvs_gui_draw_batt_section();
void psvs_gui_draw_cpu_section();
void psvs_gui_draw_memory_section();
void psvs_gui_draw_menu();

int psvs_gui_init();
void psvs_gui_deinit();
void psvs_gui_cpy();

#endif
