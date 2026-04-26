#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#define CFG_MAX_STR_LEN     256
#define CFG_MAX_PATH_LEN    512
#define CFG_FILE_PATH       "/tmp/endoscope_config.ini"

typedef struct {
    char language[CFG_MAX_STR_LEN];
    char password[CFG_MAX_STR_LEN];
    char record_path[CFG_MAX_PATH_LEN];
} config_t;

bool config_save(const config_t * cfg);
bool config_load(config_t * cfg);
void config_set_defaults(config_t * cfg);

#ifdef __cplusplus
}
#endif

#endif
