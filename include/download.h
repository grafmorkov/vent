#ifndef DOWNLOAD_H
#define DOWNLOAD_H

// default
int download(const char* url, const char* out);
int clone_repo(const char* repo_name, const char* path);
int extract_archive(const char* archive_path, const char* out_dir);

// system
typedef enum {
    VENT_PM_NONE,
    VENT_PM_APT,
    VENT_PM_DNF,
    VENT_PM_PACMAN,
    VENT_PM_ZYPPER,
    VENT_PM_APK,
    VENT_PM_XBPS,
    VENT_PM_EMERGE,
    VENT_PM_BREW,
    VENT_PM_WINGET,
    VENT_PM_CHOCO
} VentPM;

VentPM vent_detect_package_manager(void);

char* vent_install_command(VentPM pm, const char* package);

int vent_check_system_package(VentPM pm, const char* package);

#endif // DOWNLOAD_H
