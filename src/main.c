#include <stdio.h>
#include "config.h"
#include "ui.h"

int main(int argc, char** argv){
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if(argc < 2){
        ui_error("Usage: vent file.vent\n");
        return 1;
    }

    ConfigFile* cf = parse_config_file(argv[1]);
    if(!cf){
        ui_error("Failed to parse config file '%s' (file not found or invalid)\n", argv[1]);
        return 1;
    }

    ui_print_header(argv[1]);

    DepNode* tree = ui_build_tree(cf);
    ui_print_tree(tree);

    ui_print_resolve_start();

    double t0 = ui_now();
    int result = resolve_config(cf);
    double elapsed = ui_now() - t0;

    ui_print_summary(elapsed);
    ui_free_tree(tree);
    free_config(cf);

    return result == 1 ? 0 : 1;
}
