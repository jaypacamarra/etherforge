#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_MAGIC_CMD      0xEF000001
#define PROTOCOL_MAGIC_RESP     0xEF800001
#define PROTOCOL_MAX_PAYLOAD    32
#define PROTOCOL_PORT           2346

typedef enum {
    CMD_CATEGORY_NETWORK = 0x01,
    CMD_CATEGORY_PDO = 0x02,
    CMD_CATEGORY_DIAGNOSTIC = 0x03
} command_category_t;

typedef enum {
    NET_START = 0x01,
    NET_STOP = 0x02,
    NET_SCAN = 0x03,
    NET_STATUS = 0x04
} network_command_t;

typedef enum {
    PDO_READ = 0x01,
    PDO_WRITE = 0x02,
    PDO_MONITOR = 0x03,
    PDO_STOP_MON = 0x04
} pdo_command_t;

typedef enum {
    DIAG_NETWORK = 0x01,
    DIAG_TIMING = 0x02,
    DIAG_ERRORS = 0x03,
    DIAG_SLAVE = 0x04
} diagnostic_command_t;

typedef enum {
    STATUS_SUCCESS = 0x00,
    STATUS_ERROR = 0x01
} response_status_t;

typedef enum {
    ERR_NONE = 0x00,
    ERR_INVALID_MAGIC = 0x01,
    ERR_INVALID_COMMAND = 0x02,
    ERR_INVALID_PAYLOAD = 0x03,
    ERR_NETWORK_NOT_READY = 0x04,
    ERR_SLAVE_NOT_FOUND = 0x05,
    ERR_TIMEOUT = 0x06,
    ERR_INTERNAL = 0xFF
} error_code_t;

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;
    uint8_t command_type;
    uint8_t command_id;
    uint16_t payload_len;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
} udp_command_t;

typedef struct {
    uint32_t magic;
    uint8_t status;
    uint8_t error_code;
    uint16_t payload_len;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
} udp_response_t;

#pragma pack(pop)

typedef struct {
    uint32_t slave_id;
    uint32_t offset;
    uint32_t size;
    uint32_t value;
} pdo_operation_t;

typedef struct {
    uint32_t slave_count;
    bool network_active;
    uint32_t cycle_time_us;
    uint32_t error_count;
} network_status_t;

bool protocol_validate_command(const udp_command_t *cmd);
void protocol_create_response(udp_response_t *resp, response_status_t status, 
                             error_code_t error, const void *data, uint16_t len);
bool protocol_extract_pdo_op(const udp_command_t *cmd, pdo_operation_t *op);
void protocol_pack_network_status(const network_status_t *status, uint8_t *payload);

#endif