#ifndef _OC_H_
#define _OC_H_

#define PSVS_OC_MAX_FREQ_N 10

#define PSVS_OC_DECL_SETTER(fun) \
    static int _##fun(int freq) { return fun(freq); }
#define PSVS_OC_DECL_GETTER(fun) \
    static int _##fun() { return fun(); }

typedef enum {
    PSVS_OC_DEVICE_CPU,
    PSVS_OC_DEVICE_GPU_ES4,
    PSVS_OC_DEVICE_BUS,
    PSVS_OC_DEVICE_GPU_XBAR,
    PSVS_OC_DEVICE_MAX
} psvs_oc_device_t;

typedef enum {
    PSVS_OC_MODE_DEFAULT,
    PSVS_OC_MODE_MANUAL,
    PSVS_OC_MODE_MAX
} psvs_oc_mode_t;

typedef struct {
    char ver[8];
    psvs_oc_mode_t mode[PSVS_OC_DEVICE_MAX];
    int manual_freq[PSVS_OC_DEVICE_MAX];
} psvs_oc_profile_t;

typedef struct {
    const int freq[PSVS_OC_MAX_FREQ_N];
    const int freq_n;
    const int default_freq;
    int (*get_freq)();
    int (*set_freq)(int freq);
} psvs_oc_devopt_t;

int psvs_oc_get_freq(psvs_oc_device_t device);
int psvs_oc_set_freq(psvs_oc_device_t device, int freq);
void psvs_oc_holy_shit();

int psvs_oc_get_target_freq(psvs_oc_device_t device, int default_freq);
void psvs_oc_set_target_freq(psvs_oc_device_t device);
psvs_oc_mode_t psvs_oc_get_mode(psvs_oc_device_t device);
void psvs_oc_set_mode(psvs_oc_device_t device, psvs_oc_mode_t mode);

// profiles
psvs_oc_profile_t *psvs_oc_get_profile();
void psvs_oc_set_profile(psvs_oc_profile_t *oc);
bool psvs_oc_has_changed();
void psvs_oc_set_changed(bool changed);

// default freq
int psvs_oc_get_default_freq(psvs_oc_device_t device);

// manual freq adjust
void psvs_oc_reset_manual(psvs_oc_device_t device);
void psvs_oc_change_manual(psvs_oc_device_t device, bool raise_freq);

void psvs_oc_init();

#endif
