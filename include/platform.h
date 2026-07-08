#ifndef PLATFORM_H
#define PLATFORM_H

#include <sys/stat.h>
#include <stddef.h>

const char* vent_home_dir(void);
int vent_mkdir_p(const char* path);
int vent_rm_rf(const char* path);
int vent_symlink(const char* target, const char* linkpath);
int vent_readlink(const char* path, char* buf, size_t size);
int vent_lstat(const char* path, struct stat* st);
int vent_is_symlink(mode_t mode);
int vent_is_dir(mode_t mode);
int vent_access_x(const char* path);
double vent_now(void);
void vent_init_console(void);

#endif
