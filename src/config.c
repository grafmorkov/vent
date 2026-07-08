#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "download.h"
#include "ui.h"
#include "paths.h"
#include "threadpool.h"
#include "platform.h"

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
            SourceType type;
            if (strcmp(argv[0], "std") == 0) {
                type = SOURCE_STD;
            } else if (strcmp(argv[0], "git") == 0) {
                type = SOURCE_GIT;
            } else if (strcmp(argv[0], "archive") == 0) {
                type = SOURCE_ARCHIVE;
            } else if (strcmp(argv[0], "system") == 0) {
                type = SOURCE_SYSTEM;
            } else {
                line = end;
                continue;
            }

            Config* cfg = &items[count++];
            cfg->type = type;
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
/// CONFIG RESOLVING

static int strip_ext(const char *in, char *out, size_t n) {
    strncpy(out, in, n);
    out[n - 1] = '\0';
    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
    return 0;
}

static Config config_deep_copy(const Config* src) {
    Config c;
    c.type = src->type;
    c.argc = src->argc;
    if (src->argc > 0 && src->argv) {
        c.argv = malloc(sizeof(const char*) * src->argc);
        for (int j = 0; j < src->argc; j++)
            c.argv[j] = strdup(src->argv[j]);
    } else {
        c.argv = NULL;
    }
    return c;
}

static void config_deep_free(Config* c) {
    if (c->argv) {
        for (int j = 0; j < c->argc; j++)
            free((void*)c->argv[j]);
        free(c->argv);
        c->argv = NULL;
    }
}

// flat config list for parallel dispatch

typedef struct {
    Config* items;
    size_t count;
    size_t cap;
} FlatConfig;

static void flat_init(FlatConfig* f) {
    f->items = NULL;
    f->count = 0;
    f->cap = 0;
}

static void flat_append(FlatConfig* f, Config cfg) {
    if (f->count >= f->cap) {
        f->cap = f->cap ? f->cap * 2 : 64;
        f->items = realloc(f->items, sizeof(Config) * f->cap);
    }
    f->items[f->count++] = cfg;
}

static void flat_free(FlatConfig* f) {
    for (size_t i = 0; i < f->count; i++)
        config_deep_free(&f->items[i]);
    free(f->items);
    f->items = NULL;
    f->count = 0;
    f->cap = 0;
}
static int flatten_std(FlatConfig* flat, const char* name) {
    DIR* dir = opendir(vent_std_path());
    if (!dir) {
        ui_warning_safe("std/ directory not found\n");
        return -1;
    }

    struct dirent* entry;
    int found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char entry_name[256];
        strip_ext(entry->d_name, entry_name, sizeof(entry_name));

        if (strcmp(entry_name, name) == 0) {
            found = 1;
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", vent_std_path(), entry->d_name);
            ConfigFile* sub = parse_config_file(path);
            if (!sub) {
                closedir(dir);
                ui_error_safe("Failed to parse %s\n", path);
                return -2;
            }

            for (size_t i = 0; i < sub->count; i++) {
                Config* item = &sub->items[i];
                if (item->type == SOURCE_STD) {
                    for (int a = 0; a < item->argc; a++)
                        flatten_std(flat, item->argv[a]);
                } else {
                    flat_append(flat, config_deep_copy(item));
                }
            }
            free_config(sub);
            break;
        }
    }

    closedir(dir);
    if (!found)
        ui_warning_safe("No .vent file for '%s' in std/\n", name);
    return 0;
}

// thread-pool job functions

typedef struct {
    Config cfg;
    double t0;
} JobData;

static int do_git(void* data) {
    JobData* jd = (JobData*)data;
    const char* url = jd->cfg.argv[0];
    const char* path = jd->cfg.argv[1];

    char* cache_path = vent_cache_path_for_url(url);

    // if path already symlinks to cache -> skip
    struct stat st;
    if (vent_lstat(path, &st) == 0) {
        if (vent_is_symlink(st.st_mode)) {
            char link[4096];
            if (vent_readlink(path, link, sizeof(link)) == 0) {
                if (strcmp(link, cache_path) == 0) {
                    ui_print_resolve_item_safe(path, "cached", ui_now() - jd->t0, 1);
                    free(cache_path);
                    return 0;
                }
            }
            remove(path);
        } else if (vent_is_dir(st.st_mode)) {
            ui_warning_safe("Replacing existing directory with symlink: %s\n", path);
            vent_rm_rf(path);
        }
    }

    // clone to cache if missing
    if (vent_lstat(cache_path, &st) != 0 || !vent_is_dir(st.st_mode)) {
        // ensure parent of cache_path exists
        char p[4096];
        snprintf(p, sizeof(p), "%s", cache_path);
        char* s = strrchr(p, '/');
        if (s) {
            *s = '\0';
            vent_mkdir_p(p);
        }

        int ret = clone_repo(url, cache_path);
        if (ret != 0) {
            ui_print_resolve_item_safe(url, "git clone", ui_now() - jd->t0, 0);
            free(cache_path);
            return ret;
        }
    }

    // ensure parent of path exists
    {
        char p[4096];
        snprintf(p, sizeof(p), "%s", path);
        char* s = strrchr(p, '/');
        if (s) {
            *s = '\0';
            vent_mkdir_p(p);
        }
    }

    if (vent_symlink(cache_path, path) != 0) {
        ui_error_safe("Failed to create symlink %s -> %s\n", path, cache_path);
        free(cache_path);
        return -1;
    }

    ui_print_resolve_item_safe(path, "git clone", ui_now() - jd->t0, 1);
    free(cache_path);
    return 0;
}

static int do_archive(void* data) {
    JobData* jd = (JobData*)data;
    const char* url = jd->cfg.argv[0];
    const char* out_dir = jd->cfg.argv[1];

    char* cache_path = vent_cache_path_for_url(url);

    // if out_dir already symlinks to cache -> skip
    struct stat st;
    if (vent_lstat(out_dir, &st) == 0) {
        if (vent_is_symlink(st.st_mode)) {
            char link[4096];
            if (vent_readlink(out_dir, link, sizeof(link)) == 0) {
                if (strcmp(link, cache_path) == 0) {
                    ui_print_resolve_item_safe(out_dir, "cached", ui_now() - jd->t0, 1);
                    free(cache_path);
                    return 0;
                }
            }
            remove(out_dir);
        } else if (vent_is_dir(st.st_mode)) {
            ui_warning_safe("Replacing existing directory with symlink: %s\n", out_dir);
            vent_rm_rf(out_dir);
        }
    }

    // download + extract to cache if missing
    if (vent_lstat(cache_path, &st) != 0 || !vent_is_dir(st.st_mode)) {
        vent_mkdir_p(cache_path);

        const char* fname = strrchr(url, '/');
        fname = fname ? fname + 1 : "archive";

        char archive_path[4096];
        snprintf(archive_path, sizeof(archive_path), "%s/%s", cache_path, fname);

        int ret = download(url, archive_path);
        if (ret != 0) {
            ui_print_resolve_item_safe(fname, "download", ui_now() - jd->t0, 0);
            free(cache_path);
            return ret;
        }

        ret = extract_archive(archive_path, cache_path);
        if (ret != 0) {
            ui_print_resolve_item_safe(fname, "extract", ui_now() - jd->t0, 0);
            free(cache_path);
            return ret;
        }
    }

    // ensure parent of out_dir exists
    {
        char p[4096];
        snprintf(p, sizeof(p), "%s", out_dir);
        char* s = strrchr(p, '/');
        if (s) {
            *s = '\0';
            vent_mkdir_p(p);
        }
    }

    if (vent_symlink(cache_path, out_dir) != 0) {
        ui_error_safe("Failed to create symlink %s -> %s\n", out_dir, cache_path);
        free(cache_path);
        return -1;
    }

    ui_print_resolve_item_safe(out_dir, "archive", ui_now() - jd->t0, 1);
    free(cache_path);
    return 0;
}

static int do_system(void* data) {
    JobData* jd = (JobData*)data;
    VentPM pm = vent_detect_package_manager();
    if (pm == VENT_PM_NONE) {
        ui_error_safe("No supported package manager found\n");
        return -4;
    }

    static const char* pm_names[] = {
        "?", "apt", "dnf", "pacman", "zypper",
        "apk", "xbps", "emerge", "brew",
        "winget", "choco"
    };
    int pm_count = sizeof(pm_names) / sizeof(pm_names[0]);

    int ret = 0;
    for (int j = 0; j < jd->cfg.argc; j++) {
        const char* pkg = jd->cfg.argv[j];

        if (vent_check_system_package(pm, pkg)) {
            ui_print_resolve_item_safe(pkg, pm_names[(int)pm], ui_now() - jd->t0, 1);
            continue;
        }

        char* cmd = vent_install_command(pm, pkg);
        if (!cmd) {
            ui_print_resolve_item_safe(pkg, pm_names[(int)pm],
                                       ui_now() - jd->t0, 0);
            ret = -5;
            break;
        }
        int rc = system(cmd);
        free(cmd);
        int idx = (int)pm;
        if (idx < 0 || idx >= pm_count) idx = 0;
        ui_print_resolve_item_safe(pkg, pm_names[idx],
                                   ui_now() - jd->t0, rc == 0);
        if (rc != 0) {
            ret = -6;
            break;
        }
    }
    return ret;
}

int resolve_config(ConfigFile *cf, int jobs) {
    FlatConfig flat;
    flat_init(&flat);

    for (size_t i = 0; i < cf->count; i++) {
        Config* item = &cf->items[i];
        if (item->type == SOURCE_STD) {
            for (int a = 0; a < item->argc; a++)
                flatten_std(&flat, item->argv[a]);
        } else {
            flat_append(&flat, config_deep_copy(item));
        }
    }

    if (flat.count == 0) {
        flat_free(&flat);
        return 1;
    }

    ThreadPool* pool = tp_create(jobs);
    if (!pool) {
        flat_free(&flat);
        return 0;
    }

    for (size_t i = 0; i < flat.count; i++) {
        JobData* jd = malloc(sizeof(JobData));
        jd->cfg = flat.items[i];
        jd->t0 = ui_now();

        JobFunc func = NULL;
        switch (jd->cfg.type) {
            case SOURCE_GIT:     func = do_git;     break;
            case SOURCE_ARCHIVE: func = do_archive; break;
            case SOURCE_SYSTEM:  func = do_system;  break;
            default: free(jd); continue;
        }
        tp_enqueue(pool, func, jd);
    }

    int result = tp_wait(pool);
    tp_destroy(pool);

    flat_free(&flat);

    return result == 0 ? 1 : 0;
}
