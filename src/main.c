#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "ui.h"
#include "paths.h"
#include "platform.h"

int main(int argc, char** argv){
    vent_init_console();
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    const char* config_path = NULL;
    int jobs = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
            jobs = atoi(argv[++i]);
            if (jobs < 1) jobs = 1;
        } else if (strncmp(argv[i], "--jobs=", 7) == 0) {
            jobs = atoi(argv[i] + 7);
            if (jobs < 1) jobs = 1;
        } else {
            config_path = argv[i];
        }
    }

    if (!config_path){
        ui_error("Usage: vent [-j N] file.vent\n");
        return 1;
    }

    ConfigFile* cf = parse_config_file(config_path);
    if(!cf){
        ui_error("Failed to parse config file '%s' (file not found or invalid)\n", config_path);
        return 1;
    }

    ui_print_header(config_path);

    DepNode* tree = ui_build_tree(cf);
    ui_print_tree(tree);

    ui_print_resolve_start();

    // initialise cache dir before thread pool (single-threaded)
    vent_cache_dir();

    double t0 = ui_now();
    int result = resolve_config(cf, jobs);
    double elapsed = ui_now() - t0;

    ui_print_summary(elapsed);
    ui_free_tree(tree);
    free_config(cf);

    return result == 1 ? 0 : 1;
}
