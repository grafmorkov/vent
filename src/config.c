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

#define MAX_WORDS 24

static void split_words(char* line, const char** argv, int* argc) {
    *argc = 0;

    line = skip_ws(line);

    while (*line && *argc < MAX_WORDS) {
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

static SourceType source_from_verb(const char* v) {
    if (strcmp(v, "std") == 0)     return SOURCE_STD;
    if (strcmp(v, "git") == 0)     return SOURCE_GIT;
    if (strcmp(v, "archive") == 0) return SOURCE_ARCHIVE;
    if (strcmp(v, "system") == 0)  return SOURCE_SYSTEM;
    if (strcmp(v, "path") == 0)    return SOURCE_PATH;
    return (SourceType)-1;
}

static int apply_section_piece(const char* p, size_t n, VentConds* c) {
    char buf[32];
    if (n == 0 || n >= sizeof(buf))
        return -1;
    memcpy(buf, p, n);
    buf[n] = '\0';

    if (strcmp(buf, "quant") == 0) {
        c->lang = LANG_QUANT;
    }
    else if (strcmp(buf, "qu") == 0) {
        c->lang = LANG_QUANT;
    }
    else if (strcmp(buf, "cpp") == 0) {
        c->lang = LANG_CPP;
    }
    else if (strcmp(buf, "c") == 0) {
        c->lang = LANG_C;
    }
    else if (strcmp(buf, "linux") == 0) {
        c->os = VOS_LINUX;
    }
    else if (strcmp(buf, "windows") == 0) {
        c->os = VOS_WINDOWS;
    }
    else if (strcmp(buf, "macos") == 0) {
        c->os = VOS_MACOS;
    }
    else if (strcmp(buf, "zeropoint") == 0) {
        c->os = VOS_ZEROPOINT;
    }
    else if (strcmp(buf, "x86_64") == 0) {
        c->arch = VARCH_X86_64;
    }
    else if (strcmp(buf, "aarch64") == 0) {
        c->arch = VARCH_AARCH64;
    }
    else if (strcmp(buf, "x86_64-linux") == 0) {
        c->os = VOS_LINUX; c->arch = VARCH_X86_64;
    }
    else if (strcmp(buf, "aarch64-linux") == 0) {
        c->os = VOS_LINUX; c->arch = VARCH_AARCH64;
    }
    else if (strcmp(buf, "x86_64-windows") == 0) {
        c->os = VOS_WINDOWS; c->arch = VARCH_X86_64;
    }
    else if (strcmp(buf, "aarch64-zeropoint") == 0) {
        c->os = VOS_ZEROPOINT; c->arch = VARCH_AARCH64;
    }
    else {
        return -1;
    }
    return 0;
}

static int parse_section(const char* word, VentConds* out) {
    size_t len = strlen(word);
    if (len < 3 || word[0] != '[' || word[len - 1] != ']')
        return -1;

    const char* p = word + 1;
    const char* end = word + len - 1;

    while (p < end) {
        const char* dot = memchr(p, '.', (size_t)(end - p));
        const char* piece_end = dot ? dot : end;

        if (apply_section_piece(p, (size_t)(piece_end - p), out) != 0)
            return -1;

        if (!dot)
            break;
        p = dot + 1;
    }
    return 0;
}

int vent_conds_match(const VentConds* line, const VentConds* ctx) {
    if (ctx->lang != LANG_NONE && line->lang != LANG_NONE &&
        line->lang != ctx->lang) return 0;
    if (ctx->os   != VOS_ANY   && line->os   != VOS_ANY &&
        line->os   != ctx->os)   return 0;
    if (ctx->arch != VARCH_ANY && line->arch != VARCH_ANY &&
        line->arch != ctx->arch) return 0;
    return 1;
}

VentLang vent_lang_from_name(const char* name) {
    if (strcmp(name, "quant") == 0 || strcmp(name, "qu") == 0)
        return LANG_QUANT;
    if (strcmp(name, "cpp") == 0)
        return LANG_CPP;
    if (strcmp(name, "c") == 0)
        return LANG_C;
    return LANG_NONE;
}

int vent_target_apply(const char* name, VentConds* ctx) {
    VentConds t = { LANG_NONE, VOS_ANY, VARCH_ANY };
    if (apply_section_piece(name, strlen(name), &t) != 0)
        return -1;
    if (t.os == VOS_ANY || t.arch == VARCH_ANY)
        return -1; // not a full target
    ctx->os = t.os;
    ctx->arch = t.arch;
    return 0;
}

VentConds vent_host_context(void) {
    VentConds ctx = { LANG_NONE, vent_host_os(), vent_host_arch() };
    return ctx;
}

static char* read_file(const char* path, long* out_size) {
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

    char* buffer = malloc((size_t)size + 1);
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
    *out_size = size;
    return buffer;
}

// line iterator over a buffer; next() terminates the current line at '\n'
typedef struct {
    char* cur;
} LineIter;

static void lineiter_init(LineIter* it, char* buf) {
    it->cur = buf;
}

static int lineiter_done(const LineIter* it) {
    return it->cur[0] == '\0';
}

static void lineiter_next(LineIter* it) {
    char* eol = strchr(it->cur, '\n');
    if (eol) {
        *eol = '\0';
        it->cur = eol + 1;
    } else {
        it->cur += strlen(it->cur);
    }
}

// returns the current line (terminated at '\n') and advances the iterator
static char* next_line(LineIter* it) {
    char* line = it->cur;
    lineiter_next(it);
    return line;
}

// build a Package from the words of one line; returns 0 on success,
// -1 when the line is not a valid package declaration
static int package_from_words(const char** words, int n, const VentConds* sec,
                              Package* out) {
    memset(out, 0, sizeof(*out));
    out->conds = *sec;

    int has_name = (sec->lang == LANG_QUANT);
    int verb_idx = has_name ? 1 : 0;

    if (has_name && n < 2)
        return -1;

    SourceType type = source_from_verb(words[verb_idx]);
    if (type == (SourceType)-1)
        return -1;

    out->type = type;
    if (has_name)
        out->name = words[0];

    int start = verb_idx + 1;
    int argc = n - start;
    const char* ref = NULL;

    if (argc > 0 && words[n - 1][0] == '@') {
        ref = words[n - 1];
        argc--;
    }

    out->argc = argc;
    out->ref = ref;
    if (argc > 0) {
        out->argv = malloc(sizeof(const char*) * argc);
        for (int j = 0; j < argc; j++)
            out->argv[j] = words[start + j];
    }
    return 0;
}

ConfigFile* parse_config_file(const char* path) {
    long size = 0;
    char* buffer = read_file(path, &size);
    if (!buffer)
        return NULL;

    Package* items = malloc(sizeof(Package) * (size_t)size);
    if (!items) {
        free(buffer);
        return NULL;
    }
    size_t count = 0;

    VentConds current = { LANG_NONE, VOS_ANY, VARCH_ANY };

    LineIter it;
    lineiter_init(&it, buffer);

    while (!lineiter_done(&it)) {
        char* raw = next_line(&it);

        char* hash = strchr(raw, '#');
        if (hash)
            *hash = '\0';

        char* line = skip_ws(raw);
        if (*line == '\0')
            continue;

        const char* words[MAX_WORDS];
        int n = 0;
        split_words(line, words, &n);
        if (n == 0)
            continue;

        if (words[0][0] == '[') {
            VentConds sec = { LANG_NONE, VOS_ANY, VARCH_ANY };
            if (parse_section(words[0], &sec) == 0)
                current = sec;
            // invalid section header: keep previous section
            continue;
        }

        Package pkg;
        if (package_from_words(words, n, &current, &pkg) != 0)
            continue;

        items[count++] = pkg;
    }

    if (count > 0) {
        Package* shrink = realloc(items, sizeof(Package) * count);
        if (shrink)
            items = shrink;
    }

    ConfigFile* cf = malloc(sizeof(ConfigFile));
    if (!cf) {
        for (size_t i = 0; i < count; i++)
            free((void*)items[i].argv);
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

static Package package_deep_copy(const Package* src) {
    Package p;
    memset(&p, 0, sizeof(p));
    p.conds = src->conds;
    p.type = src->type;
    if (src->name)
        p.name = strdup(src->name);
    if (src->ref)
        p.ref = strdup(src->ref);
    if (src->argc > 0 && src->argv) {
        p.argv = malloc(sizeof(const char*) * src->argc);
        for (int j = 0; j < src->argc; j++)
            p.argv[j] = strdup(src->argv[j]);
    }
    p.argc = src->argc;
    return p;
}

static void package_deep_free(Package* p) {
    if (p->name) {
        free((void*)p->name);
        p->name = NULL;
    }
    if (p->ref) {
        free((void*)p->ref);
        p->ref = NULL;
    }
    if (p->argv) {
        for (int j = 0; j < p->argc; j++)
            free((void*)p->argv[j]);
        free(p->argv);
        p->argv = NULL;
    }
    p->argc = 0;
}

// flat config list for parallel dispatch

typedef struct {
    Package* items;
    size_t count;
    size_t cap;
} FlatConfig;

static void flat_init(FlatConfig* f) {
    f->items = NULL;
    f->count = 0;
    f->cap = 0;
}

static void flat_append(FlatConfig* f, Package cfg) {
    if (f->count >= f->cap) {
        f->cap = f->cap ? f->cap * 2 : 64;
        f->items = realloc(f->items, sizeof(Package) * f->cap);
    }
    f->items[f->count++] = cfg;
}

static void flat_free(FlatConfig* f) {
    for (size_t i = 0; i < f->count; i++)
        package_deep_free(&f->items[i]);
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
                Package* item = &sub->items[i];
                if (item->type == SOURCE_STD) {
                    for (int a = 0; a < item->argc; a++)
                        flatten_std(flat, item->argv[a]);
                } else {
                    flat_append(flat, package_deep_copy(item));
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
    Package cfg;
    double t0;
} JobData;

static int do_git(void* data) {
    JobData* jd = (JobData*)data;
    const char* url = jd->cfg.argv[0];

    if (jd->cfg.argc < 2) {
        ui_error_safe("git: missing target dir for %s\n", url);
        return -1;
    }
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

    if (jd->cfg.argc < 2) {
        ui_error_safe("archive: missing target dir for %s\n", url);
        return -1;
    }
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

static int do_path(void* data) {
    JobData* jd = (JobData*)data;
    const char* dir = jd->cfg.argc > 0 ? jd->cfg.argv[0] : "?";
    ui_print_resolve_item_safe(dir, "local", ui_now() - jd->t0, 1);
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

DoctorResult* check_existing_dirs(const ConfigFile* cf, const VentConds* ctx) {
    DoctorResult* dr = malloc(sizeof(DoctorResult));
    if (!dr) return NULL;
    dr->count = 0;
    dr->paths = NULL;

    // Flatten first to resolve std references
    FlatConfig flat;
    flat_init(&flat);

    for (size_t i = 0; i < cf->count; i++) {
        const Package* item = &cf->items[i];
        if (!vent_conds_match(&item->conds, ctx))
            continue;
        if (item->type == SOURCE_STD) {
            for (int a = 0; a < item->argc; a++)
                flatten_std(&flat, item->argv[a]);
        } else {
            flat_append(&flat, package_deep_copy(item));
        }
    }

    size_t cap = 0;
    for (size_t i = 0; i < flat.count; i++) {
        const Package* item = &flat.items[i];
        const char* path = NULL;

        if (!vent_conds_match(&item->conds, ctx))
            continue;

        if (item->type == SOURCE_GIT || item->type == SOURCE_ARCHIVE) {
            if (item->argc >= 2)
                path = item->argv[1];
        }

        if (path) {
            struct stat st;
            if (vent_lstat(path, &st) == 0) {
                if (dr->count >= cap) {
                    cap = cap ? cap * 2 : 16;
                    dr->paths = realloc(dr->paths, sizeof(char*) * cap);
                }
                dr->paths[dr->count] = strdup(path);
                dr->count++;
            }
        }
    }

    flat_free(&flat);
    return dr;
}

void free_doctor_result(DoctorResult* dr) {
    if (!dr) return;
    for (size_t i = 0; i < dr->count; i++)
        free(dr->paths[i]);
    free(dr->paths);
    free(dr);
}

int resolve_config(ConfigFile *cf, int jobs, const VentConds* ctx) {
    FlatConfig flat;
    flat_init(&flat);

    for (size_t i = 0; i < cf->count; i++) {
        Package* item = &cf->items[i];
        if (!vent_conds_match(&item->conds, ctx))
            continue;
        if (item->type == SOURCE_STD) {
            for (int a = 0; a < item->argc; a++)
                flatten_std(&flat, item->argv[a]);
        } else {
            flat_append(&flat, package_deep_copy(item));
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
        if (!vent_conds_match(&flat.items[i].conds, ctx))
            continue;

        JobData* jd = malloc(sizeof(JobData));
        jd->cfg = flat.items[i];
        jd->t0 = ui_now();

        JobFunc func = NULL;
        switch (jd->cfg.type) {
            case SOURCE_GIT:     func = do_git;     break;
            case SOURCE_ARCHIVE: func = do_archive; break;
            case SOURCE_SYSTEM:  func = do_system;  break;
            case SOURCE_PATH:    func = do_path;    break;
            default: free(jd); continue;
        }
        tp_enqueue(pool, func, jd);
    }

    int result = tp_wait(pool);
    tp_destroy(pool);

    flat_free(&flat);

    return result == 0 ? 1 : 0;
}
