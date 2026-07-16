#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "paths.h"
#include "sha256.h"
#include "platform.h"

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
        snprintf(base, sizeof(base), "%s/.vent", vent_home_dir());
        vent_mkdir_p(base);
        snprintf(cache_path, sizeof(cache_path), "%s/.vent/cache", vent_home_dir());
        vent_mkdir_p(cache_path);
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

int vent_cache_clean(void) {
    const char* cache = vent_cache_dir();
    return vent_rm_rf(cache);
}
