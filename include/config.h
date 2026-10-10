#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

typedef enum {
    SOURCE_STD,
    SOURCE_GIT,
    SOURCE_ARCHIVE,
    SOURCE_SYSTEM,
    SOURCE_PATH,
} SourceType;

typedef enum {
    LANG_NONE,
    LANG_C,
    LANG_CPP,
    LANG_QUANT,
} VentLang; // available languages

typedef enum {
    VOS_ANY = 0,
    VOS_LINUX,
    VOS_WINDOWS,
    VOS_MACOS,
    VOS_ZEROPOINT,
} VentOS;

typedef enum {
    VARCH_ANY = 0,
    VARCH_X86_64,
    VARCH_AARCH64,
} VentArch;

typedef struct {
    VentLang lang;
    VentOS   os;
    VentArch arch;
} VentConds;

typedef struct {
    VentConds    conds;
    SourceType   type;
    const char*  name;
    const char** argv;
    int          argc;
    const char*  ref;
} Package;

typedef struct {
    Package* items;
    size_t   count;
    char*    buffer;
} ConfigFile;

ConfigFile* parse_config_file(const char* path);
void free_config(ConfigFile* cf);

int resolve_config(ConfigFile *cf, int jobs, const VentConds* ctx);

int vent_conds_match(const VentConds* line, const VentConds* ctx);

VentLang vent_lang_from_name(const char* name);
int vent_target_apply(const char* name, VentConds* ctx);
VentConds vent_host_context(void);

typedef struct {
    size_t count;
    char** paths;
} DoctorResult;

DoctorResult* check_existing_dirs(const ConfigFile* cf, const VentConds* ctx);
void free_doctor_result(DoctorResult* dr);

#endif
