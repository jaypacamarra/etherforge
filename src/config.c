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

static int parse_yaml_value(const char *key, const char *value, config_t *config) {
    if (strcmp(key, "interface") == 0) {
        strncpy(config->network.interface, value, sizeof(config->network.interface) - 1);
        config->network.interface[sizeof(config->network.interface) - 1] = '\0';
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
        config->logging.level[sizeof(config->logging.level) - 1] = '\0';
    } else if (strcmp(key, "file") == 0) {
        strncpy(config->logging.file, value, sizeof(config->logging.file) - 1);
        config->logging.file[sizeof(config->logging.file) - 1] = '\0';
    } else if (strcmp(key, "max_size") == 0) {
        strncpy(config->logging.max_size, value, sizeof(config->logging.max_size) - 1);
        config->logging.max_size[sizeof(config->logging.max_size) - 1] = '\0';
    } else if (strcmp(key, "bind_address") == 0) {
        strncpy(config->security.bind_address, value, sizeof(config->security.bind_address) - 1);
        config->security.bind_address[sizeof(config->security.bind_address) - 1] = '\0';
    } else if (strcmp(key, "port") == 0) {
        config->security.port = (uint16_t)atoi(value);
    } else if (strcmp(key, "max_clients") == 0) {
        config->security.max_clients = (uint32_t)atol(value);
    } else if (strcmp(key, "cpu_affinity") == 0) {
        // Handle cpu_affinity array parsing - simplified for now
        config->performance.cpu_count = 1;
        config->performance.cpu_affinity[0] = atoi(value);
    } else {
        return -1; // Unknown key
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
    char current_section[64] = {0};
    bool expecting_value = false;
    bool in_array = false;
    int array_index = 0;
    
    do {
        if (!yaml_parser_scan(&parser, &token)) {
            LOG_ERROR("YAML parsing error");
            break;
        }
        
        switch (token.type) {
            case YAML_KEY_TOKEN:
                expecting_value = false;
                break;
                
            case YAML_VALUE_TOKEN:
                expecting_value = true;
                break;
                
            case YAML_FLOW_SEQUENCE_START_TOKEN:
            case YAML_BLOCK_SEQUENCE_START_TOKEN:
                in_array = true;
                array_index = 0;
                break;
                
            case YAML_FLOW_SEQUENCE_END_TOKEN:
                in_array = false;
                break;
                
            case YAML_SCALAR_TOKEN: {
                char *scalar_value = (char*)token.data.scalar.value;
                
                if (!expecting_value) {
                    // This is a key
                    if (strlen(current_section) == 0) {
                        // Top-level section name
                        strncpy(current_section, scalar_value, sizeof(current_section) - 1);
                        current_section[sizeof(current_section) - 1] = '\0';
                    } else {
                        // Key within a section
                        strncpy(current_key, scalar_value, sizeof(current_key) - 1);
                        current_key[sizeof(current_key) - 1] = '\0';
                    }
                } else {
                    // This is a value
                    if (strcmp(current_key, "cpu_affinity") == 0 && in_array) {
                        // Handle array values for cpu_affinity
                        if (array_index < 8) {
                            config->performance.cpu_affinity[array_index] = atoi(scalar_value);
                            array_index++;
                            config->performance.cpu_count = array_index;
                        }
                    } else {
                        // Regular key-value pair
                        if (parse_yaml_value(current_key, scalar_value, config) < 0) {
                            LOG_DEBUG("Unknown config key: %s", current_key);
                        }
                    }
                    expecting_value = false;
                }
                break;
            }
            
            case YAML_BLOCK_MAPPING_START_TOKEN:
                // Reset section when entering a new mapping
                if (strlen(current_key) > 0) {
                    strncpy(current_section, current_key, sizeof(current_section) - 1);
                    current_section[sizeof(current_section) - 1] = '\0';
                    current_key[0] = '\0';
                }
                break;
                
            case YAML_BLOCK_END_TOKEN:
                // Reset section when exiting a mapping
                current_section[0] = '\0';
                current_key[0] = '\0';
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