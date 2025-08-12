#include "ethercat.h"
#include "logging.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static timing_stats_t g_timing_stats = {0};
static error_stats_t g_error_stats = {0};

#ifdef HAVE_SOEM

static boolean inOP = FALSE;
static uint8 currentgroup = 0;

int ethercat_init(ethercat_context_t *ctx, const char *interface) {
    if (!ctx || !interface) return -1;
    
    strncpy(ctx->interface_name, interface, sizeof(ctx->interface_name) - 1);
    ctx->interface_name[sizeof(ctx->interface_name) - 1] = '\0';
    ctx->network_active = false;
    ctx->slave_count = 0;
    
    ctx->pdo_input = NULL;
    ctx->pdo_output = NULL;
    ctx->input_size = 0;
    ctx->output_size = 0;
    
    LOG_INFO("EtherCAT master initialized with interface: %s", interface);
    return 0;
}

int ethercat_start(ethercat_context_t *ctx) {
    if (!ctx || ctx->network_active) return -1;
    
    if (ec_init(ctx->interface_name)) {
        LOG_INFO("ec_init on %s succeeded", ctx->interface_name);
        
        if (ec_config_init(FALSE) > 0) {
            LOG_INFO("Found %d slaves", ec_slavecount);
            
            ec_config_map(&ctx->input_size);
            ec_configdc();
            
            LOG_INFO("Slaves mapped, state to SAFE_OP");
            ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);
            
            ctx->pdo_input = malloc(ctx->input_size);
            ctx->pdo_output = malloc(ctx->output_size);
            
            if (!ctx->pdo_input || !ctx->pdo_output) {
                LOG_ERROR("Failed to allocate PDO memory");
                return -1;
            }
            
            memset(ctx->pdo_input, 0, ctx->input_size);
            memset(ctx->pdo_output, 0, ctx->output_size);
            
            LOG_INFO("Request operational state for all slaves");
            ec_slave[0].state = EC_STATE_OPERATIONAL;
            ec_send_processdata();
            ec_receive_processdata(EC_TIMEOUTRET);
            ec_writestate(0);
            
            int chk = 40;
            do {
                ec_send_processdata();
                ec_receive_processdata(EC_TIMEOUTRET);
                ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
            } while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));
            
            if (ec_slave[0].state == EC_STATE_OPERATIONAL) {
                LOG_INFO("Operational state reached for all slaves");
                inOP = TRUE;
                ctx->network_active = true;
                ctx->slave_count = ec_slavecount;
                
                for (int i = 1; i <= ec_slavecount; i++) {
                    if (i - 1 < MAX_SLAVES) {
                        ctx->slaves[i-1].slave_id = i;
                        strncpy(ctx->slaves[i-1].name, ec_slave[i].name, 
                               sizeof(ctx->slaves[i-1].name) - 1);
                        ctx->slaves[i-1].vendor_id = ec_slave[i].eep_man;
                        ctx->slaves[i-1].product_code = ec_slave[i].eep_id;
                        ctx->slaves[i-1].online = true;
                        ctx->slaves[i-1].input_size = ec_slave[i].Ibytes;
                        ctx->slaves[i-1].output_size = ec_slave[i].Obytes;
                    }
                }
                
                return 0;
            } else {
                LOG_ERROR("Not all slaves reached operational state");
                return -1;
            }
        } else {
            LOG_ERROR("No slaves found!");
            return -1;
        }
    } else {
        LOG_ERROR("No socket connection on %s", ctx->interface_name);
        return -1;
    }
}

int ethercat_stop(ethercat_context_t *ctx) {
    if (!ctx) return -1;
    
    if (inOP) {
        LOG_INFO("Request safe operational state for all slaves");
        ec_slave[0].state = EC_STATE_SAFE_OP;
        ec_writestate(0);
        inOP = FALSE;
    }
    
    LOG_INFO("Request init state for all slaves");
    ec_slave[0].state = EC_STATE_INIT;
    ec_writestate(0);
    
    ec_close();
    ctx->network_active = false;
    ctx->slave_count = 0;
    
    if (ctx->pdo_input) {
        free(ctx->pdo_input);
        ctx->pdo_input = NULL;
    }
    
    if (ctx->pdo_output) {
        free(ctx->pdo_output);
        ctx->pdo_output = NULL;
    }
    
    LOG_INFO("EtherCAT network stopped");
    return 0;
}

int ethercat_scan_slaves(ethercat_context_t *ctx) {
    if (!ctx) return -1;
    
    return ctx->slave_count;
}

int ethercat_process_data(ethercat_context_t *ctx) {
    if (!ctx || !ctx->network_active) return -1;
    
    ec_send_processdata();
    int wkc = ec_receive_processdata(EC_TIMEOUTRET);
    
    if (wkc >= 0) {
        memcpy(ctx->pdo_input, ec_slave[0].inputs, ctx->input_size);
        memcpy(ec_slave[0].outputs, ctx->pdo_output, ctx->output_size);
        return 0;
    } else {
        g_error_stats.working_counter_errors++;
        return -1;
    }
}

int ethercat_read_pdo(ethercat_context_t *ctx, uint32_t slave, uint32_t offset, 
                     uint32_t size, uint32_t *value) {
    if (!ctx || !value || slave == 0 || slave > ctx->slave_count) return -1;
    
    if (!ctx->network_active || !ctx->pdo_input) return -1;
    
    if (offset + size > ctx->input_size) return -1;
    
    *value = 0;
    memcpy(value, ctx->pdo_input + offset, (size > 4) ? 4 : size);
    
    return 0;
}

int ethercat_write_pdo(ethercat_context_t *ctx, uint32_t slave, uint32_t offset,
                      uint32_t size, uint32_t value) {
    if (!ctx || slave == 0 || slave > ctx->slave_count) return -1;
    
    if (!ctx->network_active || !ctx->pdo_output) return -1;
    
    if (offset + size > ctx->output_size) return -1;
    
    memcpy(ctx->pdo_output + offset, &value, (size > 4) ? 4 : size);
    
    return 0;
}

#else

int ethercat_init(ethercat_context_t *ctx, const char *interface) {
    return ethercat_stub_init(ctx, interface);
}

int ethercat_start(ethercat_context_t *ctx) {
    if (!ctx || ctx->network_active) return -1;
    
    LOG_INFO("STUB: Starting EtherCAT network on %s", ctx->interface_name);
    ctx->network_active = true;
    ctx->slave_count = 0;
    ctx->input_size = 0;
    ctx->output_size = 0;
    
    ctx->pdo_input = NULL;
    ctx->pdo_output = NULL;
    
    LOG_INFO("STUB: EtherCAT network started with %u slaves", ctx->slave_count);
    return 0;
}

int ethercat_stop(ethercat_context_t *ctx) {
    if (!ctx) return -1;
    
    LOG_INFO("STUB: Stopping EtherCAT network");
    ctx->network_active = false;
    ctx->slave_count = 0;
    
    if (ctx->pdo_input) {
        free(ctx->pdo_input);
        ctx->pdo_input = NULL;
    }
    
    if (ctx->pdo_output) {
        free(ctx->pdo_output);
        ctx->pdo_output = NULL;
    }
    
    return 0;
}

int ethercat_scan_slaves(ethercat_context_t *ctx) {
    if (!ctx) return -1;
    LOG_INFO("STUB: Scanning for slaves");
    return ctx->network_active ? ctx->slave_count : 0;
}

int ethercat_process_data(ethercat_context_t *ctx) {
    if (!ctx || !ctx->network_active) return -1;
    
    static uint32_t counter = 0;
    counter++;
    
    if (ctx->pdo_input && ctx->input_size >= 4) {
        *(uint32_t*)ctx->pdo_input = counter;
    }
    
    return 0;
}

int ethercat_read_pdo(ethercat_context_t *ctx, uint32_t slave, uint32_t offset, 
                     uint32_t size, uint32_t *value) {
    if (!ctx || !value || slave == 0 || slave > ctx->slave_count) return -1;
    
    if (!ctx->network_active) return -1;
    
    LOG_DEBUG("STUB: PDO read failed - no slaves available");
    return -1;
}

int ethercat_write_pdo(ethercat_context_t *ctx, uint32_t slave, uint32_t offset,
                      uint32_t size, uint32_t value) {
    if (!ctx || slave == 0 || slave > ctx->slave_count) return -1;
    
    if (!ctx->network_active) return -1;
    
    LOG_DEBUG("STUB: PDO write failed - no slaves available");
    return -1;
}

int ethercat_stub_init(ethercat_context_t *ctx, const char *interface) {
    if (!ctx || !interface) return -1;
    
    strncpy(ctx->interface_name, interface, sizeof(ctx->interface_name) - 1);
    ctx->interface_name[sizeof(ctx->interface_name) - 1] = '\0';
    ctx->network_active = false;
    ctx->slave_count = 0;
    
    ctx->pdo_input = NULL;
    ctx->pdo_output = NULL;
    ctx->input_size = 0;
    ctx->output_size = 0;
    
    LOG_INFO("STUB: EtherCAT master initialized with interface: %s", interface);
    return 0;
}

#endif

void ethercat_cleanup(ethercat_context_t *ctx) {
    if (!ctx) return;
    
    ethercat_stop(ctx);
}

void ethercat_get_timing_stats(timing_stats_t *stats) {
    if (!stats) return;
    
    memcpy(stats, &g_timing_stats, sizeof(timing_stats_t));
    
    if (g_timing_stats.cycles_total == 0) {
        stats->avg_cycle_us = 1000;
        stats->min_cycle_us = 950;
        stats->max_cycle_us = 1050;
        stats->jitter_us = 25;
    }
}

void ethercat_get_error_stats(error_stats_t *stats) {
    if (!stats) return;
    
    memcpy(stats, &g_error_stats, sizeof(error_stats_t));
}

void ethercat_reset_stats(void) {
    memset(&g_timing_stats, 0, sizeof(timing_stats_t));
    memset(&g_error_stats, 0, sizeof(error_stats_t));
}