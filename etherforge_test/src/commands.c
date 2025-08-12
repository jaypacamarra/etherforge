#include "daemon.h"
#include "protocol.h"
#include "ethercat.h"
#include "logging.h"
#include <string.h>
#include <arpa/inet.h>

static int handle_network_command(daemon_context_t *ctx, const udp_command_t *cmd, udp_response_t *resp) {
    switch (cmd->command_id) {
        case NET_START: {
            LOG_INFO("Network start command received");
            if (ctx->ec_ctx.network_active) {
                protocol_create_response(resp, STATUS_ERROR, ERR_NETWORK_NOT_READY, NULL, 0);
                return 0;
            }
            
            int result = ethercat_start(&ctx->ec_ctx);
            if (result == 0) {
                protocol_create_response(resp, STATUS_SUCCESS, ERR_NONE, NULL, 0);
                LOG_INFO("EtherCAT network started");
            } else {
                protocol_create_response(resp, STATUS_ERROR, ERR_INTERNAL, NULL, 0);
                LOG_ERROR("Failed to start EtherCAT network");
            }
            break;
        }
        
        case NET_STOP: {
            LOG_INFO("Network stop command received");
            ethercat_stop(&ctx->ec_ctx);
            protocol_create_response(resp, STATUS_SUCCESS, ERR_NONE, NULL, 0);
            LOG_INFO("EtherCAT network stopped");
            break;
        }
        
        case NET_SCAN: {
            LOG_INFO("Network scan command received");
            int slave_count = ethercat_scan_slaves(&ctx->ec_ctx);
            if (slave_count >= 0) {
                uint8_t payload[4];
                uint32_t *count_ptr = (uint32_t*)payload;
                *count_ptr = htonl((uint32_t)slave_count);
                protocol_create_response(resp, STATUS_SUCCESS, ERR_NONE, payload, 4);
                LOG_INFO("Network scan found %d slaves", slave_count);
            } else {
                protocol_create_response(resp, STATUS_ERROR, ERR_INTERNAL, NULL, 0);
                LOG_ERROR("Network scan failed");
            }
            break;
        }
        
        case NET_STATUS: {
            network_status_t status;
            status.slave_count = ctx->ec_ctx.slave_count;
            status.network_active = ctx->ec_ctx.network_active;
            status.cycle_time_us = ctx->config.network.cycle_time_us;
            status.error_count = 0;
            
            uint8_t payload[8];
            protocol_pack_network_status(&status, payload);
            protocol_create_response(resp, STATUS_SUCCESS, ERR_NONE, payload, 8);
            break;
        }
        
        default:
            protocol_create_response(resp, STATUS_ERROR, ERR_INVALID_COMMAND, NULL, 0);
            return -1;
    }
    
    return 0;
}

static int handle_pdo_command(daemon_context_t *ctx, const udp_command_t *cmd, udp_response_t *resp) {
    if (!ctx->ec_ctx.network_active) {
        protocol_create_response(resp, STATUS_ERROR, ERR_NETWORK_NOT_READY, NULL, 0);
        return 0;
    }
    
    pdo_operation_t op;
    if (!protocol_extract_pdo_op(cmd, &op)) {
        protocol_create_response(resp, STATUS_ERROR, ERR_INVALID_PAYLOAD, NULL, 0);
        return 0;
    }
    
    switch (cmd->command_id) {
        case PDO_READ: {
            LOG_DEBUG("PDO read: slave=%u, offset=%u, size=%u", op.slave_id, op.offset, op.size);
            uint32_t value;
            int result = ethercat_read_pdo(&ctx->ec_ctx, op.slave_id, op.offset, op.size, &value);
            if (result == 0) {
                uint8_t payload[4];
                uint32_t *value_ptr = (uint32_t*)payload;
                *value_ptr = htonl(value);
                protocol_create_response(resp, STATUS_SUCCESS, ERR_NONE, payload, 4);
            } else {
                protocol_create_response(resp, STATUS_ERROR, ERR_SLAVE_NOT_FOUND, NULL, 0);
            }
            break;
        }
        
        case PDO_WRITE: {
            LOG_DEBUG("PDO write: slave=%u, offset=%u, size=%u, value=0x%08X", 
                     op.slave_id, op.offset, op.size, op.value);
            int result = ethercat_write_pdo(&ctx->ec_ctx, op.slave_id, op.offset, op.size, op.value);
            if (result == 0) {
                protocol_create_response(resp, STATUS_SUCCESS, ERR_NONE, NULL, 0);
            } else {
                protocol_create_response(resp, STATUS_ERROR, ERR_SLAVE_NOT_FOUND, NULL, 0);
            }
            break;
        }
        
        case PDO_MONITOR:
            LOG_INFO("PDO monitoring not yet implemented");
            protocol_create_response(resp, STATUS_ERROR, ERR_INVALID_COMMAND, NULL, 0);
            break;
            
        case PDO_STOP_MON:
            LOG_INFO("PDO monitor stop not yet implemented");
            protocol_create_response(resp, STATUS_ERROR, ERR_INVALID_COMMAND, NULL, 0);
            break;
            
        default:
            protocol_create_response(resp, STATUS_ERROR, ERR_INVALID_COMMAND, NULL, 0);
            return -1;
    }
    
    return 0;
}

static int handle_diagnostic_command(daemon_context_t *ctx, const udp_command_t *cmd, udp_response_t *resp) {
    switch (cmd->command_id) {
        case DIAG_NETWORK: {
            LOG_DEBUG("Network diagnostics requested");
            uint8_t payload[8] = {0};
            payload[0] = ctx->ec_ctx.network_active ? 1 : 0;
            payload[1] = (uint8_t)ctx->ec_ctx.slave_count;
            protocol_create_response(resp, STATUS_SUCCESS, ERR_NONE, payload, 8);
            break;
        }
        
        case DIAG_TIMING: {
            LOG_DEBUG("Timing diagnostics requested");
            timing_stats_t stats;
            ethercat_get_timing_stats(&stats);
            
            uint8_t payload[8];
            uint32_t *payload32 = (uint32_t*)payload;
            payload32[0] = htonl(stats.avg_cycle_us);
            payload32[1] = htonl(stats.jitter_us);
            protocol_create_response(resp, STATUS_SUCCESS, ERR_NONE, payload, 8);
            break;
        }
        
        case DIAG_ERRORS: {
            LOG_DEBUG("Error diagnostics requested");
            error_stats_t stats;
            ethercat_get_error_stats(&stats);
            
            uint8_t payload[8];
            uint32_t *payload32 = (uint32_t*)payload;
            payload32[0] = htonl(stats.frame_errors);
            payload32[1] = htonl(stats.timeout_errors);
            protocol_create_response(resp, STATUS_SUCCESS, ERR_NONE, payload, 8);
            break;
        }
        
        case DIAG_SLAVE: {
            LOG_DEBUG("Slave diagnostics requested");
            uint32_t slave_id = 0;
            if (ntohs(cmd->payload_len) >= 4) {
                slave_id = ntohl(*(uint32_t*)cmd->payload);
            }
            
            if (slave_id < ctx->ec_ctx.slave_count && ctx->ec_ctx.slaves[slave_id].online) {
                uint8_t payload[8] = {1}; 
                protocol_create_response(resp, STATUS_SUCCESS, ERR_NONE, payload, 8);
            } else {
                protocol_create_response(resp, STATUS_ERROR, ERR_SLAVE_NOT_FOUND, NULL, 0);
            }
            break;
        }
        
        default:
            protocol_create_response(resp, STATUS_ERROR, ERR_INVALID_COMMAND, NULL, 0);
            return -1;
    }
    
    return 0;
}

int handle_client_command(daemon_context_t *ctx, const udp_command_t *cmd,
                         udp_response_t *resp, struct sockaddr_in *client_addr) {
    (void)client_addr;
    
    if (!protocol_validate_command(cmd)) {
        LOG_WARN("Invalid command received");
        protocol_create_response(resp, STATUS_ERROR, ERR_INVALID_COMMAND, NULL, 0);
        return 0;
    }
    
    LOG_DEBUG("Command received: type=0x%02X, id=0x%02X, payload_len=%u",
              cmd->command_type, cmd->command_id, ntohs(cmd->payload_len));
    
    switch (cmd->command_type) {
        case CMD_CATEGORY_NETWORK:
            return handle_network_command(ctx, cmd, resp);
            
        case CMD_CATEGORY_PDO:
            return handle_pdo_command(ctx, cmd, resp);
            
        case CMD_CATEGORY_DIAGNOSTIC:
            return handle_diagnostic_command(ctx, cmd, resp);
            
        default:
            protocol_create_response(resp, STATUS_ERROR, ERR_INVALID_COMMAND, NULL, 0);
            return 0;
    }
}