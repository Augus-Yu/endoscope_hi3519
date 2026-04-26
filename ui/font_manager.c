/**
 * @file font_manager.c
 * @brief TTF Font Manager Implementation - 从文件系统加载字体
 */

#include "font_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 字体配置 */
typedef struct {
    font_type_t type;
    const char * filename;
    const char * name;
    bool is_rtl;
} font_config_t;

static const font_config_t font_configs[] = {
    { FONT_TYPE_LATIN,   "NotoSans-Regular.ttf",       "Latin",     false },
    { FONT_TYPE_CJK,     "NotoSansCJKsc-Regular.otf",  "CJK",       false },
    { FONT_TYPE_THAI,    "NotoSansThai-Regular.ttf",   "Thai",      false },
    { FONT_TYPE_ARABIC,  "NotoSansArabic-Regular.ttf", "Arabic",    true  },
    { FONT_TYPE_MYANMAR, "NotoSansMyanmar-Regular.ttf","Myanmar",   false },
};

#define FONT_CONFIG_COUNT (sizeof(font_configs) / sizeof(font_configs[0]))

/* 字体数据缓存 */
typedef struct {
    uint8_t * data;
    size_t size;
} font_data_cache_t;

static struct {
    char font_dir[256];
    lv_font_t * fonts[FONT_TYPE_COUNT];
    lv_font_t * fonts_large[FONT_TYPE_COUNT];
    font_data_cache_t font_data[FONT_TYPE_COUNT];
    font_type_t current_type;
    bool is_rtl;
    bool initialized;
} g_fm = {0};

static const font_config_t * get_font_config(font_type_t type)
{
    for(size_t i = 0; i < FONT_CONFIG_COUNT; i++) {
        if(font_configs[i].type == type) {
            return &font_configs[i];
        }
    }
    return NULL;
}

static bool load_font_data_to_memory(font_type_t type)
{
    if(g_fm.font_data[type].data != NULL) {
        return true; /* 已加载 */
    }

    const font_config_t * config = get_font_config(type);
    if(!config) return false;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_fm.font_dir, config->filename);

    FILE * fp = fopen(path, "rb");
    if(!fp) {
        printf("[FontManager] Font file not found: %s\n", path);
        return false;
    }

    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if(file_size <= 0) {
        printf("[FontManager] Invalid font file size: %s\n", path);
        fclose(fp);
        return false;
    }

    /* 分配内存 */
    uint8_t * data = malloc(file_size);
    if(!data) {
        printf("[FontManager] Failed to allocate memory for font: %ld bytes\n", file_size);
        fclose(fp);
        return false;
    }

    /* 读取文件 */
    if(fread(data, 1, file_size, fp) != (size_t)file_size) {
        printf("[FontManager] Failed to read font file: %s\n", path);
        free(data);
        fclose(fp);
        return false;
    }

    fclose(fp);

    g_fm.font_data[type].data = data;
    g_fm.font_data[type].size = file_size;
    printf("[FontManager] Loaded font: %s (%ld bytes)\n", config->name, file_size);
    return true;
}

static bool load_font_by_type(font_type_t type, int size, lv_font_t ** font_out)
{
    const font_config_t * config = get_font_config(type);
    if(!config) return false;

    /* 加载字体数据到内存 */
    if(!load_font_data_to_memory(type)) {
        printf("[FontManager] Failed to load font data for type %d\n", type);
        return false;
    }

    /* 使用 lv_tiny_ttf 创建字体 */
    printf("[FontManager] Creating font with cache size: %d\n", LV_TINY_TTF_CACHE_GLYPH_CNT);
    lv_font_t * font = lv_tiny_ttf_create_data(g_fm.font_data[type].data,
                                                g_fm.font_data[type].size,
                                                size);
    if(!font) {
        printf("[FontManager] Failed to create font from data: %s\n", config->filename);
        return false;
    }

    *font_out = font;
    printf("[FontManager] Created font: %s (size=%d, cache=%d)\n", config->name, size, LV_TINY_TTF_CACHE_GLYPH_CNT);
    return true;
}

static void unload_font(lv_font_t ** font)
{
    if(*font) {
        lv_tiny_ttf_destroy(*font);
        *font = NULL;
    }
}

bool font_manager_init(const char * font_dir)
{
    if(g_fm.initialized) return true;

    memset(&g_fm, 0, sizeof(g_fm));

    if(font_dir) {
        strncpy(g_fm.font_dir, font_dir, sizeof(g_fm.font_dir) - 1);
    } else {
        strncpy(g_fm.font_dir, FONT_PATH_PREFIX, sizeof(g_fm.font_dir) - 1);
    }

    g_fm.current_type = FONT_TYPE_LATIN;
    g_fm.is_rtl = false;
    g_fm.initialized = true;

    printf("[FontManager] Initialized, font dir: %s\n", g_fm.font_dir);
    return true;
}

bool font_manager_load_for_language(const char * lang_code)
{
    if(!g_fm.initialized || !lang_code) return false;

    font_type_t new_type = font_manager_detect_type(lang_code);
    bool new_rtl = font_manager_lang_is_rtl(lang_code);

    printf("[FontManager] Loading language: %s (type=%d, rtl=%d)\n",
           lang_code, new_type, new_rtl);

    /* 如果字体未加载，尝试加载 */
    if(!g_fm.fonts[new_type]) {
        if(!load_font_by_type(new_type, FONT_DEFAULT_SIZE, &g_fm.fonts[new_type])) {
            printf("[FontManager] Warning: Failed to load font for type %d, using fallback\n", new_type);
            /* 尝试使用 Latin 字体作为回退 */
            if(new_type != FONT_TYPE_LATIN && g_fm.fonts[FONT_TYPE_LATIN]) {
                g_fm.fonts[new_type] = g_fm.fonts[FONT_TYPE_LATIN];
            }
        }
    }

    /* 加载大字体 */
    if(!g_fm.fonts_large[new_type] && g_fm.fonts[new_type]) {
        load_font_by_type(new_type, FONT_LARGE_SIZE, &g_fm.fonts_large[new_type]);
    }

    g_fm.current_type = new_type;
    g_fm.is_rtl = new_rtl;

    return true;
}

lv_font_t * font_manager_get_font(void)
{
    if(!g_fm.initialized) return NULL;

    lv_font_t * font = g_fm.fonts[g_fm.current_type];
    if(!font) {
        font = g_fm.fonts[FONT_TYPE_LATIN];
    }
    if(!font) {
        /* 最后回退到内置字体 */
        return (lv_font_t *)&lv_font_montserrat_16;
    }
    return font;
}

lv_font_t * font_manager_get_font_large(void)
{
    if(!g_fm.initialized) return NULL;

    lv_font_t * font = g_fm.fonts_large[g_fm.current_type];
    if(!font) {
        font = g_fm.fonts_large[FONT_TYPE_LATIN];
    }
    if(!font) {
        return font_manager_get_font();
    }
    return font;
}

lv_font_t * font_manager_get_font_by_type(font_type_t type)
{
    if(!g_fm.initialized || type >= FONT_TYPE_COUNT) return NULL;
    return g_fm.fonts[type] ? g_fm.fonts[type] : g_fm.fonts[FONT_TYPE_LATIN];
}

bool font_manager_is_rtl(void)
{
    return g_fm.is_rtl;
}

font_type_t font_manager_get_current_type(void)
{
    return g_fm.current_type;
}

void font_manager_reload(const char * lang_code)
{
    font_manager_load_for_language(lang_code);
}

void font_manager_deinit(void)
{
    if(!g_fm.initialized) return;

    /* 释放所有字体 */
    for(int i = 0; i < FONT_TYPE_COUNT; i++) {
        unload_font(&g_fm.fonts[i]);
        unload_font(&g_fm.fonts_large[i]);

        /* 释放字体数据内存 */
        if(g_fm.font_data[i].data) {
            free(g_fm.font_data[i].data);
            g_fm.font_data[i].data = NULL;
            g_fm.font_data[i].size = 0;
        }
    }

    g_fm.initialized = false;
    printf("[FontManager] Deinitialized\n");
}

font_type_t font_manager_detect_type(const char * lang_code)
{
    if(!lang_code) return FONT_TYPE_LATIN;

    /* CJK 语言 */
    if(strncmp(lang_code, "zh", 2) == 0 ||
       strncmp(lang_code, "ja", 2) == 0 ||
       strncmp(lang_code, "ko", 2) == 0) {
        return FONT_TYPE_CJK;
    }

    /* Thai */
    if(strncmp(lang_code, "th", 2) == 0) {
        return FONT_TYPE_THAI;
    }

    /* Arabic / Hebrew (RTL) */
    if(strncmp(lang_code, "ar", 2) == 0 ||
       strncmp(lang_code, "fa", 2) == 0 ||
       strncmp(lang_code, "ur", 2) == 0 ||
       strncmp(lang_code, "he", 2) == 0) {
        return FONT_TYPE_ARABIC;
    }

    /* Myanmar */
    if(strncmp(lang_code, "my", 2) == 0) {
        return FONT_TYPE_MYANMAR;
    }

    return FONT_TYPE_LATIN;
}

bool font_manager_lang_is_rtl(const char * lang_code)
{
    if(!lang_code) return false;

    return (strncmp(lang_code, "ar", 2) == 0 ||
            strncmp(lang_code, "fa", 2) == 0 ||
            strncmp(lang_code, "ur", 2) == 0 ||
            strncmp(lang_code, "he", 2) == 0);
}
