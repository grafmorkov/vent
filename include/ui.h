#ifndef UI_H
#define UI_H

#include <stddef.h>
#include "config.h"

#define UI_RED     "\033[1;31m"
#define UI_GREEN   "\033[1;32m"
#define UI_YELLOW  "\033[1;33m"
#define UI_BLUE    "\033[1;34m"
#define UI_CYAN    "\033[1;36m"
#define UI_BOLD    "\033[1m"
#define UI_DIM     "\033[2m"
#define UI_RESET   "\033[0m"
#define UI_CHECK   "\xE2\x9C\x93"
#define UI_CROSS   "\xE2\x9C\x97"

typedef struct DepNode {
    SourceType type;
    char label[256];
    char detail[512];
    struct DepNode** children;
    size_t child_count;
    size_t child_cap;
} DepNode;

DepNode* ui_build_tree(ConfigFile* cf);
void     ui_print_tree(DepNode* root);
void     ui_free_tree(DepNode* node);

void ui_print_header(const char* filename);
void ui_print_resolve_start(void);
void ui_print_resolve_item(const char* name, const char* detail, double elapsed, int success);
void ui_print_summary(double elapsed);

void ui_error(const char* fmt, ...);
void ui_warning(const char* fmt, ...);
void ui_success(const char* fmt, ...);
void ui_info(const char* fmt, ...);

double ui_now(void);
int    ui_ask_continue(void);

void ui_print_resolve_item_safe(const char* name, const char* detail, double elapsed, int success);
void ui_error_safe(const char* fmt, ...);
void ui_warning_safe(const char* fmt, ...);
void ui_info_safe(const char* fmt, ...);

#endif
