#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "download.h"
#include "ui.h"
#include "paths.h"
#include "platform.h"

static int ui_resolve_ok = 0;
static int ui_resolve_fail = 0;

// Helpers

static void strip_ext(const char* in, char* out, size_t n) {
    strncpy(out, in, n);
    out[n - 1] = '\0';
    char* dot = strrchr(out, '.');
    if (dot) *dot = '\0';
}

static void depnode_add(DepNode* parent, DepNode* child) {
    if (parent->child_count >= parent->child_cap) {
        parent->child_cap = parent->child_cap ? parent->child_cap * 2 : 8;
        DepNode** p = realloc(parent->children,
                              parent->child_cap * sizeof(DepNode*));
        if (!p) return;
        parent->children = p;
    }
    parent->children[parent->child_count++] = child;
}

// tree builder

DepNode* ui_build_tree(ConfigFile* cf) {
    DepNode* root = calloc(1, sizeof(DepNode));

    for (size_t i = 0; i < cf->count; i++) {
        Config* cfg = &cf->items[i];

        if (cfg->type == SOURCE_STD) {
            if (cfg->argc <= 0) continue;

            DIR* dir = opendir(vent_std_path());
            if (!dir) {
                for (int a = 0; a < cfg->argc; a++) {
                    DepNode* pkg = calloc(1, sizeof(DepNode));
                    pkg->type = SOURCE_STD;
                    snprintf(pkg->label, sizeof(pkg->label),
                             "std %s", cfg->argv[a]);
                    snprintf(pkg->detail, sizeof(pkg->detail),
                             "std/ not found");
                    depnode_add(root, pkg);
                }
                continue;
            }

            for (int a = 0; a < cfg->argc; a++) {
                DepNode* pkg = calloc(1, sizeof(DepNode));
                pkg->type = SOURCE_STD;
                snprintf(pkg->label, sizeof(pkg->label),
                         "std %s", cfg->argv[a]);

                rewinddir(dir);
                struct dirent* entry;
                int found = 0;
                while ((entry = readdir(dir)) != NULL && !found) {
                    if (strcmp(entry->d_name, ".")  == 0 ||
                        strcmp(entry->d_name, "..") == 0)
                        continue;
                    char name[256];
                    strip_ext(entry->d_name, name, sizeof(name));
                    if (strcmp(name, cfg->argv[a]) == 0) {
                        found = 1;
                        snprintf(pkg->detail, sizeof(pkg->detail),
                                 "%s", entry->d_name);
                        char path[1024];
                        snprintf(path, sizeof(path), "%s/%s",
                                 vent_std_path(), entry->d_name);
                        ConfigFile* sub = parse_config_file(path);
                        if (sub) {
                            DepNode* sub_root = ui_build_tree(sub);
                            for (size_t c = 0; c < sub_root->child_count; c++)
                                depnode_add(pkg, sub_root->children[c]);
                            free(sub_root->children);
                            free(sub_root);
                            free_config(sub);
                        }
                    }
                }
                if (!found)
                    snprintf(pkg->detail, sizeof(pkg->detail), "not found");

                depnode_add(root, pkg);
            }
            closedir(dir);

        } else {
            DepNode* node = calloc(1, sizeof(DepNode));
            node->type = cfg->type;

            switch (cfg->type) {
                case SOURCE_GIT: {
                    if (cfg->argc >= 1)
                        snprintf(node->label, sizeof(node->label),
                                 "git %s", cfg->argv[0]);
                    if (cfg->argc >= 2)
                        snprintf(node->detail, sizeof(node->detail),
                                 "clone \342\206\222 %s", cfg->argv[1]);
                    break;
                }
                case SOURCE_ARCHIVE: {
                    if (cfg->argc >= 1) {
                        const char* fname = strrchr(cfg->argv[0], '/');
                        fname = fname ? fname + 1 : cfg->argv[0];
                        snprintf(node->label, sizeof(node->label),
                                 "archive %s", fname);
                    }
                    if (cfg->argc >= 2)
                        snprintf(node->detail, sizeof(node->detail),
                                 "\342\206\222 %s", cfg->argv[1]);
                    break;
                }
                case SOURCE_SYSTEM: {
                    if (cfg->argc > 0) {
                        size_t pos = 0;
                        for (int a = 0; a < cfg->argc; a++) {
                            if (a > 0 && pos < sizeof(node->label) - 2)
                                pos += snprintf(node->label + pos,
                                    sizeof(node->label) - pos, " ");
                            if (pos < sizeof(node->label))
                                pos += snprintf(node->label + pos,
                                    sizeof(node->label) - pos, "%s",
                                    cfg->argv[a]);
                        }
                    }
                    VentPM pm = vent_detect_package_manager();
                    const char* pm_name = "?";
                    switch (pm) {
                        case VENT_PM_APT:    pm_name = "apt";    break;
                        case VENT_PM_DNF:    pm_name = "dnf";    break;
                        case VENT_PM_PACMAN: pm_name = "pacman"; break;
                        case VENT_PM_ZYPPER: pm_name = "zypper"; break;
                        case VENT_PM_APK:    pm_name = "apk";    break;
                        case VENT_PM_XBPS:   pm_name = "xbps";   break;
                        case VENT_PM_EMERGE: pm_name = "emerge"; break;
                        case VENT_PM_BREW:   pm_name = "brew";   break;
                        case VENT_PM_WINGET: pm_name = "winget"; break;
                        case VENT_PM_CHOCO:  pm_name = "choco";  break;
                        default:             pm_name = "?";      break;
                    }
                    snprintf(node->detail, sizeof(node->detail),
                             "via %s", pm_name);
                    break;
                }
                default: break;
            }

            depnode_add(root, node);
        }
    }

    return root;
}

// tree printer

static void ui_print_tree_inner(DepNode* node, char* prefix, int is_last) {
    printf("%s%s\342\224\200\342\224\200 %s%s",
           prefix,
           is_last ? "\342\224\224" : "\342\224\234",
           UI_CYAN, node->label);
    if (node->detail[0])
        printf(" %s(%s)%s", UI_DIM, node->detail, UI_RESET);
    printf("%s\n", UI_RESET);

    size_t pos = strlen(prefix);
    strcat(prefix, is_last ? "    " : "\342\224\202   ");

    for (size_t i = 0; i < node->child_count; i++) {
        int last = (i == node->child_count - 1);
        ui_print_tree_inner(node->children[i], prefix, last);
    }

    prefix[pos] = '\0';
}

void ui_print_tree(DepNode* root) {
    if (root->child_count == 0)
        return;

    printf("\n  %sDependency Graph:%s\n", UI_BOLD, UI_RESET);

    char prefix[1024] = "  ";
    for (size_t i = 0; i < root->child_count; i++) {
        int last = (i == root->child_count - 1);
        ui_print_tree_inner(root->children[i], prefix, last);
    }
    printf("\n");
}

// tree cleanup

void ui_free_tree(DepNode* node) {
    if (!node) return;
    for (size_t i = 0; i < node->child_count; i++)
        ui_free_tree(node->children[i]);
    free(node->children);
    free(node);
}

// display

void ui_print_header(const char* filename) {
    printf("\n  %sVent Dependency Resolver%s\n", UI_BOLD, UI_RESET);
    printf("  %sfile: %s%s\n\n", UI_DIM, filename, UI_RESET);
}

void ui_print_resolve_start(void) {
    ui_resolve_ok = 0;
    ui_resolve_fail = 0;
    printf("  %sResolving dependencies...%s\n\n", UI_BOLD, UI_RESET);
}

void ui_print_resolve_item(const char* name, const char* detail,
                           double elapsed, int success)
{
    if (success) {
        printf("  %s %s %s", UI_GREEN, UI_CHECK, UI_RESET);
        ui_resolve_ok++;
    } else {
        printf("  %s %s %s", UI_RED, UI_CROSS, UI_RESET);
        ui_resolve_fail++;
    }

    printf("%s", name);
    if (detail && detail[0])
        printf(" %s\342\200\224 %s%s", UI_DIM, detail, UI_RESET);
    printf(" %s(%.1fs)%s\n", UI_DIM, elapsed, UI_RESET);
}

void ui_print_summary(double elapsed) {
    int total = ui_resolve_ok + ui_resolve_fail;

    if (total == 0 && ui_resolve_fail == 0) return;

    printf("\n  %s\342\224\200\342\224\200\342\224\200\342\224\200"
           "\342\224\200\342\224\200\342\224\200\342\224\200\342\224\200"
           "\342\224\200\342\224\200\342\224\200\342\224\200\342\224\200"
           "\342\224\200\342\224\200\342\224\200\342\224\200\342\224\200"
           "\342\224\200\342\224\200\342\224\200\342\224\200\342\224\200"
           "\342\224\200\342\224\200\342\224\200%s\n", UI_DIM, UI_RESET);

    if (ui_resolve_fail == 0) {
        printf("  %s %s %d package%s resolved successfully %s(%.1fs)%s\n",
               UI_GREEN, UI_CHECK, total,
               total == 1 ? "" : "s",
               UI_DIM, elapsed, UI_RESET);
    } else {
        printf("  %s %s ", UI_RED, UI_CROSS);
        printf("%s%d resolved, %d failed %s(%.1fs)%s\n",
               UI_RESET, ui_resolve_ok, ui_resolve_fail,
               UI_DIM, elapsed, UI_RESET);
    }
    printf("\n");
}

// logging

void ui_error(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "  %sERROR:%s ", UI_RED, UI_RESET);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void ui_warning(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("  %sWARNING:%s ", UI_YELLOW, UI_RESET);
    vprintf(fmt, ap);
    va_end(ap);
}

void ui_success(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("  %sOK:%s ", UI_GREEN, UI_RESET);
    vprintf(fmt, ap);
    va_end(ap);
}

void ui_info(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("  %sINFO:%s ", UI_BLUE, UI_RESET);
    vprintf(fmt, ap);
    va_end(ap);
}

// timer

double ui_now(void) {
    return vent_now();
}

// thread-safe wrappers

#include "threadpool.h"

void ui_print_resolve_item_safe(const char* name, const char* detail,
                                double elapsed, int success)
{
    ui_lock();
    ui_print_resolve_item(name, detail, elapsed, success);
    ui_unlock();
}

void ui_error_safe(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ui_lock();
    fprintf(stderr, "  %sERROR:%s ", UI_RED, UI_RESET);
    vfprintf(stderr, fmt, ap);
    ui_unlock();
    va_end(ap);
}

void ui_warning_safe(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ui_lock();
    printf("  %sWARNING:%s ", UI_YELLOW, UI_RESET);
    vprintf(fmt, ap);
    ui_unlock();
    va_end(ap);
}

void ui_info_safe(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ui_lock();
    printf("  %sINFO:%s ", UI_BLUE, UI_RESET);
    vprintf(fmt, ap);
    ui_unlock();
    va_end(ap);
}
