#include "protocol.h"
#include <string.h>
#include <arpa/inet.h>

bool protocol_validate_command(const udp_command_t *cmd) {
    if (!cmd) return false;
    
    if (ntohl(cmd->magic) != PROTOCOL_MAGIC_CMD) {
        return false;
    }
    
    if (cmd->command_type < CMD_CATEGORY_NETWORK || cmd->command_type > CMD_CATEGORY_DIAGNOSTIC) {
        return false;
    }
    
    if (ntohs(cmd->payload_len) > PROTOCOL_MAX_PAYLOAD) {
        return false;
    }
    
    switch (cmd->command_type) {
        case CMD_CATEGORY_NETWORK:
            return (cmd->command_id >= NET_START && cmd->command_id <= NET_STATUS);
        case CMD_CATEGORY_PDO:
            return (cmd->command_id >= PDO_READ && cmd->command_id <= PDO_STOP_MON);
        case CMD_CATEGORY_DIAGNOSTIC:
            return (cmd->command_id >= DIAG_NETWORK && cmd->command_id <= DIAG_SLAVE);
        default:
            return false;
    }
}

void protocol_create_response(udp_response_t *resp, response_status_t status, 
                             error_code_t error, const void *data, uint16_t len) {
    if (!resp) return;
    
    memset(resp, 0, sizeof(udp_response_t));
    
    resp->magic = htonl(PROTOCOL_MAGIC_RESP);
    resp->status = status;
    resp->error_code = error;
    resp->payload_len = htons(len);
    
    if (data && len > 0 && len <= PROTOCOL_MAX_PAYLOAD) {
        memcpy(resp->payload, data, len);
    }
}

bool protocol_extract_pdo_op(const udp_command_t *cmd, pdo_operation_t *op) {
    if (!cmd || !op) return false;
    
    if (cmd->command_type != CMD_CATEGORY_PDO) return false;
    
    uint16_t payload_len = ntohs(cmd->payload_len);
    if (payload_len < 8) return false;
    
    const uint32_t *payload32 = (const uint32_t *)cmd->payload;
    op->slave_id = ntohl(payload32[0]);
    op->offset = ntohl(payload32[1]);
    
    if (cmd->command_id == PDO_WRITE && payload_len >= 12) {
        op->size = 4;
        op->value = ntohl(payload32[2]);
    } else if (cmd->command_id == PDO_READ && payload_len >= 12) {
        op->size = ntohl(payload32[2]);
        op->value = 0;
    } else {
        op->size = 1;
        op->value = 0;
    }
    
    return true;
}

void protocol_pack_network_status(const network_status_t *status, uint8_t *payload) {
    if (!status || !payload) return;
    
    uint32_t *payload32 = (uint32_t *)payload;
    payload32[0] = htonl(status->slave_count);
    payload32[1] = htonl(status->network_active ? 1 : 0);
}

void protocol_pack_pdo_response(uint32_t value, uint8_t *payload) {
    if (!payload) return;
    
    uint32_t *payload32 = (uint32_t *)payload;
    payload32[0] = htonl(value);
}