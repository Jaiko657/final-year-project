#pragma once
#include <stdarg.h>
#include <stdbool.h>

// Use names that won't collide with raylib.h
typedef enum {
    LOG_LVL_TRACE = 0,
    LOG_LVL_DEBUG,
    LOG_LVL_INFO,
    LOG_LVL_WARN,
    LOG_LVL_ERROR,
    LOG_LVL_FATAL
} log_level_t;

typedef struct {
    const char* name;   // optional category name, e.g. "ECS","ASSET"
} log_cat_t;

typedef void (*log_sink_fn)(log_level_t lvl, const log_cat_t* cat, const char* fmt, va_list ap);

// Core API
void log_set_sink(log_sink_fn sink);
void log_set_min_level(log_level_t lvl);
bool log_would_log(log_level_t lvl);
void log_msg(log_level_t lvl, const log_cat_t* cat, const char* fmt, ...);

// Convenience
#define LOGC(cat, lvl, fmt, ...) do { if (log_would_log(lvl)) { log_cat_t _cat_tmp_ = (cat); log_msg((lvl), &_cat_tmp_, fmt, ##__VA_ARGS__); } } while (0)
#define LOG(lvl, fmt, ...)       do{ static const log_cat_t _anon = { NULL }; if(log_would_log(lvl)) log_msg((lvl), &_anon, fmt, ##__VA_ARGS__); }while(0)

// Common categories
#define LOGCAT(name) ((log_cat_t){ #name })
#define LOGCAT_ECS   ((log_cat_t){ "ECS" })
#define LOGCAT_REND  ((log_cat_t){ "RENDER" })
#define LOGCAT_ASSET ((log_cat_t){ "ASSET" })
#define LOGCAT_MAIN  ((log_cat_t){ "MAIN" })
