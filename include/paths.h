#ifndef PATHS_H
#define PATHS_H

const char* vent_std_path(void);
const char* vent_cache_dir(void);
char* vent_cache_path_for_url(const char* url);
int vent_cache_clean(void);

#endif // PATHS_H
