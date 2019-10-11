#include <vitasdkkern.h>
#include <taihen.h>
#include <stdbool.h>

#include "main.h"
#include "oc.h"

#define PSVS_OC_FREQ_CPU_N 8
static const int PSVS_OC_FREQ_CPU[PSVS_OC_FREQ_CPU_N] = {
    41, 83, 111, 166, 222, 333, 444, 500
};
#define PSVS_OC_FREQ_BUS_N 5
static const int PSVS_OC_FREQ_BUS[PSVS_OC_FREQ_BUS_N] = {
    55, 83, 111, 166, 222
};
#define PSVS_OC_FREQ_GPU_N 7
static const int PSVS_OC_FREQ_GPU[PSVS_OC_FREQ_GPU_N] = {
    41, 55, 83, 111, 166, 222, 333
};
#define PSVS_OC_FREQ_GPU_ES4_N 6
static const int PSVS_OC_FREQ_GPU_ES4[PSVS_OC_FREQ_GPU_ES4_N] = {
    41, 55, 83, 111, 166, 222
};
#define PSVS_OC_FREQ_GPU_XBAR_N 3
static const int PSVS_OC_FREQ_GPU_XBAR[PSVS_OC_FREQ_GPU_XBAR_N] = {
    83, 111, 166
};

static psvs_oc_mode_t g_oc_mode[PSVS_OC_MAX] = {PSVS_OC_MODE_DEFAULT};
static int g_oc_manual_freq[PSVS_OC_MAX] = {0};

int psvs_oc_get_freq(psvs_oc_device_t device, int default_freq) {
    return g_oc_mode[device] == PSVS_OC_MODE_MANUAL ? g_oc_manual_freq[device] : default_freq;
}

psvs_oc_mode_t psvs_oc_get_mode(psvs_oc_device_t device) {
    return g_oc_mode[device];
}

void psvs_oc_set_mode(psvs_oc_device_t device, psvs_oc_mode_t mode) {
    g_oc_mode[device] = mode;
}

void psvs_oc_holy_shit() {
    // Apply mul:div (15:0)
    ScePervasiveForDriver_0xE9D95643(15, 16 - 0);

    // Store global freq & mul for kscePowerGetArmClockFrequency()
    *ScePower_41C8 = 500;
    *ScePower_41CC = 15;
}

void psvs_oc_reset_manual_for_device(psvs_oc_device_t device) {
    int r2;
    switch (device) {
        case PSVS_OC_CPU: g_oc_manual_freq[PSVS_OC_CPU] = _kscePowerGetArmClockFrequency(); break;
        case PSVS_OC_BUS: g_oc_manual_freq[PSVS_OC_BUS] = _kscePowerGetBusClockFrequency(); break;
        case PSVS_OC_GPU: g_oc_manual_freq[PSVS_OC_GPU] = _kscePowerGetGpuClockFrequency(); break;
        case PSVS_OC_GPU_ES4: _kscePowerGetGpuEs4ClockFrequency(&g_oc_manual_freq[PSVS_OC_GPU_ES4], &r2); break;
        case PSVS_OC_GPU_XBAR: g_oc_manual_freq[PSVS_OC_GPU_XBAR] = _kscePowerGetGpuXbarClockFrequency(); break;
        default: break;
    }
}

static int _psvs_oc_get_next_freq(psvs_oc_device_t device, bool raise_freq) {
    const int *table;
    int size = 0;
    int target_freq = g_oc_manual_freq[device];

    switch (device) {
        case PSVS_OC_CPU:      table = PSVS_OC_FREQ_CPU;      size = PSVS_OC_FREQ_CPU_N;      break;
        case PSVS_OC_BUS:      table = PSVS_OC_FREQ_BUS;      size = PSVS_OC_FREQ_BUS_N;      break;
        case PSVS_OC_GPU:      table = PSVS_OC_FREQ_GPU;      size = PSVS_OC_FREQ_GPU_N;      break;
        case PSVS_OC_GPU_ES4:  table = PSVS_OC_FREQ_GPU_ES4;  size = PSVS_OC_FREQ_GPU_ES4_N;  break;
        case PSVS_OC_GPU_XBAR: table = PSVS_OC_FREQ_GPU_XBAR; size = PSVS_OC_FREQ_GPU_XBAR_N; break;
        default: return 0;
    }

    for (int i = 0; i < size; i++) {
        int ii = raise_freq ? i : size - i - 1;
        if ((raise_freq && table[ii] > target_freq)
                || (!raise_freq && table[ii] < target_freq)) {
            target_freq = table[ii];
            break;
        }
    }

    return target_freq;
}

void psvs_oc_change_manual(psvs_oc_device_t device, bool raise_freq) {
    g_oc_manual_freq[device] = _psvs_oc_get_next_freq(device, raise_freq);

    switch (device) {
        case PSVS_OC_CPU: _kscePowerSetArmClockFrequency(g_oc_manual_freq[device]); break;
        case PSVS_OC_BUS: _kscePowerSetBusClockFrequency(g_oc_manual_freq[device]); break;
        case PSVS_OC_GPU: _kscePowerSetGpuClockFrequency(g_oc_manual_freq[device]); break;
        case PSVS_OC_GPU_ES4: _kscePowerSetGpuEs4ClockFrequency(
                                g_oc_manual_freq[device], g_oc_manual_freq[device]); break;
        case PSVS_OC_GPU_XBAR: _kscePowerSetGpuXbarClockFrequency(g_oc_manual_freq[device]); break;
        default: break;
    }
}

void psvs_oc_init() {
    for (int i = 0; i < PSVS_OC_MAX; i++)
        g_oc_mode[i] = PSVS_OC_MODE_DEFAULT;

    psvs_oc_reset_manual_for_device(PSVS_OC_CPU);
    psvs_oc_reset_manual_for_device(PSVS_OC_BUS);
    psvs_oc_reset_manual_for_device(PSVS_OC_GPU);
    psvs_oc_reset_manual_for_device(PSVS_OC_GPU_ES4);
    psvs_oc_reset_manual_for_device(PSVS_OC_GPU_XBAR);
}
