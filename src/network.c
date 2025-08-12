#include "service.h"
#include "protocol.h"
#include "logging.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

static int setup_socket(service_context_t *ctx) {
    ctx->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->socket_fd < 0) {
        LOG_ERROR("Failed to create UDP socket: %s", strerror(errno));
        return -1;
    }
    
    int flags = fcntl(ctx->socket_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(ctx->socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG_WARN("Failed to set socket non-blocking: %s", strerror(errno));
    }
    
    // Note: SO_REUSEADDR removed to prevent multiple service instances
    
    memset(&ctx->bind_addr, 0, sizeof(ctx->bind_addr));
    ctx->bind_addr.sin_family = AF_INET;
    ctx->bind_addr.sin_port = htons(ctx->config.security.port);
    
    if (inet_pton(AF_INET, ctx->config.security.bind_address, &ctx->bind_addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid bind address: %s", ctx->config.security.bind_address);
        close(ctx->socket_fd);
        return -1;
    }
    
    if (bind(ctx->socket_fd, (struct sockaddr*)&ctx->bind_addr, sizeof(ctx->bind_addr)) < 0) {
        LOG_ERROR("Failed to bind socket: %s", strerror(errno));
        close(ctx->socket_fd);
        return -1;
    }
    
    LOG_INFO("UDP server bound to %s:%d", ctx->config.security.bind_address, 
             ctx->config.security.port);
    
    return 0;
}

static void add_client(service_context_t *ctx, struct sockaddr_in *client_addr) {
    pthread_mutex_lock(&ctx->client_lock);
    
    for (uint32_t i = 0; i < MAX_CLIENTS; i++) {
        if (!ctx->clients[i].active) {
            memcpy(&ctx->clients[i].addr, client_addr, sizeof(struct sockaddr_in));
            ctx->clients[i].addr_len = sizeof(struct sockaddr_in);
            ctx->clients[i].active = true;
            ctx->clients[i].last_seen = time(NULL);
            
            if (i >= ctx->client_count) {
                ctx->client_count = i + 1;
            }
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, INET_ADDRSTRLEN);
            LOG_INFO("Client connected: %s:%d (slot %d)", client_ip, 
                     ntohs(client_addr->sin_port), i);
            break;
        }
    }
    
    pthread_mutex_unlock(&ctx->client_lock);
}

static void update_client(service_context_t *ctx, struct sockaddr_in *client_addr) {
    pthread_mutex_lock(&ctx->client_lock);
    
    for (uint32_t i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].active &&
            memcmp(&ctx->clients[i].addr, client_addr, sizeof(struct sockaddr_in)) == 0) {
            ctx->clients[i].last_seen = time(NULL);
            pthread_mutex_unlock(&ctx->client_lock);
            return;
        }
    }
    
    pthread_mutex_unlock(&ctx->client_lock);
    add_client(ctx, client_addr);
}

static void cleanup_stale_clients(service_context_t *ctx) {
    uint32_t current_time = time(NULL);
    const uint32_t timeout = 300;
    
    pthread_mutex_lock(&ctx->client_lock);
    
    for (uint32_t i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].active && 
            (current_time - ctx->clients[i].last_seen) > timeout) {
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ctx->clients[i].addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            LOG_INFO("Client timeout: %s:%d", client_ip, 
                     ntohs(ctx->clients[i].addr.sin_port));
            
            ctx->clients[i].active = false;
        }
    }
    
    while (ctx->client_count > 0 && !ctx->clients[ctx->client_count - 1].active) {
        ctx->client_count--;
    }
    
    pthread_mutex_unlock(&ctx->client_lock);
}

void* network_thread_func(void *arg) {
    service_context_t *ctx = (service_context_t*)arg;
    
    if (setup_socket(ctx) < 0) {
        LOG_ERROR("Network thread failed to initialize");
        return NULL;
    }
    
    LOG_INFO("Network thread started");
    
    udp_command_t cmd;
    udp_response_t resp;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    uint32_t last_cleanup = time(NULL);
    
    while (ctx->threads_running && !ctx->shutdown_requested) {
        ssize_t received = recvfrom(ctx->socket_fd, &cmd, sizeof(cmd), 0,
                                   (struct sockaddr*)&client_addr, &client_len);
        
        if (received < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                LOG_ERROR("recvfrom error: %s", strerror(errno));
            }
            usleep(1000);
            
            uint32_t now = time(NULL);
            if (now - last_cleanup > 60) {
                cleanup_stale_clients(ctx);
                last_cleanup = now;
            }
            continue;
        }
        
        if (received < sizeof(udp_command_t)) {
            LOG_WARN("Received truncated packet (%zd bytes)", received);
            continue;
        }
        
        update_client(ctx, &client_addr);
        
        if (handle_client_command(ctx, &cmd, &resp, &client_addr) == 0) {
            ssize_t sent = sendto(ctx->socket_fd, &resp, sizeof(resp), 0,
                                 (struct sockaddr*)&client_addr, client_len);
            if (sent < 0) {
                LOG_ERROR("sendto error: %s", strerror(errno));
            }
        }
    }
    
    close(ctx->socket_fd);
    LOG_INFO("Network thread stopped");
    return NULL;
}
