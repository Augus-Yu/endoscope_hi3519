#include "config_manager.h"
#include <stdio.h>
#include <string.h>

void config_set_defaults(config_t * cfg)
{
    if(!cfg) return;
    memset(cfg, 0, sizeof(config_t));
    strncpy(cfg->language, "zh_CN", sizeof(cfg->language) - 1);
    strncpy(cfg->password, "", sizeof(cfg->password) - 1);
    strncpy(cfg->record_path, "/mnt/sd/record", sizeof(cfg->record_path) - 1);
}

bool config_save(const config_t * cfg)
{
    if(!cfg) return false;

    FILE * fp = fopen(CFG_FILE_PATH, "w");
    if(!fp) {
        fprintf(stderr, "[ConfigManager] Failed to open %s for writing\n", CFG_FILE_PATH);
        return false;
    }

    fprintf(fp, "[endoscope]\n");
    fprintf(fp, "language=%s\n", cfg->language);
    fprintf(fp, "password=%s\n", cfg->password);
    fprintf(fp, "record_path=%s\n", cfg->record_path);

    if(fclose(fp) != 0) {
        fprintf(stderr, "[ConfigManager] Failed to close file %s\n", CFG_FILE_PATH);
        return false;
    }

    return true;
}

bool config_load(config_t * cfg)
{
    if(!cfg) return false;

    config_set_defaults(cfg);

    FILE * fp = fopen(CFG_FILE_PATH, "r");
    if(!fp) {
        fprintf(stderr, "[ConfigManager] Config file not found, using defaults\n");
        return false;
    }

    char line[1024];
    while(fgets(line, sizeof(line), fp)) {
        char * p = line;
        while(*p == ' ' || *p == '\t') p++;
        if(*p == '\0' || *p == '\n' || *p == '\r' || *p == '#' || *p == ';') continue;
        if(*p == '[') continue;

        char * eq = strchr(p, '=');
        if(!eq) continue;
        *eq = '\0';

        char * key = p;
        char * value = eq + 1;

        size_t key_len = strlen(key);
        while(key_len > 0 && (key[key_len - 1] == ' ' || key[key_len - 1] == '\t')) {
            key[key_len - 1] = '\0';
            key_len--;
        }

        while(*value == ' ' || *value == '\t') value++;

        size_t val_len = strlen(value);
        while(val_len > 0 && (value[val_len - 1] == '\n' || value[val_len - 1] == '\r')) {
            value[val_len - 1] = '\0';
            val_len--;
        }

        if(strcmp(key, "language") == 0) {
            strncpy(cfg->language, value, sizeof(cfg->language) - 1);
        } else if(strcmp(key, "password") == 0) {
            strncpy(cfg->password, value, sizeof(cfg->password) - 1);
        } else if(strcmp(key, "record_path") == 0) {
            strncpy(cfg->record_path, value, sizeof(cfg->record_path) - 1);
        }
    }

    fclose(fp);
    return true;
}
