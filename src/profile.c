#include <vitasdkkern.h>
#include <taihen.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "oc.h"
#include "log.h"

#define PSVS_PROFILES_DIR "ur0:data/PSVshell/profiles/"

void psvs_profile_init() {
    ksceIoMkdir("ur0:data/", 0777);
    ksceIoMkdir("ur0:data/PSVshell/", 0777);
    ksceIoMkdir(PSVS_PROFILES_DIR, 0777);
}

bool psvs_profile_load() {
    char path[128];
    snprintf(path, 128, "%s%s", PSVS_PROFILES_DIR, g_titleid);

    SceUID fd = ksceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        psvs_log_printf("psvs_profile_load(): failed to open '%s'! 0x%X\n", path, fd);
        return false;
    }

    psvs_oc_profile_t oc;
    int bytes = ksceIoRead(fd, &oc, sizeof(psvs_oc_profile_t));
    ksceIoClose(fd);
    psvs_log_printf("psvs_profile_load(): read %d bytes from '%s'\n", bytes, path);

    if (bytes != sizeof(psvs_oc_profile_t))
        return false;

    if (strncmp(oc.ver, PSVS_VERSION_VER, 8))
        return false;

    psvs_oc_set_profile(&oc);
    return true;
}

bool psvs_profile_save() {
    char path[128];
    snprintf(path, 128, "%s%s", PSVS_PROFILES_DIR, g_titleid);

    SceUID fd = ksceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) {
        psvs_log_printf("psvs_profile_save(): failed to open '%s'! 0x%X\n", path, fd);
        return false;
    }

    int bytes = ksceIoWrite(fd, psvs_oc_get_profile(), sizeof(psvs_oc_profile_t));
    ksceIoClose(fd);
    psvs_log_printf("psvs_profile_save(): written %d bytes to '%s'\n", bytes, path);

    if (bytes != sizeof(psvs_oc_profile_t))
        return false;

    psvs_oc_set_changed(false);
    return true;
}

bool psvs_profile_delete() {
    char path[128];
    snprintf(path, 128, "%s%s", PSVS_PROFILES_DIR, g_titleid);

    int ret = ksceIoRemove(path);
    psvs_log_printf("psvs_profile_delete(): deleting '%s': 0x%X\n", path, ret);
    if (ret < 0)
        return false;

    psvs_oc_set_changed(true);
    return true;
}
