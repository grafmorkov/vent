#include <stdlib.h>
#include <string.h>
#include "option.h"

static Option parsed_option;

const Option* parse_option(int argc, char** argv) {
    memset(&parsed_option, 0, sizeof(Option));
    parsed_option.jobs_count = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
            parsed_option.jobs_count = atoi(argv[++i]);
            if (parsed_option.jobs_count < 1)
                parsed_option.jobs_count = 1;
        } else if (strncmp(argv[i], "--jobs=", 7) == 0) {
            parsed_option.jobs_count = atoi(argv[i] + 7);
            if (parsed_option.jobs_count < 1)
                parsed_option.jobs_count = 1;
        } else if (strcmp(argv[i], "--ask") == 0 || strcmp(argv[i], "-a") == 0) {
            parsed_option.ask = 1;
        } else if (strcmp(argv[i], "--clean") == 0) {
            parsed_option.clean = 1;
        } else if (strcmp(argv[i], "--no-remote-std") == 0) {
            parsed_option.no_remote_std = 1;
        } else if (strcmp(argv[i], "--update-std") == 0) {
            parsed_option.update_std = 1;
        } else {
            parsed_option.file = argv[i];
        }
    }

    return &parsed_option;
}
