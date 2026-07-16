#ifndef OPTIONS_H
#define OPTIONS_H

typedef struct {
    const char* file;
    int jobs_count;
    int ask;
    int clean;
} Option;

const Option* parse_option(int argc, char** argv);

#endif
