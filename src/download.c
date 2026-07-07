#include "download.h"
#include "config.h"
#include "ui.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

static char* format(const char* fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0)
        return NULL;

    char* buf = malloc(n + 1);
    if (!buf)
        return NULL;

    va_start(ap, fmt);
    vsnprintf(buf, n + 1, fmt, ap);
    va_end(ap);

    return buf;
}

static int is_executable(const char* path) {
    return access(path, X_OK) == 0;
}

int download(const char* url, const char* out){
    CURL* curl = curl_easy_init();
    if (!curl)
        return -1;

    FILE* f = fopen(out, "wb");
    if (!f) {
        curl_easy_cleanup(curl);
        return -2;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    CURLcode res = curl_easy_perform(curl);

    fclose(f);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return -3;

    return 0;
}
int clone_repo(const char* repo_name, const char* path){

    char command[1024];

    snprintf(command,
             sizeof(command),
             "git clone \"%s\" \"%s\"",
             repo_name,
             path);

    return system(command);
}

static int has_suffix(const char* str, const char* suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len)
        return 0;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static const char* filename_from_url(const char* url) {
    const char* slash = strrchr(url, '/');
    return slash ? slash + 1 : url;
}

int extract_archive(const char* archive_path, const char* out_dir){
    const char* name = filename_from_url(archive_path);
    char cmd[4096];
    int ret;

    if (has_suffix(name, ".tar.gz") || has_suffix(name, ".tgz")) {
        snprintf(cmd, sizeof(cmd), "tar -xzf \"%s\" -C \"%s\"", archive_path, out_dir);
    } else if (has_suffix(name, ".tar.bz2") || has_suffix(name, ".tbz2")) {
        snprintf(cmd, sizeof(cmd), "tar -xjf \"%s\" -C \"%s\"", archive_path, out_dir);
    } else if (has_suffix(name, ".tar.xz") || has_suffix(name, ".txz")) {
        snprintf(cmd, sizeof(cmd), "tar -xJf \"%s\" -C \"%s\"", archive_path, out_dir);
    } else if (has_suffix(name, ".tar.zst") || has_suffix(name, ".tzst")) {
        snprintf(cmd, sizeof(cmd), "tar --zstd -xf \"%s\" -C \"%s\"", archive_path, out_dir);
    } else if (has_suffix(name, ".tar")) {
        snprintf(cmd, sizeof(cmd), "tar -xf \"%s\" -C \"%s\"", archive_path, out_dir);
    } else if (has_suffix(name, ".zip")) {
        snprintf(cmd, sizeof(cmd), "unzip -q \"%s\" -d \"%s\"", archive_path, out_dir);
    } else {
        ui_error("Unsupported archive format: %s\n", name);
        return -1;
    }

    ui_info("Extracting %s...\n", name);
    ret = system(cmd);
    if (ret == 0)
        remove(archive_path);
    return ret;
}

char* vent_install_command(VentPM pm, const char* package){
    switch (pm){
        case VENT_PM_APT: return format("sudo apt install -y %s", package);
        case VENT_PM_DNF: return format("sudo dnf install -y %s", package);
        case VENT_PM_PACMAN: return format("sudo pacman -S --needed --noconfirm %s", package);
        case VENT_PM_ZYPPER: return format("sudo zypper --non-interactive install %s", package);
        case VENT_PM_APK: return format("sudo apk add %s", package);
        case VENT_PM_XBPS: return format("sudo xbps-install -Sy %s", package);
        case VENT_PM_EMERGE: return format("sudo emerge %s", package);
        case VENT_PM_BREW: return format("brew install %s", package);

        default:
            return NULL;
    }
}
VentPM vent_detect_package_manager(void){
    if (is_executable("/usr/bin/apt"))
        return VENT_PM_APT;

    if (is_executable("/usr/bin/dnf"))
        return VENT_PM_DNF;

    if (is_executable("/usr/bin/pacman"))
        return VENT_PM_PACMAN;

    if (is_executable("/usr/bin/zypper"))
        return VENT_PM_ZYPPER;

    if (is_executable("/sbin/apk") ||
        is_executable("/usr/bin/apk"))
        return VENT_PM_APK;

    if (is_executable("/usr/bin/xbps-install"))
        return VENT_PM_XBPS;

    if (is_executable("/usr/bin/emerge"))
        return VENT_PM_EMERGE;

    if (is_executable("/usr/bin/brew"))
        return VENT_PM_BREW;

    return VENT_PM_NONE;
}
