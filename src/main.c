#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "ui.h"
#include "paths.h"
#include "platform.h"
#include "option.h"

int main(int argc, char** argv){
    vent_init_console();
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    const Option* opt = parse_option(argc, argv);

    if (opt->update_std) {
        vent_std_repo_ensure();
        if (vent_std_repo_path())
            ui_info("std repository updated: %s\n", vent_std_repo_path());
        return 0;
    }

    if (!opt->file){
        ui_error("Usage: vent [-j N] [--ask] [--clean] [--doctor] [--forced] [--no-remote-std] [--update-std] file.vent\n"
                 "       vent -d file.vent   (short flags can be combined: -adf, -j4)\n");
        return 1;
    }

    if (opt->clean) {
        vent_cache_clean();
        ui_info("Cache cleaned\n");
    }

    if (!opt->no_remote_std)
        vent_std_repo_ensure();

    ConfigFile* cf = parse_config_file(opt->file);
    if(!cf){
        ui_error("Failed to parse config file '%s' (file not found or invalid)\n", opt->file);
        return 1;
    }

    if (opt->doctor) {
        DoctorResult* dr = check_existing_dirs(cf);
        if (dr->count > 0) {
            ui_info("Found %zu existing target director%s:\n", dr->count, dr->count == 1 ? "y" : "ies");
            for (size_t i = 0; i < dr->count; i++)
                ui_info("  %s\n", dr->paths[i]);
            free_doctor_result(dr);
            free_config(cf);
            return 0;
        }
        free_doctor_result(dr);
        if (!ui_ask_continue()) {
            ui_info("Aborted by user\n");
            free_config(cf);
            return 0;
        }
    }

    if (opt->forced) {
        // check existing dirs and remove them
        DoctorResult* dr = check_existing_dirs(cf);
        for (size_t i = 0; i < dr->count; i++) {
            ui_warning("Removing existing directory: %s\n", dr->paths[i]);
            vent_rm_rf(dr->paths[i]);
        }
        free_doctor_result(dr);
    }

    ui_print_header(opt->file);

    DepNode* tree = ui_build_tree(cf);
    ui_print_tree(tree);

    if (opt->ask && !ui_ask_continue()) {
        ui_info("Aborted by user\n");
        ui_free_tree(tree);
        free_config(cf);
        return 0;
    }

    ui_print_resolve_start();

    // initialise cache dir before thread pool (single-threaded)
    vent_cache_dir();

    double t0 = ui_now();
    int result = resolve_config(cf, opt->jobs_count);
    double elapsed = ui_now() - t0;

    ui_print_summary(elapsed);
    ui_free_tree(tree);
    free_config(cf);

    return result == 1 ? 0 : 1;

cleanup:
    free_config(cf);
    return 0;
}
