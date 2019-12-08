#include <vitasdkkern.h>
#include <taihen.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "log.h"

#define LOG_PATH "ur0:data/PSVshell/log.txt"

static SceUID g_mutex_log_uid = -1;
int g_fd = 0;

void psvs_log_init() {
    SceUID fd = ksceIoOpen(LOG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    g_fd = fd;
    if (fd >= 0)
        ksceIoClose(fd);

    g_mutex_log_uid = ksceKernelCreateMutex("psvs_mutex_log", 0, 0, NULL);
}

void psvs_log_printf(const char *format, ...) {
    int ret = ksceKernelLockMutex(g_mutex_log_uid, 1, NULL);
    if (ret < 0)
        return;

    va_list args;
    va_start(args, format);

    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);

    va_end(args);

    SceUID fd = ksceIoOpen(LOG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
    g_fd = fd;
    if (fd < 0) {
        ksceKernelUnlockMutex(g_mutex_log_uid, 1);
        return;
    }

    ksceIoWrite(fd, buffer, strlen(buffer));
    ksceIoClose(fd);
    ksceKernelUnlockMutex(g_mutex_log_uid, 1);
}
