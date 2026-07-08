#include "platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <libgen.h>

const char* vent_home_dir(void) {
    const char* home = getenv("HOME");
    if (home && *home)
        return home;
    return "/tmp";
}

int vent_mkdir_p(const char* path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

int vent_rm_rf(const char* path) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
    return system(cmd);
}

int vent_symlink(const char* target, const char* linkpath) {
    return symlink(target, linkpath);
}

int vent_readlink(const char* path, char* buf, size_t size) {
    ssize_t len = readlink(path, buf, size - 1);
    if (len < 0)
        return -1;
    buf[len] = '\0';
    return 0;
}

int vent_lstat(const char* path, struct stat* st) {
    return lstat(path, st);
}

int vent_is_symlink(mode_t mode) {
    return S_ISLNK(mode);
}

int vent_is_dir(mode_t mode) {
    return S_ISDIR(mode);
}

int vent_access_x(const char* path) {
    return access(path, X_OK);
}

double vent_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void vent_init_console(void) {
    // no-op on POSIX
}
