#include "config.h"
#include "protocol.h"
#include "logging.h"
#include <string.h>
#include <yaml.h>
#include <stdlib.h>

void config_set_defaults(config_t *config) {
    if (!config) return;
    
    strcpy(config->network.interface, "eth0");
    config->network.cycle_time_us = 1000;
    config->network.timeout_ms = 1000;
    
    config->performance.rt_priority = 50;
    config->performance.cpu_count = 1;
    config->performance.cpu_affinity[0] = 1;
    config->performance.buffer_size = 8192;
    
    strcpy(config->logging.level, "info");
    strcpy(config->logging.file, "/var/log/etherforged.log");
    strcpy(config->logging.max_size, "100MB");
    
    strcpy(config->security.bind_address, "127.0.0.1");
    config->security.port = PROTOCOL_PORT;
    config->security.max_clients = 16;
}

static int parse_yaml_value(yaml_parser_t *parser, const char *key, config_t *config) {
    yaml_token_t token;
    char value[256];
    
    if (!yaml_parser_scan(parser, &token)) {
        return -1;
    }
    
    if (token.type != YAML_SCALAR_TOKEN) {
        yaml_token_delete(&token);
        return -1;
    }
    
    strncpy(value, (char*)token.data.scalar.value, sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';
    yaml_token_delete(&token);
    
    if (strcmp(key, "interface") == 0) {
        strncpy(config->network.interface, value, sizeof(config->network.interface) - 1);
    } else if (strcmp(key, "cycle_time_us") == 0) {
        config->network.cycle_time_us = (uint32_t)atol(value);
    } else if (strcmp(key, "timeout_ms") == 0) {
        config->network.timeout_ms = (uint32_t)atol(value);
    } else if (strcmp(key, "rt_priority") == 0) {
        config->performance.rt_priority = atoi(value);
    } else if (strcmp(key, "buffer_size") == 0) {
        config->performance.buffer_size = (uint32_t)atol(value);
    } else if (strcmp(key, "level") == 0) {
        strncpy(config->logging.level, value, sizeof(config->logging.level) - 1);
    } else if (strcmp(key, "file") == 0) {
        strncpy(config->logging.file, value, sizeof(config->logging.file) - 1);
    } else if (strcmp(key, "bind_address") == 0) {
        strncpy(config->security.bind_address, value, sizeof(config->security.bind_address) - 1);
    } else if (strcmp(key, "port") == 0) {
        config->security.port = (uint16_t)atoi(value);
    } else if (strcmp(key, "max_clients") == 0) {
        config->security.max_clients = (uint32_t)atol(value);
    }
    
    return 0;
}

int config_load(config_t *config, const char *filename) {
    if (!config || !filename) return -1;
    
    config_set_defaults(config);
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        LOG_WARN("Config file %s not found, using defaults", filename);
        return 0;
    }
    
    yaml_parser_t parser;
    yaml_token_t token;
    
    if (!yaml_parser_initialize(&parser)) {
        LOG_ERROR("Failed to initialize YAML parser");
        fclose(file);
        return -1;
    }
    
    yaml_parser_set_input_file(&parser, file);
    
    char current_key[64] = {0};
    bool in_section = false;
    
    do {
        if (!yaml_parser_scan(&parser, &token)) {
            LOG_ERROR("YAML parsing error");
            break;
        }
        
        switch (token.type) {
            case YAML_KEY_TOKEN:
                break;
                
            case YAML_SCALAR_TOKEN:
                if (!in_section) {
                    strncpy(current_key, (char*)token.data.scalar.value, sizeof(current_key) - 1);
                    in_section = true;
                } else {
                    if (parse_yaml_value(&parser, current_key, config) < 0) {
                        LOG_WARN("Failed to parse config value for key: %s", current_key);
                    }
                    in_section = false;
                }
                break;
                
            default:
                break;
        }
        
        if (token.type != YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
        }
    } while (token.type != YAML_STREAM_END_TOKEN);
    
    yaml_token_delete(&token);
    yaml_parser_delete(&parser);
    fclose(file);
    
    LOG_INFO("Configuration loaded from %s", filename);
    return 0;
}

void config_print(const config_t *config) {
    if (!config) return;
    
    LOG_INFO("Configuration:");
    LOG_INFO("  Network interface: %s", config->network.interface);
    LOG_INFO("  Cycle time: %u us", config->network.cycle_time_us);
    LOG_INFO("  RT priority: %d", config->performance.rt_priority);
    LOG_INFO("  Bind address: %s:%u", config->security.bind_address, config->security.port);
    LOG_INFO("  Max clients: %u", config->security.max_clients);
}