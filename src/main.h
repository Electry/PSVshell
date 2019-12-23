#ifndef _MAIN_H_
#define _MAIN_H_
#include "perf.h"

#define PSVS_VERSION_STRING "PSVshell v1.1"
#define PSVS_VERSION_VER    "PSVS0100"

#define DECL_FUNC_HOOK_PATCH_CTRL(index, name) \
    static int name##_patched(int port, SceCtrlData *pad_data, int count) { \
        int ret = TAI_CONTINUE(int, g_hookrefs[(index)], port, pad_data, count); \
        if (ret > 0) \
            psvs_input_check(pad_data, count); \
        return ret; \
    }

#define DECL_FUNC_HOOK_PATCH_FREQ_GETTER(index, name, device) \
    static int name##_patched() { \
        uint32_t state; \
        ENTER_SYSCALL(state); \
        TAI_CONTINUE(int, g_hookrefs[(index)]); \
        int freq = psvs_oc_get_freq((device));  \
        EXIT_SYSCALL(state); \
        return freq; \
    }

#define DACR_UNRESTRICT(state) \
    asm volatile ("mrc p15, 0, %0, c3, c0, 0\n\t" \
                  "mcr p15, 0, %1, c3, c0, 0" : "=&r" (state) : "r" (0xffffffff));

#define DACR_RESET(state) \
    asm volatile ("mcr p15, 0, %0, c3, c0, 0" : : "r" (state));

#define INVALID_PID -1

#define PSVS_FRAMEBUF_HOOK_MAGIC 0x7183015

typedef enum {
    PSVS_APP_SCESHELL,
    PSVS_APP_SYSTEM,
    PSVS_APP_SYSTEM_XCL,
    PSVS_APP_PSPEMU,
    PSVS_APP_GAME, // or homebrew
    PSVS_APP_MAX
} psvs_app_t;

extern SceUID g_pid;
extern psvs_app_t g_app;
extern char g_titleid[32];

extern bool g_is_dolce;

extern int (*SceSysmemForKernel_0x3650963F)(uint32_t a1, SceSysmemAddressSpaceInfo *a2);
extern int (*SceThreadmgrForDriver_0x7E280B69)(SceKernelSystemInfo *pInfo);
extern int (*ScePervasiveForDriver_0xE9D95643)(int mul, int ndiv);

extern uint32_t *ScePower_41C8;
extern uint32_t *ScePower_41CC;
extern uint32_t *ScePower_0;

extern int (*_kscePowerGetArmClockFrequency)();
extern int (*_kscePowerGetBusClockFrequency)();
extern int (*_kscePowerGetGpuEs4ClockFrequency)(int *a1, int *a2);
extern int (*_kscePowerGetGpuXbarClockFrequency)();

extern int (*_kscePowerSetArmClockFrequency)(int freq);
extern int (*_kscePowerSetBusClockFrequency)(int freq);
extern int (*_kscePowerSetGpuEs4ClockFrequency)(int a1, int a2);
extern int (*_kscePowerSetGpuXbarClockFrequency)(int freq);

#endif
