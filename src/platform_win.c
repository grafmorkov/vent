#include "platform.h"
#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define vent_strcasecmp _stricmp

#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif
#ifndef S_IFMT
#define S_IFMT 0170000
#endif

const char* vent_home_dir(void) {
    const char* home = getenv("USERPROFILE");
    if (home && *home)
        return home;
    return "C:\\Users\\Default";
}

int vent_mkdir_p(const char* path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            CreateDirectoryA(tmp, NULL);
            *p = '/';
        }
    }
    return CreateDirectoryA(tmp, NULL) ? 0 : -1;
}

int vent_rm_rf(const char* path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp; *p; p++) {
        if (*p == '/') *p = '\\';
    }

    SHFILEOPSTRUCTA fos;
    memset(&fos, 0, sizeof(fos));
    fos.wFunc = FO_DELETE;
    fos.pFrom = tmp;
    fos.fFlags = FOF_NO_UI | FOF_SILENT | FOF_NOCONFIRMATION;
    return SHFileOperationA(&fos);
}

int vent_symlink(const char* target, const char* linkpath) {
    DWORD flags = 0;
    struct stat st;
    if (stat(target, &st) == 0 && S_ISDIR(st.st_mode))
        flags = SYMBOLIC_LINK_FLAG_DIRECTORY;

    if (CreateSymbolicLinkA(linkpath, target, flags))
        return 0;

    return -1;
}

int vent_readlink(const char* path, char* buf, size_t size) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;

    DWORD len = GetFinalPathNameByHandleA(h, buf, (DWORD)size, VOLUME_NAME_DOS);
    CloseHandle(h);

    if (len == 0 || len >= size)
        return -1;

    buf[len] = '\0';

    // GetFinalPathNameByHandle returns \\?\C:\... prefix; strip it
    if (strncmp(buf, "\\\\?\\", 4) == 0) {
        memmove(buf, buf + 4, strlen(buf + 4) + 1);
        if (buf[1] == ':') {
            // already a DOS path
        }
    }

    return 0;
}

int vent_lstat(const char* path, struct stat* st) {
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return -1;

    memset(st, 0, sizeof(*st));
    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        st->st_mode = S_IFDIR;
    else
        st->st_mode = S_IFREG;

    if (attrs & FILE_ATTRIBUTE_REPARSE_POINT)
        st->st_mode |= S_IFLNK;

    return 0;
}

int vent_is_symlink(mode_t mode) {
    return (mode & S_IFMT) == S_IFLNK;
}

int vent_is_dir(mode_t mode) {
    return (mode & S_IFMT) == S_IFDIR;
}

int vent_access_x(const char* path) {
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return -1;

    const char* ext = strrchr(path, '.');
    if (ext && (vent_strcasecmp(ext, ".exe") == 0 ||
                vent_strcasecmp(ext, ".com") == 0 ||
                vent_strcasecmp(ext, ".bat") == 0 ||
                vent_strcasecmp(ext, ".cmd") == 0 ||
                vent_strcasecmp(ext, ".ps1") == 0))
        return 0;

    return -1;
}

double vent_now(void) {
    static LARGE_INTEGER freq;
    static int initialized = 0;
    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)freq.QuadPart;
}

void vent_init_console(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
        return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
        return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
