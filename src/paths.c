#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "paths.h"
#include "sha256.h"
#include "platform.h"

static char std_repo_path[4096];
static int std_repo_ok = 0;

const char* vent_std_repo_path(void) {
    snprintf(std_repo_path, sizeof(std_repo_path),
             "%s/.vent/std-repo", vent_home_dir());
    return std_repo_path;
}

int vent_std_repo_ensure(void) {
    const char* repo = vent_std_repo_path();
    struct stat st;

    if (vent_lstat(repo, &st) == 0 && vent_is_dir(st.st_mode)) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "git -C \"%s\" pull --quiet", repo);
        int rc = system(cmd);
        std_repo_ok = (rc == 0);
        return std_repo_ok;
    }

    vent_mkdir_p(vent_home_dir());

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "git clone --depth 1 \"%s\" \"%s\"", VENT_STD_REPO_URL, repo);
    int rc = system(cmd);
    std_repo_ok = (rc == 0);
    return std_repo_ok;
}

const char* vent_std_path(void) {
    const char* env = getenv("VENT_STD");
    if (env && *env)
        return env;

    if (std_repo_ok)
        return vent_std_repo_path();

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
