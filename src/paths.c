#include <stdlib.h>
#include "paths.h"

const char* vent_std_path(void) {
    const char* env = getenv("VENT_STD");
    if (env && *env)
        return env;

    return VENT_STD_INSTALL;
}
