#include <vitasdkkern.h>
#include <taihen.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "oc.h"
#include "gui.h"
#include "perf.h"
#include "profile.h"

int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t *addr);
int module_get_export_func(SceUID pid, const char *modname, uint32_t libnid, uint32_t funcnid, uintptr_t *func);
bool ksceAppMgrIsExclusiveProcessRunning();
bool ksceSblAimgrIsGenuineDolce();
bool ksceSblACMgrIsPspEmu(SceUID pid);
bool ksceSblACMgrIsSceShell(SceUID pid);

#define PSVS_MAX_HOOKS 18
static tai_hook_ref_t g_hookrefs[PSVS_MAX_HOOKS];
static SceUID         g_hooks[PSVS_MAX_HOOKS];
static SceUID         g_injects[1];

static SceUID g_mutex_cpufreq_uid = -1;
static SceUID g_mutex_procevent_uid = -1;
static SceUID g_mutex_framebuf_uid = -1;
static SceUID g_thread_uid = -1;
static bool   g_thread_run = true;

SceUID g_pid = INVALID_PID;
psvs_app_t g_app = PSVS_APP_SCESHELL;
char g_titleid[32] = "";

bool g_is_dolce = false;

int (*SceSysmemForKernel_0x3650963F)(uint32_t a1, SceSysmemAddressSpaceInfo *a2);
int (*SceThreadmgrForDriver_0x7E280B69)(SceKernelSystemInfo *pInfo);
int (*ScePervasiveForDriver_0xE9D95643)(int mul, int ndiv);

uint32_t *ScePower_41C8 = NULL;
uint32_t *ScePower_41CC = NULL;
uint32_t *ScePower_0    = NULL;

int (*_kscePowerGetArmClockFrequency)();
int (*_kscePowerGetBusClockFrequency)();
int (*_kscePowerGetGpuEs4ClockFrequency)(int *a1, int *a2);
int (*_kscePowerGetGpuXbarClockFrequency)();

int (*_kscePowerSetArmClockFrequency)(int freq);
int (*_kscePowerSetBusClockFrequency)(int freq);
int (*_kscePowerSetGpuEs4ClockFrequency)(int a1, int a2);
int (*_kscePowerSetGpuXbarClockFrequency)(int freq);

static void psvs_input_check(SceCtrlData *pad_data, int count) {
    // Do not pass input to fg app
    if (g_app != PSVS_APP_BLACKLIST && psvs_gui_get_mode() == PSVS_GUI_MODE_FULL) {
        SceCtrlData kctrl;
        kctrl.buttons = 0;
        for (int i = 0; i < count; i++)
            ksceKernelMemcpyKernelToUser((uintptr_t)&pad_data[i].buttons, &kctrl.buttons, sizeof(uint32_t));
    }
}

int ksceDisplaySetFrameBufInternal_patched(int head, int index, const SceDisplayFrameBuf *pParam, int sync) {
    if (sync == PSVS_FRAMEBUF_HOOK_MAGIC) {
        sync = 1;
        goto DISPLAY_HOOK_RET;
    }

    if (head != ksceDisplayGetPrimaryHead() || !pParam || !pParam->base)
        goto DISPLAY_HOOK_RET;

    if (g_app == PSVS_APP_BLACKLIST)
        goto DISPLAY_HOOK_RET;

    if (!index && g_app == PSVS_APP_SCESHELL)
        goto DISPLAY_HOOK_RET; // Do not draw on i0 in SceShell

    if (index && (ksceAppMgrIsExclusiveProcessRunning() || g_app == PSVS_APP_GAME || g_app == PSVS_APP_SYSTEM_XCL))
        goto DISPLAY_HOOK_RET; // Do not draw over SceShell overlay

    psvs_gui_mode_t mode = psvs_gui_get_mode();
    if (mode == PSVS_GUI_MODE_HIDDEN)
        goto DISPLAY_HOOK_RET;

    int ret = ksceKernelLockMutex(g_mutex_framebuf_uid, 1, NULL);
    if (ret < 0)
        goto DISPLAY_HOOK_RET;

    psvs_perf_calc_fps();

    if (mode == PSVS_GUI_MODE_FULL)
        psvs_perf_poll_memory();

    psvs_gui_set_framebuf(pParam);

    if (mode == PSVS_GUI_MODE_FPS || mode == PSVS_GUI_MODE_FULL) {
        psvs_gui_dd_fps(); // draw fps onto fb
    }

    if (mode == PSVS_GUI_MODE_OSD || mode == PSVS_GUI_MODE_FULL) {
        psvs_gui_cpy(); // cpy from buffer

        if (sync && mode == PSVS_GUI_MODE_FULL && g_app != PSVS_APP_SCESHELL && g_app != PSVS_APP_SYSTEM) {
            // update now to fix flicker when vblank period is missed
            ksceKernelUnlockMutex(g_mutex_framebuf_uid, 1);

            int ret = TAI_CONTINUE(int, g_hookrefs[0], head, index, pParam, 0);
            ret = ksceDisplaySetFrameBufInternal(head, index, pParam, PSVS_FRAMEBUF_HOOK_MAGIC);
            return ret;
        }
    }

    ksceKernelUnlockMutex(g_mutex_framebuf_uid, 1);

DISPLAY_HOOK_RET:
    return TAI_CONTINUE(int, g_hookrefs[0], head, index, pParam, sync);
}

DECL_FUNC_HOOK_PATCH_CTRL(1, sceCtrlPeekBufferNegative)
DECL_FUNC_HOOK_PATCH_CTRL(2, sceCtrlPeekBufferNegative2)
DECL_FUNC_HOOK_PATCH_CTRL(3, sceCtrlPeekBufferPositive)
DECL_FUNC_HOOK_PATCH_CTRL(4, sceCtrlPeekBufferPositive2)
DECL_FUNC_HOOK_PATCH_CTRL(5, sceCtrlReadBufferNegative)
DECL_FUNC_HOOK_PATCH_CTRL(6, sceCtrlReadBufferNegative2)
DECL_FUNC_HOOK_PATCH_CTRL(7, sceCtrlReadBufferPositive)
DECL_FUNC_HOOK_PATCH_CTRL(8, sceCtrlReadBufferPositive2)

int kscePowerSetArmClockFrequency_patched(int freq) {
    int ret = ksceKernelLockMutex(g_mutex_cpufreq_uid, 1, NULL);
    if (ret < 0)
        return ret;

    freq = psvs_oc_get_target_freq(PSVS_OC_DEVICE_CPU, freq);

    if (freq > 444 && freq <= 500) {
        TAI_CONTINUE(int, g_hookrefs[9], 444);
        psvs_oc_holy_shit();
        ret = 0;
    } else {
        ret = TAI_CONTINUE(int, g_hookrefs[9], freq);
    }

    ksceKernelUnlockMutex(g_mutex_cpufreq_uid, 1);
    return ret;
}

int kscePowerSetBusClockFrequency_patched(int freq) {
    return TAI_CONTINUE(int, g_hookrefs[10], psvs_oc_get_target_freq(PSVS_OC_DEVICE_BUS, freq));
}

int kscePowerSetGpuEs4ClockFrequency_patched(int a1, int a2) {
    a1 = psvs_oc_get_target_freq(PSVS_OC_DEVICE_GPU_ES4, a1);
    a2 = psvs_oc_get_target_freq(PSVS_OC_DEVICE_GPU_ES4, a2);
    return TAI_CONTINUE(int, g_hookrefs[11], a1, a2);
}

int kscePowerSetGpuXbarClockFrequency_patched(int freq) {
    return TAI_CONTINUE(int, g_hookrefs[12], psvs_oc_get_target_freq(PSVS_OC_DEVICE_GPU_XBAR, freq));
}

DECL_FUNC_HOOK_PATCH_FREQ_GETTER(14, scePowerGetArmClockFrequency,     PSVS_OC_DEVICE_CPU)
DECL_FUNC_HOOK_PATCH_FREQ_GETTER(15, scePowerGetBusClockFrequency,     PSVS_OC_DEVICE_BUS)
DECL_FUNC_HOOK_PATCH_FREQ_GETTER(16, scePowerGetGpuClockFrequency,     PSVS_OC_DEVICE_GPU_ES4)
DECL_FUNC_HOOK_PATCH_FREQ_GETTER(17, scePowerGetGpuXbarClockFrequency, PSVS_OC_DEVICE_GPU_XBAR)

static psvs_app_t _psvs_get_app_type(int pid, const char *titleid) {
    psvs_app_t app = PSVS_APP_MAX;

    if (ksceSblACMgrIsPspEmu(pid)) {
        app = PSVS_APP_BLACKLIST;
    } else if (!strncmp(titleid, "NPXS", 4)) {
        app = PSVS_APP_SYSTEM;

        // TODO: Figure out a way to do this on the fly

        if (!strncmp(&titleid[4], "10079", 5) ||     // Daily Checker BG
                !strncmp(&titleid[4], "10063", 5)) { // MsgMW
            app = PSVS_APP_MAX; // not an app
        } else if (!strncmp(&titleid[4], "10007", 5) || // Welcome Park
                   !strncmp(&titleid[4], "10010", 5) || // Videos
                   !strncmp(&titleid[4], "10026", 5) || // Content Manager
                   !strncmp(&titleid[4], "10095", 5)) { // Panoramic Camera
            app = PSVS_APP_SYSTEM_XCL; // exclusive
        }
    } else if (ksceSblACMgrIsSceShell(pid) && !strncmp(titleid, "main", 4)) {
        app = PSVS_APP_SCESHELL;
    } else {
        app = PSVS_APP_GAME;
    }

    return app;
}

int ksceKernelInvokeProcEventHandler_patched(int pid, int ev, int a3, int a4, int *a5, int a6) {
    char titleid[sizeof(g_titleid)];
    psvs_app_t app = PSVS_APP_SCESHELL;

    int ret = ksceKernelLockMutex(g_mutex_procevent_uid, 1, NULL);
    if (ret < 0)
        goto PROCEVENT_EXIT;

    switch (ev) {
        case 1: // startup
        case 5: // resume
            // Ignore startup events if non-SceShell app is running
            if (g_app != PSVS_APP_SCESHELL)
                goto PROCEVENT_UNLOCK_EXIT;

            // Check titleid
            ksceKernelGetProcessTitleId(pid, titleid, sizeof(titleid));

            // Check app type
            app = _psvs_get_app_type(pid, titleid);
            if (app == PSVS_APP_MAX) // not an app
                goto PROCEVENT_UNLOCK_EXIT;

            break;

        case 3: // exit
        case 4: // suspend
            if (g_pid != pid)
                goto PROCEVENT_UNLOCK_EXIT;

            app = PSVS_APP_SCESHELL;
            snprintf(titleid, sizeof(titleid), "main");
            break;
    }

    if (ev == 1 || ev == 5 || ev == 3 || ev == 4) {
        if (strncmp(g_titleid, titleid, sizeof(g_titleid))) {
            // Set titleid
            strncpy(g_titleid, titleid, sizeof(g_titleid));

            // Set pid
            g_pid = (ev == 1 || ev == 5) ? pid : INVALID_PID;

            // Set type
            g_app = app;

            // Load profile
            if (g_app == PSVS_APP_BLACKLIST || !psvs_profile_load()) {
                // If no profile exists or in blacklisted app,
                // reset all options to default
                psvs_oc_init();
            }
        }
    }

PROCEVENT_UNLOCK_EXIT:
    ksceKernelUnlockMutex(g_mutex_procevent_uid, 1);

PROCEVENT_EXIT:
    return TAI_CONTINUE(int, g_hookrefs[13], pid, ev, a3, a4, a5, a6);
}

static int psvs_thread(SceSize args, void *argp) {
    while (g_thread_run) {
        if (g_app == PSVS_APP_BLACKLIST) {
            // Don't do anything if blacklisted app is running
            ksceKernelDelayThread(200 * 1000);
            continue;
        }

        // Check buttons
        SceCtrlData kctrl;
        int ret = ksceCtrlPeekBufferPositive(0, &kctrl, 1);
        if (ret < 0)
            ret = ksceCtrlPeekBufferPositive(1, &kctrl, 1);
        if (ret > 0)
            psvs_gui_input_check(kctrl.buttons);

        bool fb_or_mode_changed = psvs_gui_mode_changed() || psvs_gui_fb_res_changed();
        psvs_gui_mode_t mode = psvs_gui_get_mode();

        // If in OSD/FULL mode, poll shown info
        if (mode == PSVS_GUI_MODE_OSD || mode == PSVS_GUI_MODE_FULL) {
            psvs_perf_poll_cpu();
            psvs_perf_poll_batt();
        }

        // Redraw buffer template on gui mode or fb change
        if (fb_or_mode_changed) {
            if (mode == PSVS_GUI_MODE_OSD) {
                psvs_gui_draw_osd_template();
            } else if (mode == PSVS_GUI_MODE_FULL) {
                psvs_gui_draw_template();
            }
        }

        // Draw OSD mode
        if (mode == PSVS_GUI_MODE_OSD) {
            psvs_gui_draw_osd_cpu();
            psvs_gui_draw_osd_fps();
            psvs_gui_draw_osd_batt();
        }

        // Draw FULL mode
        else if (mode == PSVS_GUI_MODE_FULL) {
            psvs_gui_draw_header();
            psvs_gui_draw_batt_section();
            psvs_gui_draw_cpu_section();
            psvs_gui_draw_memory_section();
            psvs_gui_draw_menu();
        }

        ksceKernelDelayThread(50 * 1000);
    }

    return 0;
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
    int ret = 0;

    if (ksceSblAimgrIsGenuineDolce())
        g_is_dolce = true;

    psvs_gui_init();
    psvs_profile_init();

    tai_module_info_t tai_info;
    tai_info.size = sizeof(tai_module_info_t);
    taiGetModuleInfoForKernel(KERNEL_PID, "ScePower", &tai_info);

    module_get_offset(KERNEL_PID, tai_info.modid, 1, 0x41C8, (uintptr_t *)&ScePower_41C8);
    module_get_offset(KERNEL_PID, tai_info.modid, 1, 0x41CC, (uintptr_t *)&ScePower_41CC);
    module_get_offset(KERNEL_PID, tai_info.modid, 1, 0x0,    (uintptr_t *)&ScePower_0);

    module_get_export_func(KERNEL_PID,
            "ScePower", 0x1590166F, 0xABC6F88F, (uintptr_t *)&_kscePowerGetArmClockFrequency);
    module_get_export_func(KERNEL_PID,
            "ScePower", 0x1590166F, 0x478FE6F5, (uintptr_t *)&_kscePowerGetBusClockFrequency);
    module_get_export_func(KERNEL_PID,
            "ScePower", 0x1590166F, 0x475BCC82, (uintptr_t *)&_kscePowerGetGpuEs4ClockFrequency);
    module_get_export_func(KERNEL_PID,
            "ScePower", 0x1590166F, 0x0A750DEE, (uintptr_t *)&_kscePowerGetGpuXbarClockFrequency);

    module_get_export_func(KERNEL_PID,
            "ScePower", 0x1590166F, 0x74DB5AE5, (uintptr_t *)&_kscePowerSetArmClockFrequency);
    module_get_export_func(KERNEL_PID,
            "ScePower", 0x1590166F, 0xB8D7B3FB, (uintptr_t *)&_kscePowerSetBusClockFrequency);
    module_get_export_func(KERNEL_PID,
            "ScePower", 0x1590166F, 0x264C24FC, (uintptr_t *)&_kscePowerSetGpuEs4ClockFrequency);
    module_get_export_func(KERNEL_PID,
            "ScePower", 0x1590166F, 0xA7739DBE, (uintptr_t *)&_kscePowerSetGpuXbarClockFrequency);

    g_mutex_cpufreq_uid = ksceKernelCreateMutex("psvs_mutex_cpufreq", 0, 0, NULL);
    g_mutex_procevent_uid = ksceKernelCreateMutex("psvs_mutex_procevent", 0, 0, NULL);
    g_mutex_framebuf_uid = ksceKernelCreateMutex("psvs_mutex_framebuf", 0, 0, NULL);

    psvs_oc_init(); // reset profile options to default

    g_hooks[0] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[0],
            "SceDisplay", 0x9FED47AC, 0x16466675, ksceDisplaySetFrameBufInternal_patched);

    g_hooks[1] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[1],
            "SceCtrl", 0xD197E3C7, 0x104ED1A7, sceCtrlPeekBufferNegative_patched);
    g_hooks[2] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[2],
            "SceCtrl", 0xD197E3C7, 0x81A89660, sceCtrlPeekBufferNegative2_patched);
    g_hooks[3] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[3],
            "SceCtrl", 0xD197E3C7, 0xA9C3CED6, sceCtrlPeekBufferPositive_patched);
    g_hooks[4] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[4],
            "SceCtrl", 0xD197E3C7, 0x15F81E8C, sceCtrlPeekBufferPositive2_patched);
    g_hooks[5] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[5],
            "SceCtrl", 0xD197E3C7, 0x15F96FB0, sceCtrlReadBufferNegative_patched);
    g_hooks[6] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[6],
            "SceCtrl", 0xD197E3C7, 0x27A0C5FB, sceCtrlReadBufferNegative2_patched);
    g_hooks[7] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[7],
            "SceCtrl", 0xD197E3C7, 0x67E7AB83, sceCtrlReadBufferPositive_patched);
    g_hooks[8] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[8],
            "SceCtrl", 0xD197E3C7, 0xC4226A3E, sceCtrlReadBufferPositive2_patched);

    g_hooks[9] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[9],
            "ScePower", 0x1590166F, 0x74DB5AE5, kscePowerSetArmClockFrequency_patched);
    g_hooks[10] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[10],
            "ScePower", 0x1590166F, 0xB8D7B3FB, kscePowerSetBusClockFrequency_patched);
    g_hooks[11] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[11],
            "ScePower", 0x1590166F, 0x264C24FC, kscePowerSetGpuEs4ClockFrequency_patched);
    g_hooks[12] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[12],
            "ScePower", 0x1590166F, 0xA7739DBE, kscePowerSetGpuXbarClockFrequency_patched);

    g_hooks[13] = taiHookFunctionImportForKernel(KERNEL_PID, &g_hookrefs[13],
            "SceProcessmgr", 0x887F19D0, 0x414CC813, ksceKernelInvokeProcEventHandler_patched);

    g_hooks[14] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[14],
            "ScePower", 0x1082DA7F, 0xABC6F88F, scePowerGetArmClockFrequency_patched);
    g_hooks[15] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[15],
            "ScePower", 0x1082DA7F, 0x478FE6F5, scePowerGetBusClockFrequency_patched);
    g_hooks[16] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[16],
            "ScePower", 0x1082DA7F, 0x1B04A1D6, scePowerGetGpuClockFrequency_patched);
    g_hooks[17] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[17],
            "ScePower", 0x1082DA7F, 0x0A750DEE, scePowerGetGpuXbarClockFrequency_patched);

    ret = module_get_export_func(KERNEL_PID,
            "SceSysmem", 0x63A519E5, 0x3650963F, (uintptr_t *)&SceSysmemForKernel_0x3650963F); // 3.60
    if (ret < 0) {
        module_get_export_func(KERNEL_PID,
            "SceSysmem", 0x02451F0F, 0xB9B69700, (uintptr_t *)&SceSysmemForKernel_0x3650963F); // 3.65
    }
    module_get_export_func(KERNEL_PID,
            "SceKernelThreadMgr", 0xE2C40624, 0x7E280B69, (uintptr_t *)&SceThreadmgrForDriver_0x7E280B69);
    module_get_export_func(KERNEL_PID,
            "SceLowio", 0xE692C727, 0xE9D95643, (uintptr_t *)&ScePervasiveForDriver_0xE9D95643);

    const uint8_t nop[] = {0x00, 0xBF};
    g_injects[0] = taiInjectAbsForKernel(KERNEL_PID,
            (void *)((uintptr_t)ScePervasiveForDriver_0xE9D95643 + 0x1D), &nop, 2);

    // Load main profile
    snprintf(g_titleid, sizeof(g_titleid), "main");
    psvs_profile_load();

    g_thread_uid = ksceKernelCreateThread("psvs_thread", psvs_thread, 0x3C, 0x3000, 0, 0x10000, 0);
    ksceKernelStartThread(g_thread_uid, 0, NULL);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    if (g_thread_uid >= 0) {
        g_thread_run = 0;
        ksceKernelWaitThreadEnd(g_thread_uid, NULL, NULL);
        ksceKernelDeleteThread(g_thread_uid);
    }

    for (int i = 0; i < PSVS_MAX_HOOKS; i++) {
        if (g_hooks[i] >= 0)
            taiHookReleaseForKernel(g_hooks[i], g_hookrefs[i]);
    }

    if (g_injects[0] >= 0)
        taiInjectReleaseForKernel(g_injects[0]);

    if (g_mutex_cpufreq_uid >= 0)
        ksceKernelDeleteMutex(g_mutex_cpufreq_uid);
    if (g_mutex_procevent_uid >= 0)
        ksceKernelDeleteMutex(g_mutex_procevent_uid);
    if (g_mutex_framebuf_uid >= 0)
        ksceKernelDeleteMutex(g_mutex_framebuf_uid);

    psvs_gui_deinit();

    return SCE_KERNEL_STOP_SUCCESS;
}
