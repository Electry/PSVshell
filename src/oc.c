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
        .freq_n = 8, .freq = {41, 83, 111, 166, 222, 333, 444, 500}, .default_freq = 333,
        .get_freq = __kscePowerGetArmClockFrequency,
        .set_freq = __kscePowerSetArmClockFrequency
    },
    [PSVS_OC_DEVICE_GPU_ES4] = {
        .freq_n = 6, .freq = {41, 55, 83, 111, 166, 222}, .default_freq = 111,
        .get_freq = __kscePowerGetGpuEs4ClockFrequency,
        .set_freq = __kscePowerSetGpuEs4ClockFrequency
    },
    [PSVS_OC_DEVICE_BUS] = {
        .freq_n = 5, .freq = {55, 83, 111, 166, 222}, .default_freq = 222,
        .get_freq = __kscePowerGetBusClockFrequency,
        .set_freq = __kscePowerSetBusClockFrequency
    },
    [PSVS_OC_DEVICE_GPU_XBAR] = {
        .freq_n = 3, .freq = {83, 111, 166}, .default_freq = 111,
        .get_freq = __kscePowerGetGpuXbarClockFrequency,
        .set_freq = __kscePowerSetGpuXbarClockFrequency
    },
};

static psvs_oc_profile_t g_oc = {
    .ver = PSVS_VERSION_VER,
    .mode = {0},
    .target_freq = {0},
    .max_freq = {0}
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
    if (g_oc.mode[device] != PSVS_OC_MODE_DEFAULT)
        return g_oc.target_freq[device];
    return default_freq;
}

int psvs_oc_get_max_freq(psvs_oc_device_t device) {
    if (g_oc.mode[device] != PSVS_OC_MODE_DEFAULT)
        return g_oc.max_freq[device];
    return g_oc_devopt[device].default_freq;
}

void psvs_oc_set_target_freq(psvs_oc_device_t device) {
    // Refresh manual clocks
    if (g_oc.mode[device] == PSVS_OC_MODE_MANUAL)
        psvs_oc_set_freq(device, g_oc.max_freq[device]);
    // Refresh auto clocks
    else if (g_oc.mode[device] == PSVS_OC_MODE_AUTO)
        psvs_oc_set_freq(device, g_oc.target_freq[device]);
    // Restore default clocks
    else if (g_oc.mode[device] == PSVS_OC_MODE_DEFAULT)
        psvs_oc_set_freq(device, psvs_oc_get_default_freq(device));
}

psvs_oc_mode_t psvs_oc_get_mode(psvs_oc_device_t device) {
    return g_oc.mode[device];
}

void psvs_oc_set_mode(psvs_oc_device_t device, psvs_oc_mode_t mode) {
    g_oc.mode[device] = mode;
    g_oc_has_changed = true;
    psvs_oc_set_target_freq(device);
}

psvs_oc_profile_t *psvs_oc_get_profile() {
    return &g_oc;
}

void psvs_oc_set_profile(psvs_oc_profile_t *oc) {
    memcpy(&g_oc, oc, sizeof(psvs_oc_profile_t));
    g_oc_has_changed = false;

    for (int i = 0; i < PSVS_OC_DEVICE_MAX; i++)
        psvs_oc_set_target_freq(i);
}

bool psvs_oc_has_changed() {
    return g_oc_has_changed;
}

void psvs_oc_set_changed(bool changed) {
    g_oc_has_changed = changed;
}

int psvs_oc_get_default_freq(psvs_oc_device_t device) {
    int freq = g_oc_devopt[device].default_freq;

    if (g_pid == INVALID_PID)
        return freq;

    uintptr_t pstorage = 0;
    int ret = ksceKernelGetProcessLocalStorageAddrForPid(g_pid, *ScePower_0, (void **)&pstorage, 0);
    if (ret < 0 || pstorage == 0)
        return freq;

    switch (device) {
        case PSVS_OC_DEVICE_BUS: freq = *(uint32_t *)(pstorage + 40); break;
        default:
        case PSVS_OC_DEVICE_CPU: freq = *(uint32_t *)(pstorage + 44); break;
        case PSVS_OC_DEVICE_GPU_XBAR: freq = *(uint32_t *)(pstorage + 48); break;
        case PSVS_OC_DEVICE_GPU_ES4: freq = *(uint32_t *)(pstorage + 52); break;
    }

    // Validate
    bool valid = false;
    for (int i = 0; i < g_oc_devopt[device].freq_n; i++) {
        if (freq == g_oc_devopt[device].freq[i]) {
            valid = true;
            break;
        }
    }

    return valid ? freq : g_oc_devopt[device].default_freq;
}

void psvs_oc_reset(psvs_oc_device_t device) {
    g_oc.max_freq[device] = psvs_oc_get_freq(device);
    g_oc.target_freq[device] = psvs_oc_get_freq(device);
    g_oc_has_changed = true;
}

void psvs_oc_change(psvs_oc_device_t device, bool raise_freq) {
    int target_freq = g_oc.target_freq[device]; // current freq

    for (int i = 0; i < g_oc_devopt[device].freq_n; i++) {
        int ii = raise_freq ? i : g_oc_devopt[device].freq_n - i - 1;
        if ((raise_freq && g_oc_devopt[device].freq[ii] > target_freq)
                || (!raise_freq && g_oc_devopt[device].freq[ii] < target_freq)) {
            target_freq = g_oc_devopt[device].freq[ii];
            break;
        }
    }

    // Keep clocks inside the limits (111MHz to max_freq) in AUTO mode
    if (g_oc.mode[device] == PSVS_OC_MODE_AUTO) {
        if (target_freq < PSVS_OC_CPU_MIN_FREQ)
            target_freq = PSVS_OC_CPU_MIN_FREQ;
        if (target_freq > g_oc.max_freq[device])
            target_freq = g_oc.max_freq[device];
    }
    // In manual mode, max_freq and target_freq are the same
    if (g_oc.mode[device] == PSVS_OC_MODE_MANUAL) {
        g_oc.max_freq[device] = target_freq;
        g_oc_has_changed = true;
    }
    g_oc.target_freq[device] = target_freq;

    // Refresh manual clocks
    if (g_oc.mode[device] != PSVS_OC_MODE_DEFAULT)
        psvs_oc_set_freq(device, g_oc.max_freq[device]);
}

bool psvs_oc_check_raise_freq(psvs_oc_device_t device) {
    if(device != PSVS_OC_DEVICE_CPU)
        return false;

    int freq = g_oc_devopt[device].get_freq();
    int peak = psvs_perf_get_peak();
    int avg = (psvs_perf_get_load(0) + psvs_perf_get_load(1) + psvs_perf_get_load(2)) / 3;

    if(freq <= 111 && peak >= 50)
        return true;
    if(freq <= 222 && peak >= 60)
        return true;
    if(freq == 333 && (peak >= 80 || avg >= 50))
        return true;
    if(freq == 444 && (peak >=85 || avg >= 60))
        return true;
    
    return false;
}

bool psvs_oc_check_lower_freq(psvs_oc_device_t device) {
    if(device != PSVS_OC_DEVICE_CPU)
        return false;

    int freq = g_oc_devopt[device].get_freq();
    int peak = psvs_perf_get_peak();

    if (freq == 500 && peak < 75)
        return true;
    if (freq < 500 && peak < 70)
        return true;
    if (freq < 333 && peak < 60)
        return true;
    if (freq < 222 && peak < 50)
        return true;
    if (freq < 111 && peak < 45)
        return true;

    return false;
}

void psvs_oc_change_max_freq(psvs_oc_device_t device, bool raise_freq) {
    int max_freq = g_oc.max_freq[device]; // current max freq

    for (int i = 0; i < g_oc_devopt[device].freq_n; i++) {
        int ii = raise_freq ? i : g_oc_devopt[device].freq_n - i - 1;
        if ((raise_freq && g_oc_devopt[device].freq[ii] > max_freq)
                || (!raise_freq && g_oc_devopt[device].freq[ii] < max_freq)) {
            max_freq = g_oc_devopt[device].freq[ii];
            break;
        }
    }
    // Update max freq
    g_oc.max_freq[device] = max_freq;
    if (g_oc.target_freq[device] > max_freq) {
        g_oc.target_freq[device] = max_freq;
        psvs_oc_set_target_freq(device);
    }
    g_oc_has_changed = true;
}

void psvs_oc_init() {
    g_oc_has_changed = true;
    for (int i = 0; i < PSVS_OC_DEVICE_MAX; i++) {
        g_oc.mode[i] = PSVS_OC_MODE_DEFAULT;
        psvs_oc_reset(i);
    }
}
