#include <vitasdkkern.h>
#include <taihen.h>
#include <stdbool.h>
#include <stdio.h>

#include "main.h"
#include "oc.h"

#define PSVS_DIR          "ux0:data/PSVshell/"
#define PSVS_PROFILES_DIR "ux0:data/PSVshell/profiles/"

void psvs_profile_init() {
    ksceIoMkdir(PSVS_DIR, 0777);
    ksceIoMkdir(PSVS_PROFILES_DIR, 0777);
}

bool psvs_profile_load() {
    char path[128];
    snprintf(path, 128, "%s%s", PSVS_PROFILES_DIR, g_titleid);

    SceUID fd = ksceIoOpen(path, SCE_O_RDONLY, 0777);
    if (fd < 0)
        return false;

    psvs_oc_profile_t oc;
    int bytes = ksceIoRead(fd, &oc, sizeof(psvs_oc_profile_t));
    ksceIoClose(fd);

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
    if (fd < 0)
        return false;

    int bytes = ksceIoWrite(fd, psvs_oc_get_profile(), sizeof(psvs_oc_profile_t));
    ksceIoClose(fd);

    if (bytes != sizeof(psvs_oc_profile_t))
        return false;

    psvs_oc_set_changed(false);
    return true;
}

bool psvs_profile_delete() {
    char path[128];
    snprintf(path, 128, "%s%s", PSVS_PROFILES_DIR, g_titleid);

    if (ksceIoRemove(path) < 0)
        return false;

    psvs_oc_set_changed(true);
    return true;
}
