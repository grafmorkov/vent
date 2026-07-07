#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "download.h"

// Helpers
static char* skip_ws(char* s) {
    while (*s && isspace((unsigned char)*s))
        s++;
    return s;
}

static void split_words(char* line, const char** argv, int* argc) {
    *argc = 0;

    line = skip_ws(line);

    while (*line && *argc < 8) {
        char* start = line;

        while (*line && !isspace((unsigned char)*line))
            line++;

        if (*line) {
            *line = '\0';
            line++;
        }

        argv[(*argc)++] = start;
        line = skip_ws(line);
    }
}

ConfigFile* parse_config_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    fseek(f, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, size, f);
    fclose(f);

    if (read_size != (size_t)size) {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';

    Config* items = malloc(sizeof(Config) * (size_t)size);
    size_t count = 0;

    char* line = buffer;

    while (*line) {
        char* end = line;
        while (*end && *end != '\n')
            end++;

        if (*end) {
            *end = '\0';
            end++;
        }

        line = skip_ws(line);

        if (*line == '\0') {
            line = end;
            continue;
        }

        const char* argv[8];
        int argc = 0;

        split_words(line, argv, &argc);

        if (argc > 0) {
            Config* cfg = &items[count++];

            if (strcmp(argv[0], "std") == 0) {
                cfg->type = SOURCE_STD;
            } else if (strcmp(argv[0], "git") == 0) {
                cfg->type = SOURCE_GIT;
            } else if (strcmp(argv[0], "archive") == 0) {
                cfg->type = SOURCE_ARCHIVE;
            } else if (strcmp(argv[0], "system") == 0) {
                cfg->type = SOURCE_SYSTEM;
            } else {
                line = end;
                continue;
            }

            cfg->argc = argc - 1;
            if (cfg->argc > 0) {
                cfg->argv = malloc(sizeof(const char*) * cfg->argc);
                for (int j = 0; j < cfg->argc; j++)
                    cfg->argv[j] = argv[j + 1];
            } else {
                cfg->argv = NULL;
            }
        }

        line = end;
    }

    ConfigFile* cf = malloc(sizeof(ConfigFile));
    if (!cf) {
        free(items);
        free(buffer);
        return NULL;
    }

    cf->items = items;
    cf->count = count;
    cf->buffer = buffer;

    return cf;
}

void free_config(ConfigFile* cf) {
    if (!cf)
        return;

    for (size_t i = 0; i < cf->count; i++)
        free((void*)cf->items[i].argv);

    free(cf->buffer);
    free(cf->items);
    free(cf);
}
/// CONFIG RESOLVEING

// Check file name:
int strip_ext(const char *in, char *out, size_t n) {
    strncpy(out, in, n);
    out[n - 1] = '\0';

    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';

    return 0;
}
// Resolving:
int resolve_config(ConfigFile *cf){
    for(size_t i = 0; i < cf->count; i++){
        Config cfg = cf->items[i];
        switch(cfg.type){

            case SOURCE_STD: {
                DIR *dir = opendir(VENT_STD);
                if (!dir)
                    return -1;

                for (int j = 0; j < cfg.argc; j++) {
                    rewinddir(dir);
                    struct dirent *entry;
                    int found = 0;

                    while ((entry = readdir(dir)) != NULL) {
                        if (strcmp(entry->d_name, ".") == 0 ||
                            strcmp(entry->d_name, "..") == 0)
                            continue;

                        char name[256];
                        strip_ext(entry->d_name, name, sizeof(name));

                        if (strcmp(name, cfg.argv[j]) == 0) {
                            printf("INFO: Found match: %s\n", entry->d_name);
                            char path[1024];
                            snprintf(path, sizeof(path), "%s/%s", VENT_STD, entry->d_name);
                            ConfigFile* sub = parse_config_file(path);
                            if (!sub) {
                                closedir(dir);
                                printf("ERROR: Failed to parse %s\n", path);
                                return -2;
                            }
                            if (!resolve_config(sub)) {
                                free_config(sub);
                                closedir(dir);
                                return -3;
                            }
                            free_config(sub);
                            found = 1;
                            break;
                        }
                    }

                    if (!found)
                        printf("WARNING: No .vent file for '%s' in std/\n", cfg.argv[j]);
                }
                closedir(dir);
                break;
            }

            case SOURCE_GIT:
                if (cfg.argc < 2) {
                    printf("ERROR: git requires 2 arguments\n");
                    return -2;
                }
                clone_repo(cfg.argv[0], cfg.argv[1]);
                break;

            case SOURCE_ARCHIVE: {
                if (cfg.argc < 2) {
                    printf("ERROR: archive requires 2 arguments\n");
                    return -3;
                }

                const char* url = cfg.argv[0];
                const char* out_dir = cfg.argv[1];

                char mkdir_cmd[1024];
                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", out_dir);
                system(mkdir_cmd);

                const char* fname = strrchr(url, '/');
                fname = fname ? fname + 1 : "archive";

                char archive_path[4096];
                snprintf(archive_path, sizeof(archive_path), "%s/%s", out_dir, fname);

                int ret = download(url, archive_path);
                if (ret != 0) {
                    printf("ERROR: Failed to download %s\n", url);
                    return ret;
                }

                ret = extract_archive(archive_path, out_dir);
                if (ret != 0) {
                    printf("ERROR: Failed to extract %s\n", archive_path);
                    return ret;
                }

                break;
            }
            case SOURCE_SYSTEM: {
                VentPM pm = vent_detect_package_manager();
                if (pm == VENT_PM_NONE) {
                    printf("ERROR: No supported package manager found\n");
                    return -4;
                }
                for (int j = 0; j < cfg.argc; j++) {
                    char* cmd = vent_install_command(pm, cfg.argv[j]);
                    if (!cmd) {
                        printf("ERROR: Failed to build install command for '%s'\n", cfg.argv[j]);
                        return -5;
                    }
                    printf("INFO: Installing %s...\n", cfg.argv[j]);
                    int ret = system(cmd);
                    free(cmd);
                    if (ret != 0) {
                        printf("ERROR: Failed to install '%s' (exit code %d)\n", cfg.argv[j], ret);
                        return -6;
                    }
                }
                break;
            }

            default:
                printf("Undefined source type: %d\n", cfg.type);
                return 0;
        }
    }
    return 1;
}
