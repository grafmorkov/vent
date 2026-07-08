#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <dirent.h>

typedef enum {
    SOURCE_STD,
    SOURCE_GIT,
    SOURCE_ARCHIVE,
    SOURCE_SYSTEM,
} SourceType;

typedef struct {
    SourceType type;
    int argc;
    const char** argv;
} Config;

typedef struct {
    Config* items;
    size_t count;
    char* buffer;
} ConfigFile;

ConfigFile* parse_config_file(const char* path);
void free_config(ConfigFile* cf);

int resolve_config(ConfigFile *cf, int jobs);

#endif
