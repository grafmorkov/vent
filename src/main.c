#include <stdio.h>
#include "config.h"

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "ERROR: Usage: vent file.vent\n");
        return 1;
    }

    ConfigFile* cf = parse_config_file(argv[1]);
    if(!cf){
        fprintf(stderr, "ERROR: Failed to parse config file '%s' (file not found or invalid)\n", argv[1]);
        return 1;
    }

    if(!resolve_config(cf)){
        fprintf(stderr, "ERROR: resolve_config failed\n");
        free_config(cf);
        return 1;
    }

    free_config(cf);
    return 0;
}
