#ifndef OPTIONS_H
#define OPTIONS_H

typedef struct {
    const char* file;
    int jobs_count;
    int ask;
    int clean;
    int no_remote_std;
    int update_std;
    int doctor;
    int forced;
} Option;

const Option* parse_option(int argc, char** argv);

#endif
