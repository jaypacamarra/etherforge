#include "logging.h"
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

static FILE *log_file = NULL;
static log_level_t current_level = LOG_LEVEL_INFO;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool log_to_console = true;

static const char* level_strings[] = {
    "ERROR",
    "WARN ",
    "INFO ",
    "DEBUG"
};

static log_level_t parse_log_level(const char *level_str) {
    if (!level_str) return LOG_LEVEL_INFO;
    
    if (strcasecmp(level_str, "error") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(level_str, "warn") == 0) return LOG_LEVEL_WARN;
    if (strcasecmp(level_str, "info") == 0) return LOG_LEVEL_INFO;
    if (strcasecmp(level_str, "debug") == 0) return LOG_LEVEL_DEBUG;
    
    return LOG_LEVEL_INFO;
}

int logging_init(const char *log_file_path, const char *level_str) {
    pthread_mutex_lock(&log_mutex);
    
    current_level = parse_log_level(level_str);
    
    if (log_file_path && strcmp(log_file_path, "console") != 0) {
        log_file = fopen(log_file_path, "a");
        if (!log_file) {
            pthread_mutex_unlock(&log_mutex);
            fprintf(stderr, "Failed to open log file: %s\n", log_file_path);
            return -1;
        }
        log_to_console = false;
    }
    
    pthread_mutex_unlock(&log_mutex);
    return 0;
}

void logging_cleanup(void) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void log_message(log_level_t level, const char *file, int line, const char *fmt, ...) {
    if (level > current_level) return;
    
    pthread_mutex_lock(&log_mutex);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    const char *basename = strrchr(file, '/');
    if (!basename) basename = file;
    else basename++;
    
    FILE *output = log_to_console ? stdout : log_file;
    if (!output) output = stdout;
    
    fprintf(output, "[%s] %s %s:%d - ", timestamp, level_strings[level], basename, line);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(output, fmt, args);
    va_end(args);
    
    fprintf(output, "\n");
    fflush(output);
    
    pthread_mutex_unlock(&log_mutex);
}

void log_hex_dump(log_level_t level, const char *prefix, const void *data, size_t len) {
    if (level > current_level) return;
    
    const uint8_t *bytes = (const uint8_t *)data;
    char hex_str[49];  
    char ascii_str[17]; 
    
    pthread_mutex_lock(&log_mutex);
    
    FILE *output = log_to_console ? stdout : log_file;
    if (!output) output = stdout;
    
    fprintf(output, "%s (length: %zu bytes):\n", prefix, len);
    
    for (size_t i = 0; i < len; i += 16) {
        memset(hex_str, 0, sizeof(hex_str));
        memset(ascii_str, 0, sizeof(ascii_str));
        
        for (size_t j = 0; j < 16 && (i + j) < len; j++) {
            uint8_t byte = bytes[i + j];
            sprintf(hex_str + j * 3, "%02x ", byte);
            ascii_str[j] = (byte >= 32 && byte <= 126) ? byte : '.';
        }
        
        fprintf(output, "  %04zx: %-48s |%s|\n", i, hex_str, ascii_str);
    }
    
    fflush(output);
    pthread_mutex_unlock(&log_mutex);
}