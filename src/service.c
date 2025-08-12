#include "service.h"
#include "logging.h"
#include "ethercat.h"
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sched.h>
#include <errno.h>

static void set_thread_priority(int priority) {
    struct sched_param param;
    param.sched_priority = priority;
    
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        LOG_WARN("Failed to set real-time priority %d: %s", priority, strerror(errno));
    } else {
        LOG_INFO("Set thread priority to %d", priority);
    }
}

static void set_thread_affinity(const int *cpus, int count) {
    if (!cpus || count <= 0) return;
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    for (int i = 0; i < count; i++) {
        if (cpus[i] >= 0 && cpus[i] < CPU_SETSIZE) {
            CPU_SET(cpus[i], &cpuset);
        }
    }
    
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        LOG_WARN("Failed to set CPU affinity: %s", strerror(errno));
    } else {
        LOG_INFO("Set CPU affinity to %d core(s)", count);
    }
}

void* rt_thread_func(void *arg) {
    service_context_t *ctx = (service_context_t*)arg;
    
    LOG_INFO("Real-time thread starting");
    
    if (ctx->config.performance.rt_priority > 0) {
        set_thread_priority(ctx->config.performance.rt_priority);
    }
    
    if (ctx->config.performance.cpu_count > 0) {
        set_thread_affinity(ctx->config.performance.cpu_affinity, 
                           ctx->config.performance.cpu_count);
    }
    
    struct timespec next_cycle;
    clock_gettime(CLOCK_MONOTONIC, &next_cycle);
    
    uint64_t cycle_ns = ctx->config.network.cycle_time_us * 1000ULL;
    uint32_t cycle_count = 0;
    
    while (ctx->threads_running && !ctx->shutdown_requested) {
        if (ctx->ec_ctx.network_active) {
            int result = ethercat_process_data(&ctx->ec_ctx);
            if (result != 0) {
                LOG_DEBUG("EtherCAT process data failed");
            }
            cycle_count++;
        }
        
        next_cycle.tv_nsec += cycle_ns;
        if (next_cycle.tv_nsec >= 1000000000) {
            next_cycle.tv_sec++;
            next_cycle.tv_nsec -= 1000000000;
        }
        
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_cycle, NULL);
    }
    
    LOG_INFO("Real-time thread stopping (processed %u cycles)", cycle_count);
    return NULL;
}

void* mgmt_thread_func(void *arg) {
    service_context_t *ctx = (service_context_t*)arg;
    
    LOG_INFO("Management thread starting");
    
    uint32_t last_stats_log = time(NULL);
    
    while (ctx->threads_running && !ctx->shutdown_requested) {
        sleep(10);
        
        uint32_t now = time(NULL);
        if (now - last_stats_log > 60) {
            LOG_INFO("Status: Network=%s, Slaves=%u, Clients=%u",
                     ctx->ec_ctx.network_active ? "UP" : "DOWN",
                     ctx->ec_ctx.slave_count,
                     ctx->client_count);
            last_stats_log = now;
        }
    }
    
    LOG_INFO("Management thread stopping");
    return NULL;
}

int service_init(service_context_t *ctx, const char *config_file) {
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(service_context_t));
    
    if (config_load(&ctx->config, config_file) < 0) {
        LOG_ERROR("Failed to load configuration");
        return -1;
    }
    
    config_print(&ctx->config);
    
    if (ethercat_init(&ctx->ec_ctx, ctx->config.network.interface) < 0) {
        LOG_ERROR("Failed to initialize EtherCAT master");
        return -1;
    }
    
    if (pthread_mutex_init(&ctx->client_lock, NULL) != 0) {
        LOG_ERROR("Failed to initialize client mutex");
        return -1;
    }
    
    ctx->pdo_buffer.mask = PDO_BUFFER_SIZE - 1;
    ctx->pdo_buffer.write_idx = 0;
    ctx->pdo_buffer.read_idx = 0;
    
    ctx->socket_fd = -1;
    ctx->threads_running = false;
    ctx->shutdown_requested = false;
    
    LOG_INFO("Service initialized");
    return 0;
}

int service_start(service_context_t *ctx) {
    if (!ctx) return -1;
    
    ctx->threads_running = true;
    
    if (pthread_create(&ctx->network_thread, NULL, network_thread_func, ctx) != 0) {
        LOG_ERROR("Failed to create network thread");
        return -1;
    }
    
    if (pthread_create(&ctx->rt_thread, NULL, rt_thread_func, ctx) != 0) {
        LOG_ERROR("Failed to create real-time thread");
        ctx->threads_running = false;
        pthread_join(ctx->network_thread, NULL);
        return -1;
    }
    
    if (pthread_create(&ctx->mgmt_thread, NULL, mgmt_thread_func, ctx) != 0) {
        LOG_ERROR("Failed to create management thread");
        ctx->threads_running = false;
        pthread_join(ctx->network_thread, NULL);
        pthread_join(ctx->rt_thread, NULL);
        return -1;
    }
    
    LOG_INFO("Service started - all threads running");
    return 0;
}

void service_stop(service_context_t *ctx) {
    if (!ctx) return;
    
    LOG_INFO("Stopping service...");
    
    ctx->shutdown_requested = true;
    ctx->threads_running = false;
    
    if (pthread_join(ctx->network_thread, NULL) != 0) {
        LOG_WARN("Failed to join network thread");
    }
    
    if (pthread_join(ctx->rt_thread, NULL) != 0) {
        LOG_WARN("Failed to join real-time thread");
    }
    
    if (pthread_join(ctx->mgmt_thread, NULL) != 0) {
        LOG_WARN("Failed to join management thread");
    }
    
    LOG_INFO("All threads stopped");
}

void service_cleanup(service_context_t *ctx) {
    if (!ctx) return;
    
    ethercat_cleanup(&ctx->ec_ctx);
    
    if (ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }
    
    pthread_mutex_destroy(&ctx->client_lock);
    
    LOG_INFO("Service cleaned up");
}
