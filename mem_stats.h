#ifndef MEM_STATS_H
#define MEM_STATS_H

#include "memcached.h"

void stats_init(void);
void stats_reset(const void *cookie);
void server_stats(ADD_STAT add_stats, conn *c, bool aggregate);
void process_stats_settings(ADD_STAT add_stats, void *c);
#ifdef ENABLE_ZK_INTEGRATION
void process_stats_zookeeper(ADD_STAT add_stats, void *c);
#endif
void update_stat_cas(conn *c, ENGINE_ERROR_CODE ret);
void disable_stats_detail(void);
#endif
