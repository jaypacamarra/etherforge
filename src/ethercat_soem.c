#include "ethercat.h"
#include "logging.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_SOEM

static ecx_contextt ec_context;
static boolean inOP = FALSE;

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
    
    if (ecx_init(&ec_context, ctx->interface_name)) {
        LOG_INFO("ecx_init on %s succeeded", ctx->interface_name);
        
        if (ecx_config_init(&ec_context) > 0) {
            LOG_INFO("Found %d slaves", ec_context.slavecount);
            
            // Allocate IOmap size based on total slaves
            ctx->input_size = 1024;  // Conservative estimate
            ctx->output_size = 1024; // Conservative estimate
            
            ctx->pdo_input = malloc(ctx->input_size);
            ctx->pdo_output = malloc(ctx->output_size);
            
            if (!ctx->pdo_input || !ctx->pdo_output) {
                LOG_ERROR("Failed to allocate PDO memory");
                return -1;
            }
            
            memset(ctx->pdo_input, 0, ctx->input_size);
            memset(ctx->pdo_output, 0, ctx->output_size);
            
            // Map slaves to IOmap
            ecx_config_map_group(&ec_context, ctx->pdo_input, 0);
            ecx_configdc(&ec_context);
            
            LOG_INFO("Slaves mapped, state to SAFE_OP");
            ecx_statecheck(&ec_context, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);
            
            LOG_INFO("Request operational state for all slaves");
            ec_context.slavelist[0].state = EC_STATE_OPERATIONAL;
            ecx_send_processdata(&ec_context);
            ecx_receive_processdata(&ec_context, EC_TIMEOUTRET);
            ecx_writestate(&ec_context, 0);
            
            int chk = 40;
            do {
                ecx_send_processdata(&ec_context);
                ecx_receive_processdata(&ec_context, EC_TIMEOUTRET);
                ecx_statecheck(&ec_context, 0, EC_STATE_OPERATIONAL, 50000);
            } while (chk-- && (ec_context.slavelist[0].state != EC_STATE_OPERATIONAL));
            
            if (ec_context.slavelist[0].state == EC_STATE_OPERATIONAL) {
                LOG_INFO("Operational state reached for all slaves");
                inOP = TRUE;
                ctx->network_active = true;
                ctx->slave_count = ec_context.slavecount;
                
                for (int i = 1; i <= ec_context.slavecount; i++) {
                    if (i - 1 < MAX_SLAVES) {
                        ctx->slaves[i - 1].slave_id = i;
                        strncpy(ctx->slaves[i - 1].name, ec_context.slavelist[i].name, 
                               sizeof(ctx->slaves[i - 1].name) - 1);
                        ctx->slaves[i - 1].vendor_id = ec_context.slavelist[i].eep_man;
                        ctx->slaves[i - 1].product_code = ec_context.slavelist[i].eep_id;
                        ctx->slaves[i - 1].online = true;
                        ctx->slaves[i - 1].input_size = ec_context.slavelist[i].Ibytes;
                        ctx->slaves[i - 1].output_size = ec_context.slavelist[i].Obytes;
                    }
                }
                
                return 0;
            } else {
                LOG_ERROR("Not all slaves reached operational state");
                return -1;
            }
        } else {
            LOG_ERROR("No slaves found");
            return -1;
        }
    } else {
        LOG_ERROR("ecx_init failed");
        return -1;
    }
    
    return -1;
}

int ethercat_stop(ethercat_context_t *ctx) {
    if (!ctx) return -1;
    
    if (ctx->network_active && inOP) {
        LOG_INFO("Stopping EtherCAT network");
        
        ec_context.slavelist[0].state = EC_STATE_SAFE_OP;
        ecx_writestate(&ec_context, 0);
        ecx_statecheck(&ec_context, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);
        
        if (ctx->pdo_input) {
            free(ctx->pdo_input);
            ctx->pdo_input = NULL;
        }
        
        if (ctx->pdo_output) {
            free(ctx->pdo_output);
            ctx->pdo_output = NULL;
        }
        
        inOP = FALSE;
    }
    
    ecx_close(&ec_context);
    ctx->network_active = false;
    ctx->slave_count = 0;
    
    return 0;
}

int ethercat_process_data(ethercat_context_t *ctx) {
    if (!ctx || !ctx->network_active || !inOP) return -1;
    
    ecx_send_processdata(&ec_context);
    
    int wkc = ecx_receive_processdata(&ec_context, EC_TIMEOUTRET);
    
    if (wkc >= 0 && ctx->pdo_input) {
        memcpy(ctx->pdo_input, ec_context.slavelist[0].inputs, ctx->input_size);
    }
    
    return (wkc >= 0) ? 0 : -1;
}

int ethercat_read_pdo(ethercat_context_t *ctx, uint32_t slave, uint32_t offset, 
                     uint32_t size, uint32_t *value) {
    if (!ctx || !value || slave == 0 || slave > ctx->slave_count) return -1;
    
    if (!ctx->network_active || !ctx->pdo_input) return -1;
    
    if (offset + size > ctx->input_size) return -1;
    
    *value = 0;
    memcpy(value, ctx->pdo_input + offset, (size > 4) ? 4 : size);
    
    LOG_DEBUG("PDO read slave=%u, offset=%u, size=%u, value=0x%08X", 
              slave, offset, size, *value);
    return 0;
}

int ethercat_write_pdo(ethercat_context_t *ctx, uint32_t slave, uint32_t offset,
                      uint32_t size, uint32_t value) {
    if (!ctx || slave == 0 || slave > ctx->slave_count) return -1;
    
    if (!ctx->network_active || !ctx->pdo_output) return -1;
    
    if (offset + size > ctx->output_size) return -1;
    
    memcpy(ctx->pdo_output + offset, &value, (size > 4) ? 4 : size);
    
    if (ec_context.slavelist[0].outputs) {
        memcpy(ec_context.slavelist[0].outputs, ctx->pdo_output, ctx->output_size);
    }
    
    LOG_DEBUG("PDO write slave=%u, offset=%u, size=%u, value=0x%08X", 
              slave, offset, size, value);
    return 0;
}

int ethercat_scan_slaves(ethercat_context_t *ctx) {
    if (!ctx) return -1;
    
    LOG_INFO("Scanning for slaves");
    // In the new API, this is typically done during config_init
    return ctx->slave_count;
}

void ethercat_cleanup(ethercat_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->network_active) {
        ethercat_stop(ctx);
    }
    
    LOG_INFO("EtherCAT master cleaned up");
}

void ethercat_get_timing_stats(timing_stats_t *stats) {
    // TODO: Implement timing statistics collection
    if (stats) {
        memset(stats, 0, sizeof(timing_stats_t));
    }
}

void ethercat_get_error_stats(error_stats_t *stats) {
    // TODO: Implement error statistics collection
    if (stats) {
        memset(stats, 0, sizeof(error_stats_t));
    }
}

void ethercat_reset_stats(void) {
    // TODO: Implement statistics reset
}

#endif