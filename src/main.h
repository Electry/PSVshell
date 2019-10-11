#ifndef _MAIN_H_
#define _MAIN_H_
#include "perf.h"

#define FOR_KERNEL true
#define FOR_USER   false
#define POSITIVE   true
#define NEGATIVE   false

#define DECL_FUNC_HOOK_PATCH_CTRL(index, name, logic, for_kernel) \
    static int name##_patched(int port, SceCtrlData *pad_data, int count) { \
		int ret = TAI_CONTINUE(int, g_hookrefs[(index)], port, pad_data, count); \
		if (ret > 0) \
			psvs_input_check(pad_data, count, logic, for_kernel); \
		return ret; \
	}

extern char g_titleid[32];

extern int (*SceSysmemForKernel_0x3650963F)(uint32_t a1, SceSysmemAddressSpaceInfo *a2);
extern int (*SceThreadmgrForDriver_0x7E280B69)(SceKernelSystemInfo *pInfo);
extern int (*ScePervasiveForDriver_0xE9D95643)(int mul, int ndiv);

extern uint32_t *ScePower_41C8;
extern uint32_t *ScePower_41CC;

extern int (*_kscePowerGetArmClockFrequency)();
extern int (*_kscePowerGetBusClockFrequency)();
extern int (*_kscePowerGetGpuClockFrequency)();
extern int (*_kscePowerGetGpuEs4ClockFrequency)(int *a1, int *a2);
extern int (*_kscePowerGetGpuXbarClockFrequency)();

extern int (*_kscePowerSetArmClockFrequency)(int freq);
extern int (*_kscePowerSetBusClockFrequency)(int freq);
extern int (*_kscePowerSetGpuClockFrequency)(int freq);
extern int (*_kscePowerSetGpuEs4ClockFrequency)(int a1, int a2);
extern int (*_kscePowerSetGpuXbarClockFrequency)(int freq);

#endif
