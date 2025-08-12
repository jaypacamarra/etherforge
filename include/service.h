#ifndef SERVICE_H
#define SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "protocol.h"
#include "config.h"

#define MAX_CLIENTS 32
#define MAX_SLAVES 256
#define PDO_BUFFER_SIZE 8192

typedef struct {
    struct sockaddr_in addr;
    socklen_t addr_len;
    uint32_t last_seen;
    bool active;
} client_info_t;

typedef struct {
    volatile uint32_t write_idx;
    volatile uint32_t read_idx;
    uint8_t buffer[PDO_BUFFER_SIZE];
    uint32_t mask;
} pdo_buffer_t;

typedef struct {
    uint32_t slave_id;
    char name[32];
    uint32_t vendor_id;
    uint32_t product_code;
    bool online;
    uint32_t input_size;
    uint32_t output_size;
} slave_info_t;

typedef struct {
    char interface_name[32];
    bool network_active;
    uint32_t slave_count;
    slave_info_t slaves[MAX_SLAVES];
    uint8_t *pdo_input;
    uint8_t *pdo_output;
    uint32_t input_size;
    uint32_t output_size;
} ethercat_context_t;

typedef struct {
    int socket_fd;
    struct sockaddr_in bind_addr;
    client_info_t clients[MAX_CLIENTS];
    uint32_t client_count;
    pthread_mutex_t client_lock;
    
    pthread_t network_thread;
    pthread_t rt_thread;
    pthread_t mgmt_thread;
    bool threads_running;
    
    ethercat_context_t ec_ctx;
    pdo_buffer_t pdo_buffer;
    
    config_t config;
    
    volatile bool shutdown_requested;
} service_context_t;

int service_init(service_context_t *ctx, const char *config_file);
int service_start(service_context_t *ctx);
void service_stop(service_context_t *ctx);
void service_cleanup(service_context_t *ctx);

void* network_thread_func(void *arg);
void* rt_thread_func(void *arg);
void* mgmt_thread_func(void *arg);

int handle_client_command(service_context_t *ctx, const udp_command_t *cmd,
                         udp_response_t *resp, struct sockaddr_in *client_addr);


#endif
