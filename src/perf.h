#ifndef _PERF_H_
#define _PERF_H_

#define PSVS_CHECK_ASSIGN(struct, field, new_value) \
    if (struct.field != (new_value)) struct._has_changed = true; \
    struct.field = (new_value)

typedef enum {
    PSVS_PERF_FPS_30,
    PSVS_PERF_FPS_60,
    PSVS_PERF_FPS_UNCAP
} psvs_perf_fps_cap_t;

typedef struct SceKernelSystemInfo {
    SceSize   size;
    SceUInt32 activeCpuMask;
    struct {
        SceKernelSysClock idleClock;
        SceUInt32         comesOutOfIdleCount;
        SceUInt32         threadSwitchCount;
    } cpuInfo[4];
} SceKernelSystemInfo;

typedef struct SceSysmemAddressSpaceInfo {
    uintptr_t base;
    uint32_t total;
    uint32_t free;
    uint32_t unkC;
} SceSysmemAddressSpaceInfo;

typedef struct psvs_memory_t {
    uint32_t main_free;
    uint32_t main_total;
    uint32_t cdram_free;
    uint32_t cdram_total;
    uint32_t phycont_free;
    uint32_t phycont_total;
    bool _has_changed;
} psvs_memory_t;

typedef struct psvs_battery_t {
    int temp;
    int percent;
    int lt_hours;
    int lt_minutes;
    int power_cons;
    bool is_charging;
    bool _has_changed;
} psvs_battery_t;

void psvs_perf_calc_fps();
void psvs_perf_poll_cpu(int performance_mode);
void psvs_perf_poll_memory();
void psvs_perf_poll_batt();

void psvs_perf_init_power_meter();
void psvs_perf_compute_power();
void psvs_perf_reset_peak(bool raise);

int psvs_perf_get_fps();
int psvs_perf_get_fps_cap();
int psvs_perf_get_load(int core);
int psvs_perf_get_peak();
int psvs_perf_get_smooth_peak();
psvs_battery_t *psvs_perf_get_batt();
psvs_memory_t *psvs_perf_get_memusage();

#endif
