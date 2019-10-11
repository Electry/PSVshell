#ifndef _PERF_H_
#define _PERF_H_

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
} psvs_memory_t;

void psvs_perf_calc_fps();
void psvs_perf_poll_cpu();
void psvs_perf_poll_memory();

int psvs_perf_get_fps();
int psvs_perf_get_load(int core);
int psvs_perf_get_peak();
int psvs_perf_get_temp();
psvs_memory_t *psvs_perf_get_memusage();

#endif
