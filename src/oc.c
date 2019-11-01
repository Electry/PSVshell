#include <vitasdkkern.h>
#include <taihen.h>
#include <stdbool.h>
#include <string.h>

#include "main.h"
#include "oc.h"

// Declare helper getter/setter for GpuEs4
static int __kscePowerGetGpuEs4ClockFrequency() {
    int a1, a2;
    _kscePowerGetGpuEs4ClockFrequency(&a1, &a2);
    return a1;
}
static int __kscePowerSetGpuEs4ClockFrequency(int freq) {
    return _kscePowerSetGpuEs4ClockFrequency(freq, freq);
}

// Declare static getters/setters
PSVS_OC_DECL_GETTER(_kscePowerGetArmClockFrequency);
PSVS_OC_DECL_GETTER(_kscePowerGetBusClockFrequency);
PSVS_OC_DECL_GETTER(_kscePowerGetGpuXbarClockFrequency);
PSVS_OC_DECL_SETTER(_kscePowerSetArmClockFrequency);
PSVS_OC_DECL_SETTER(_kscePowerSetBusClockFrequency);
PSVS_OC_DECL_SETTER(_kscePowerSetGpuXbarClockFrequency);

static psvs_oc_devopt_t g_oc_devopt[PSVS_OC_DEVICE_MAX] = {
    [PSVS_OC_DEVICE_CPU] = {
        .freq_n = 8, .freq = {41, 83, 111, 166, 222, 333, 444, 500},
        .get_freq = __kscePowerGetArmClockFrequency,
        .set_freq = __kscePowerSetArmClockFrequency
    },
    [PSVS_OC_DEVICE_GPU_ES4] = {
        .freq_n = 6, .freq = {41, 55, 83, 111, 166, 222},
        .get_freq = __kscePowerGetGpuEs4ClockFrequency,
        .set_freq = __kscePowerSetGpuEs4ClockFrequency
    },
    [PSVS_OC_DEVICE_BUS] = {
        .freq_n = 5, .freq = {55, 83, 111, 166, 222},
        .get_freq = __kscePowerGetBusClockFrequency,
        .set_freq = __kscePowerSetBusClockFrequency
    },
    [PSVS_OC_DEVICE_GPU_XBAR] = {
        .freq_n = 3, .freq = {83, 111, 166},
        .get_freq = __kscePowerGetGpuXbarClockFrequency,
        .set_freq = __kscePowerSetGpuXbarClockFrequency
    },
};

static psvs_oc_profile_t g_oc = {
    .ver = PSVS_VERSION_VER,
    .mode = {0},
    .manual_freq = {0}
};
static bool g_oc_has_changed = true;

int psvs_oc_get_freq(psvs_oc_device_t device) {
    return g_oc_devopt[device].get_freq();
}

int psvs_oc_set_freq(psvs_oc_device_t device, int freq) {
    return g_oc_devopt[device].set_freq(freq);
}

void psvs_oc_holy_shit() {
    // Apply mul:div (15:0)
    ScePervasiveForDriver_0xE9D95643(15, 16 - 0);

    // Store global freq & mul for kscePowerGetArmClockFrequency()
    *ScePower_41C8 = 500;
    *ScePower_41CC = 15;
}

int psvs_oc_get_target_freq(psvs_oc_device_t device, int default_freq) {
    if (g_oc.mode[device] == PSVS_OC_MODE_MANUAL)
        return g_oc.manual_freq[device];
    return default_freq;
}

psvs_oc_mode_t psvs_oc_get_mode(psvs_oc_device_t device) {
    return g_oc.mode[device];
}

void psvs_oc_set_mode(psvs_oc_device_t device, psvs_oc_mode_t mode) {
    g_oc.mode[device] = mode;
    g_oc_has_changed = true;

    // Refresh manual clocks
    if (g_oc.mode[device] == PSVS_OC_MODE_MANUAL)
        psvs_oc_set_freq(device, g_oc.manual_freq[device]);
}

psvs_oc_profile_t *psvs_oc_get_profile() {
    return &g_oc;
}

void psvs_oc_set_profile(psvs_oc_profile_t *oc) {
    memcpy(&g_oc, oc, sizeof(psvs_oc_profile_t));
    g_oc_has_changed = false;

    // Refresh manual clocks
    for (int i = 0; i < PSVS_OC_DEVICE_MAX; i++) {
        if (g_oc.mode[i] == PSVS_OC_MODE_MANUAL)
            psvs_oc_set_freq(i, g_oc.manual_freq[i]);
    }
}

bool psvs_oc_has_changed() {
    return g_oc_has_changed;
}

void psvs_oc_set_changed(bool changed) {
    g_oc_has_changed = changed;
}

void psvs_oc_reset_manual(psvs_oc_device_t device) {
    g_oc.manual_freq[device] = psvs_oc_get_freq(device);
    g_oc_has_changed = true;
}

void psvs_oc_change_manual(psvs_oc_device_t device, bool raise_freq) {
    int target_freq = g_oc.manual_freq[device]; // current manual freq

    for (int i = 0; i < g_oc_devopt[device].freq_n; i++) {
        int ii = raise_freq ? i : g_oc_devopt[device].freq_n - i - 1;
        if ((raise_freq && g_oc_devopt[device].freq[ii] > target_freq)
                || (!raise_freq && g_oc_devopt[device].freq[ii] < target_freq)) {
            target_freq = g_oc_devopt[device].freq[ii];
            break;
        }
    }

    g_oc.manual_freq[device] = target_freq;
    g_oc_has_changed = true;

    // Refresh manual clocks
    if (g_oc.mode[device] == PSVS_OC_MODE_MANUAL)
        psvs_oc_set_freq(device, g_oc.manual_freq[device]);
}

void psvs_oc_init() {
    g_oc_has_changed = true;
    for (int i = 0; i < PSVS_OC_DEVICE_MAX; i++) {
        g_oc.mode[i] = PSVS_OC_MODE_DEFAULT;
        psvs_oc_reset_manual(i);
    }
}
