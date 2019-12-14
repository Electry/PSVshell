#include <vitasdkkern.h>
#include <taihen.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "oc.h"

#define PSVS_PROFILES_DIR "ur0:data/PSVshell/profiles/"

static bool g_profile_exists = false;
static bool g_profile_exists_global = false;

void psvs_profile_init() {
    ksceIoMkdir("ur0:data/", 0777);
    ksceIoMkdir("ur0:data/PSVshell/", 0777);
    ksceIoMkdir(PSVS_PROFILES_DIR, 0777);
}

bool psvs_profile_load() {
    g_profile_exists = false;
    g_profile_exists_global = false;

    SceUID fd = -1;
    char path[128];
    snprintf(path, 128, "%s%s", PSVS_PROFILES_DIR, g_titleid);

    // always check both so we can tell if both of them exist
    SceUID fd_title = ksceIoOpen(path, SCE_O_RDONLY, 0777);
    SceUID fd_global = ksceIoOpen(PSVS_PROFILES_DIR "global", SCE_O_RDONLY, 0777);
    if (fd_title < 0 && fd_global < 0)
        return false;

    // default to global profile
    if (fd_global >= 0) {
        fd = fd_global;
        g_profile_exists_global = true;
    }

    // if present, title profile has precedence
    if (fd_title >= 0) {
        fd = fd_title;
        g_profile_exists = true;
        if (fd_global >= 0) // global profile is not needed when both are present
            ksceIoClose(fd_global);
    }

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

bool psvs_profile_save(bool global) {
    SceUID fd;

    if (!global) {
        char path[128];
        snprintf(path, 128, "%s%s", PSVS_PROFILES_DIR, g_titleid);
        fd = ksceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    } else {
        fd = ksceIoOpen(PSVS_PROFILES_DIR "global", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    }

    if (fd < 0)
        return false;

    int bytes = ksceIoWrite(fd, psvs_oc_get_profile(), sizeof(psvs_oc_profile_t));
    ksceIoClose(fd);

    if (bytes != sizeof(psvs_oc_profile_t))
        return false;

    // mark profile as present
    if (global) {
        g_profile_exists_global = true;
    } else {
        g_profile_exists = true;
        psvs_oc_set_changed(false);
    }

    return true;
}

bool psvs_profile_delete(bool global) {
    if (!global) {
        char path[128];
        snprintf(path, 128, "%s%s", PSVS_PROFILES_DIR, g_titleid);

        if (ksceIoRemove(path) < 0)
            return false;

        g_profile_exists = false;
        psvs_oc_set_changed(true);
    } else {
        if (ksceIoRemove(PSVS_PROFILES_DIR "global") < 0)
            return false;

        g_profile_exists_global = false;
    }

    return true;
}

bool psvs_profile_exists(bool global) {
    return global ? g_profile_exists_global : g_profile_exists;
}
