#ifndef ETHERCAT_H
#define ETHERCAT_H

#include <stdint.h>
#include <stdbool.h>
#include "daemon.h"

#ifdef HAVE_SOEM
#include "ethercat.h"
#include "ethercattype.h"
#include "ethercatbase.h"
#include "ethercatmain.h"
#include "ethercatdc.h"
#include "ethercatcoe.h"
#include "ethercatfoe.h"
#include "ethercatsoe.h"
#include "ethercatconfig.h"
#include "ethercatprint.h"
#endif

typedef enum {
    EC_STATE_NONE = 0,
    EC_STATE_INIT = 1,
    EC_STATE_PREOP = 2,
    EC_STATE_SAFEOP = 4,
    EC_STATE_OP = 8
} ec_state_t;

typedef struct {
    uint32_t cycles_total;
    uint32_t cycles_missed;
    uint64_t total_time_us;
    uint32_t min_cycle_us;
    uint32_t max_cycle_us;
    uint32_t avg_cycle_us;
    uint32_t jitter_us;
} timing_stats_t;

typedef struct {
    uint32_t frame_errors;
    uint32_t lost_frames;
    uint32_t working_counter_errors;
    uint32_t slave_errors;
    uint32_t timeout_errors;
} error_stats_t;

int ethercat_init(ethercat_context_t *ctx, const char *interface);
int ethercat_start(ethercat_context_t *ctx);
int ethercat_stop(ethercat_context_t *ctx);
void ethercat_cleanup(ethercat_context_t *ctx);

int ethercat_scan_slaves(ethercat_context_t *ctx);
int ethercat_configure_slaves(ethercat_context_t *ctx);
int ethercat_set_state(ethercat_context_t *ctx, ec_state_t state);
ec_state_t ethercat_get_state(ethercat_context_t *ctx);

int ethercat_process_data(ethercat_context_t *ctx);
int ethercat_read_pdo(ethercat_context_t *ctx, uint32_t slave, uint32_t offset, 
                     uint32_t size, uint32_t *value);
int ethercat_write_pdo(ethercat_context_t *ctx, uint32_t slave, uint32_t offset,
                      uint32_t size, uint32_t value);

void ethercat_get_timing_stats(timing_stats_t *stats);
void ethercat_get_error_stats(error_stats_t *stats);
void ethercat_reset_stats(void);

#ifndef HAVE_SOEM
int ethercat_stub_init(ethercat_context_t *ctx, const char *interface);
int ethercat_stub_scan(ethercat_context_t *ctx);
#endif

#endif