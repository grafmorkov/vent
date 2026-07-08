#include "download.h"
#include "config.h"
#include "ui.h"
#include "platform.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <archive.h>
#include "threadpool.h"
#include <archive_entry.h>

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
    return vent_access_x(path) == 0;
}

static const char* detect_os_id(void) {
#ifdef _WIN32
    return "windows";
#else
    FILE* f = fopen("/etc/os-release", "r");
    if (!f)
        return NULL;

    static char id[64];
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "ID=%63s", id) == 1) {
            size_t len = strlen(id);
            if (len > 1 && (id[0] == '\'' || id[0] == '"')) {
                memmove(id, id + 1, len);
                len--;
            }
            if (len > 0 && (id[len - 1] == '\'' || id[len - 1] == '"'))
                id[len - 1] = '\0';
            fclose(f);
            return id;
        }
    }
    fclose(f);
    return NULL;
#endif
}

static VentPM os_id_to_pm(const char* id) {
    if (!id) return VENT_PM_NONE;
    if (strcmp(id, "windows") == 0) return VENT_PM_WINGET;
    if (strcmp(id, "gentoo") == 0) return VENT_PM_EMERGE;
    if (strcmp(id, "alpine") == 0) return VENT_PM_APK;
    if (strcmp(id, "void") == 0) return VENT_PM_XBPS;
    if (strcmp(id, "arch") == 0 || strcmp(id, "manjaro") == 0 ||
        strcmp(id, "endeavouros") == 0) return VENT_PM_PACMAN;
    if (strcmp(id, "fedora") == 0 || strcmp(id, "rhel") == 0 ||
        strcmp(id, "centos") == 0) return VENT_PM_DNF;
    if (strcmp(id, "opensuse") == 0 || strcmp(id, "opensuse-tumbleweed") == 0 ||
        strcmp(id, "opensuse-leap") == 0 || strcmp(id, "suse") == 0) return VENT_PM_ZYPPER;
    if (strcmp(id, "debian") == 0 || strcmp(id, "ubuntu") == 0 ||
        strcmp(id, "linuxmint") == 0 || strcmp(id, "pop") == 0 ||
        strcmp(id, "elementary") == 0 || strcmp(id, "kali") == 0) return VENT_PM_APT;
    return VENT_PM_NONE;
}

static int progress_callback(void* clientp, curl_off_t dltotal,
                             curl_off_t dlnow, curl_off_t ultotal,
                             curl_off_t ulnow) {
    (void)clientp;
    (void)ultotal;
    (void)ulnow;

    if (dltotal <= 0)
        return 0;

    int bar_width = 20;
    int filled = (int)(dlnow * bar_width / dltotal);
    int percent = (int)(dlnow * 100 / dltotal);

    ui_lock();
    fprintf(stderr, "\r%*.*s%*.*s %3d%%",
            filled, filled, "--------------------",
            bar_width - filled, bar_width - filled, "                    ",
            percent);
    if (dlnow >= dltotal)
        fprintf(stderr, "\n");
    fflush(stderr);
    ui_unlock();

    return 0;
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
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

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
             "git clone --depth 1 \"%s\" \"%s\"",
             repo_name,
             path);

    return system(command);
}

static const char* filename_from_url(const char* url) {
    const char* slash = strrchr(url, '/');
    return slash ? slash + 1 : url;
}

int extract_archive(const char* archive_path, const char* out_dir){
    const char* name = filename_from_url(archive_path);
    struct archive* a = archive_read_new();
    if (!a) {
        ui_error("Failed to create archive reader\n");
        return -1;
    }

    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    int r = archive_read_open_filename(a, archive_path, 10240);
    if (r != ARCHIVE_OK) {
        ui_error("Failed to open archive %s: %s\n", name, archive_error_string(a));
        archive_read_free(a);
        return -2;
    }

    ui_info("Extracting %s...\n", name);

    struct archive_entry* entry;
    int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
                ARCHIVE_EXTRACT_SECURE_SYMLINKS | ARCHIVE_EXTRACT_SECURE_NODOTDOT;

    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const char* pathname = archive_entry_pathname(entry);
        if (!pathname)
            continue;

        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", out_dir, pathname);

        archive_entry_set_pathname(entry, fullpath);
        r = archive_read_extract(a, entry, flags);
        if (r != ARCHIVE_OK) {
            ui_error("Failed to extract %s: %s\n", pathname, archive_error_string(a));
            archive_read_free(a);
            return -3;
        }
    }

    if (r != ARCHIVE_EOF) {
        ui_error("Error reading archive %s: %s\n", name, archive_error_string(a));
        archive_read_free(a);
        return -4;
    }

    archive_read_close(a);
    archive_read_free(a);

    remove(archive_path);
    return 0;
}

int vent_check_system_package(VentPM pm, const char* package) {
    char cmd[4096];
    switch (pm) {
        case VENT_PM_WINGET:
            snprintf(cmd, sizeof(cmd),
                "winget list --name \"%s\" >nul 2>&1", package);
            break;
        case VENT_PM_CHOCO:
            snprintf(cmd, sizeof(cmd),
                "choco list --local-only \"%s\" >nul 2>&1", package);
            break;
        case VENT_PM_APT:
            snprintf(cmd, sizeof(cmd),
                "dpkg -s \"%s\" 2>/dev/null | grep -q \"Status: install ok installed\"",
                package);
            break;
        case VENT_PM_DNF:
            snprintf(cmd, sizeof(cmd), "rpm -q \"%s\" >/dev/null 2>&1", package);
            break;
        case VENT_PM_PACMAN:
            snprintf(cmd, sizeof(cmd), "pacman -Qi \"%s\" >/dev/null 2>&1", package);
            break;
        case VENT_PM_ZYPPER:
            snprintf(cmd, sizeof(cmd), "rpm -q \"%s\" >/dev/null 2>&1", package);
            break;
        case VENT_PM_APK:
            snprintf(cmd, sizeof(cmd), "apk info -e \"%s\" >/dev/null 2>&1", package);
            break;
        case VENT_PM_XBPS:
            snprintf(cmd, sizeof(cmd), "xbps-query \"%s\" >/dev/null 2>&1", package);
            break;
        case VENT_PM_EMERGE:
            snprintf(cmd, sizeof(cmd),
                "ls -d /var/db/pkg/*/\"%s\"-* >/dev/null 2>&1", package);
            break;
        case VENT_PM_BREW:
            snprintf(cmd, sizeof(cmd), "brew list \"%s\" >/dev/null 2>&1", package);
            break;
        default:
            return 0;
    }
    return system(cmd) == 0;
}

char* vent_install_command(VentPM pm, const char* package){
    switch (pm){
        case VENT_PM_WINGET: return format("winget install --name \"%s\" --accept-source-agreements", package);
        case VENT_PM_CHOCO: return format("choco install %s -y", package);
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
    const char* os_id = detect_os_id();
    VentPM pm = os_id_to_pm(os_id);

    if (pm == VENT_PM_WINGET) {
        if (is_executable("winget") || is_executable("winget.exe"))
            return VENT_PM_WINGET;
        if (is_executable("choco") || is_executable("choco.exe"))
            return VENT_PM_CHOCO;
        return VENT_PM_NONE;
    }

    if (pm != VENT_PM_NONE) {
        switch (pm) {
            case VENT_PM_EMERGE:
                if (is_executable("/usr/bin/emerge")) return pm;
                break;
            case VENT_PM_APK:
                if (is_executable("/sbin/apk") || is_executable("/usr/bin/apk")) return pm;
                break;
            case VENT_PM_XBPS:
                if (is_executable("/usr/bin/xbps-install")) return pm;
                break;
            case VENT_PM_PACMAN:
                if (is_executable("/usr/bin/pacman")) return pm;
                break;
            case VENT_PM_DNF:
                if (is_executable("/usr/bin/dnf")) return pm;
                break;
            case VENT_PM_ZYPPER:
                if (is_executable("/usr/bin/zypper")) return pm;
                break;
            case VENT_PM_APT:
                if (is_executable("/usr/bin/apt")) return pm;
                break;
            default:
                break;
        }
    }

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
