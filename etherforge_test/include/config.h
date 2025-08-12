#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char interface[32];
    uint32_t cycle_time_us;
    uint32_t timeout_ms;
} network_config_t;

typedef struct {
    int rt_priority;
    int cpu_affinity[8];
    int cpu_count;
    uint32_t buffer_size;
} performance_config_t;

typedef struct {
    char level[16];
    char file[256];
    char max_size[16];
} logging_config_t;

typedef struct {
    char bind_address[64];
    uint16_t port;
    uint32_t max_clients;
} security_config_t;

typedef struct {
    network_config_t network;
    performance_config_t performance;
    logging_config_t logging;
    security_config_t security;
} config_t;

int config_load(config_t *config, const char *filename);
void config_set_defaults(config_t *config);
void config_print(const config_t *config);

#endif