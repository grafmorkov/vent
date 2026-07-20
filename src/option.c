#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "option.h"

static Option parsed_option;

static void set_short_flag(char c) {
    switch (c) {
        case 'a': parsed_option.ask = 1; break;
        case 'd': parsed_option.doctor = 1; break;
        case 'f': parsed_option.forced = 1; break;
        case 'c': parsed_option.clean = 1; break;
        default:
            fprintf(stderr, "Unknown flag -%c\n", c);
            break;
    }
}

const Option* parse_option(int argc, char** argv) {
    memset(&parsed_option, 0, sizeof(Option));
    parsed_option.jobs_count = 1;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                // long option
                if (strcmp(argv[i], "--ask") == 0) {
                    parsed_option.ask = 1;
                } else if (strcmp(argv[i], "--doctor") == 0) {
                    parsed_option.doctor = 1;
                } else if (strcmp(argv[i], "--forced") == 0) {
                    parsed_option.forced = 1;
                } else if (strcmp(argv[i], "--clean") == 0) {
                    parsed_option.clean = 1;
                } else if (strcmp(argv[i], "--no-remote-std") == 0) {
                    parsed_option.no_remote_std = 1;
                } else if (strcmp(argv[i], "--update-std") == 0) {
                    parsed_option.update_std = 1;
                } else if (strncmp(argv[i], "--jobs=", 7) == 0) {
                    parsed_option.jobs_count = atoi(argv[i] + 7);
                    if (parsed_option.jobs_count < 1)
                        parsed_option.jobs_count = 1;
                } else {
                    fprintf(stderr, "Unknown option: %s\n", argv[i]);
                }
            } else {
                // short option(s) - parse each character
                for (int j = 1; argv[i][j] != '\0'; j++) {
                    if (argv[i][j] == 'j') {
                        // -j takes a value: -j N or -jN
                        if (argv[i][j+1] != '\0') {
                            parsed_option.jobs_count = atoi(argv[i] + j + 1);
                            if (parsed_option.jobs_count < 1)
                                parsed_option.jobs_count = 1;
                        } else if (i + 1 < argc) {
                            parsed_option.jobs_count = atoi(argv[++i]);
                            if (parsed_option.jobs_count < 1)
                                parsed_option.jobs_count = 1;
                        }
                        break;
                    }
                    set_short_flag(argv[i][j]);
                }
            }
        } else {
            parsed_option.file = argv[i];
        }
    }

    return &parsed_option;
}
