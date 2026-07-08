#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "paths.h"
#include "sha256.h"

static const char* vent_home(void) {
    const char* home = getenv("HOME");
    if (home && *home)
        return home;
    return "/tmp";
}

static void ensure_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return;
    mkdir(path, 0755);
}

const char* vent_std_path(void) {
    const char* env = getenv("VENT_STD");
    if (env && *env)
        return env;

    return VENT_STD_INSTALL;
}

const char* vent_cache_dir(void) {
    static char cache_path[4096];
    static int initialized = 0;
    if (!initialized) {
        char base[4096];
        snprintf(base, sizeof(base), "%s/.vent", vent_home());
        ensure_dir(base);
        snprintf(cache_path, sizeof(cache_path), "%s/.vent/cache", vent_home());
        ensure_dir(cache_path);
        initialized = 1;
    }
    return cache_path;
}

char* vent_cache_path_for_url(const char* url) {
    char hash[65];
    sha256_string(url, hash);
    char* path = malloc(4096);
    snprintf(path, 4096, "%s/%s", vent_cache_dir(), hash);
    return path;
}
