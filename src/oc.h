#ifndef _OC_H_
#define _OC_H_

typedef enum {
    PSVS_OC_CPU,
    PSVS_OC_GPU_ES4,
    PSVS_OC_GPU,
    PSVS_OC_BUS,
    PSVS_OC_GPU_XBAR,
    PSVS_OC_MAX
} psvs_oc_device_t;

typedef enum {
    PSVS_OC_MODE_DEFAULT,
    PSVS_OC_MODE_MANUAL,
    PSVS_OC_MODE_MAX
} psvs_oc_mode_t;

int psvs_oc_get_freq(psvs_oc_device_t device, int default_freq);

psvs_oc_mode_t psvs_oc_get_mode(psvs_oc_device_t device);
void psvs_oc_set_mode(psvs_oc_device_t device, psvs_oc_mode_t mode);

void psvs_oc_holy_shit();

void psvs_oc_reset_manual_for_device(psvs_oc_device_t device);
void psvs_oc_change_manual(psvs_oc_device_t device, bool raise_freq);

void psvs_oc_init();

#endif
