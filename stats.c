#include "config.h"
#include "memcached.h"
#ifdef ENABLE_ZK_INTEGRATION
#include "arcus_zk.h"
#include "arcus_hb.h"
#endif

#include <assert.h>
#include <string.h>

struct thread_stats *default_thread_stats;

/* Lock for global stats */
static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

void LOCK_STATS(void) {
    pthread_mutex_lock(&stats_lock);
}

void UNLOCK_STATS(void) {
    pthread_mutex_unlock(&stats_lock);
}

void append_stat(const char *name, ADD_STAT add_stats, conn *c,
                 const char *fmt, ...)
{
    char val_str[STAT_VAL_LEN];
    int vlen;
    va_list ap;

    assert(name);
    assert(add_stats);
    assert(c);
    assert(fmt);

    va_start(ap, fmt);
    vlen = vsnprintf(val_str, sizeof(val_str) - 1, fmt, ap);
    va_end(ap);

    add_stats(name, strlen(name), val_str, vlen, c);
}

static void disable_stats_detail(void)
{
    settings.detail_enabled = 0;
    mc_logger->log(EXTENSION_LOG_WARNING, NULL,
                   "Detailed stats internally disabled.\n");
}

void stats_init(void)
{
    mc_stats.daemon_conns = 0;
    mc_stats.rejected_conns = 0;
    mc_stats.quit_conns = 0;
    mc_stats.curr_conns = mc_stats.total_conns = mc_stats.conn_structs = 0;

    /* make the time we started always be 2 seconds before we really
       did, so time(0) - time.started is never zero.  if so, things
       like 'settings.oldest_live' which act as booleans as well as
       values are now false in boolean context... */
    stats_prefix_init(settings.prefix_delimiter, disable_stats_detail);
}

void stats_reset(const void *cookie)
{
    LOCK_STATS();
    mc_stats.rejected_conns = 0;
    mc_stats.quit_conns = 0;
    mc_stats.total_conns = 0;
    stats_prefix_clear();
    UNLOCK_STATS();
    threadlocal_stats_reset(default_thread_stats);
    mc_engine.v1->reset_stats(mc_engine.v0, cookie);
}

static void aggregate_callback(void *in, void *out)
{
    struct thread_stats *out_thread_stats = out;
    struct thread_stats *in_thread_stats = in;
    threadlocal_stats_aggregate(in_thread_stats, out_thread_stats);
}

/* return server specific stats only */
void server_stats(ADD_STAT add_stats, conn *c, bool aggregate)
{
    pid_t pid = getpid();
    rel_time_t now = get_current_time();

    struct thread_stats thread_stats;
    threadlocal_stats_clear(&thread_stats);

    if (aggregate && mc_engine.v1->aggregate_stats != NULL) {
        mc_engine.v1->aggregate_stats(mc_engine.v0, (const void *)c,
                                      aggregate_callback, &thread_stats);
    } else {
        threadlocal_stats_aggregate(default_thread_stats, &thread_stats);
    }

#ifndef __WIN32__
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
#endif

#ifdef ENABLE_ZK_INTEGRATION
    arcus_hb_stats hb_stats;
    arcus_hb_get_stats(&hb_stats);
#endif

    LOCK_STATS();

    APPEND_STAT("pid", "%lu", (long)pid);
    APPEND_STAT("uptime", "%u", now);
    APPEND_STAT("time", "%ld", now + (long)get_started_time());
    APPEND_STAT("version", "%s", VERSION);
    APPEND_STAT("libevent", "%s", event_get_version());
    APPEND_STAT("pointer_size", "%d", (int)(8 * sizeof(void *)));
#ifdef ENABLE_ZK_INTEGRATION
    APPEND_STAT("hb_count", "%"PRIu64, hb_stats.count);
    APPEND_STAT("hb_latency", "%"PRIu64, hb_stats.latency);
#endif

#ifndef __WIN32__
    append_stat("rusage_user", add_stats, c, "%ld.%06ld",
                (long)usage.ru_utime.tv_sec,
                (long)usage.ru_utime.tv_usec);
    append_stat("rusage_system", add_stats, c, "%ld.%06ld",
                (long)usage.ru_stime.tv_sec,
                (long)usage.ru_stime.tv_usec);
#endif

    APPEND_STAT("daemon_connections", "%u", mc_stats.daemon_conns);
    APPEND_STAT("curr_connections", "%u", mc_stats.curr_conns);
    APPEND_STAT("quit_connections", "%u", mc_stats.quit_conns);
    APPEND_STAT("reject_connections", "%u", mc_stats.rejected_conns);
    APPEND_STAT("total_connections", "%u", mc_stats.total_conns);
    APPEND_STAT("connection_structures", "%u", mc_stats.conn_structs);
    APPEND_STAT("cmd_get", "%"PRIu64, thread_stats.cmd_get);
    APPEND_STAT("cmd_set", "%"PRIu64, thread_stats.cmd_set);
    APPEND_STAT("cmd_incr", "%"PRIu64, thread_stats.cmd_incr);
    APPEND_STAT("cmd_decr", "%"PRIu64, thread_stats.cmd_decr);
    APPEND_STAT("cmd_delete", "%"PRIu64, thread_stats.cmd_delete);
    APPEND_STAT("cmd_cas", "%"PRIu64, thread_stats.cmd_cas);
    APPEND_STAT("cmd_flush", "%"PRIu64, thread_stats.cmd_flush);
    APPEND_STAT("cmd_flush_prefix", "%"PRIu64, thread_stats.cmd_flush_prefix);
    APPEND_STAT("cmd_auth", "%"PRIu64, thread_stats.cmd_auth);
    APPEND_STAT("cmd_lop_create", "%"PRIu64, thread_stats.cmd_lop_create);
    APPEND_STAT("cmd_lop_insert", "%"PRIu64, thread_stats.cmd_lop_insert);
    APPEND_STAT("cmd_lop_delete", "%"PRIu64, thread_stats.cmd_lop_delete);
    APPEND_STAT("cmd_lop_get", "%"PRIu64, thread_stats.cmd_lop_get);
    APPEND_STAT("cmd_sop_create", "%"PRIu64, thread_stats.cmd_sop_create);
    APPEND_STAT("cmd_sop_insert", "%"PRIu64, thread_stats.cmd_sop_insert);
    APPEND_STAT("cmd_sop_delete", "%"PRIu64, thread_stats.cmd_sop_delete);
    APPEND_STAT("cmd_sop_get", "%"PRIu64, thread_stats.cmd_sop_get);
    APPEND_STAT("cmd_sop_exist", "%"PRIu64, thread_stats.cmd_sop_exist);
    APPEND_STAT("cmd_mop_create", "%"PRIu64, thread_stats.cmd_mop_create);
    APPEND_STAT("cmd_mop_insert", "%"PRIu64, thread_stats.cmd_mop_insert);
    APPEND_STAT("cmd_mop_update", "%"PRIu64, thread_stats.cmd_mop_update);
    APPEND_STAT("cmd_mop_delete", "%"PRIu64, thread_stats.cmd_mop_delete);
    APPEND_STAT("cmd_mop_get", "%"PRIu64, thread_stats.cmd_mop_get);
    APPEND_STAT("cmd_bop_create", "%"PRIu64, thread_stats.cmd_bop_create);
    APPEND_STAT("cmd_bop_insert", "%"PRIu64, thread_stats.cmd_bop_insert);
    APPEND_STAT("cmd_bop_update", "%"PRIu64, thread_stats.cmd_bop_update);
    APPEND_STAT("cmd_bop_delete", "%"PRIu64, thread_stats.cmd_bop_delete);
    APPEND_STAT("cmd_bop_get", "%"PRIu64, thread_stats.cmd_bop_get);
    APPEND_STAT("cmd_bop_count", "%"PRIu64, thread_stats.cmd_bop_count);
    APPEND_STAT("cmd_bop_position", "%"PRIu64, thread_stats.cmd_bop_position);
    APPEND_STAT("cmd_bop_pwg", "%"PRIu64, thread_stats.cmd_bop_pwg);
    APPEND_STAT("cmd_bop_gbp", "%"PRIu64, thread_stats.cmd_bop_gbp);
#ifdef SUPPORT_BOP_MGET
    APPEND_STAT("cmd_bop_mget", "%"PRIu64, thread_stats.cmd_bop_mget);
#endif
#ifdef SUPPORT_BOP_SMGET
    APPEND_STAT("cmd_bop_smget", "%"PRIu64, thread_stats.cmd_bop_smget);
#endif
    APPEND_STAT("cmd_bop_incr", "%"PRIu64, thread_stats.cmd_bop_incr);
    APPEND_STAT("cmd_bop_decr", "%"PRIu64, thread_stats.cmd_bop_decr);
    APPEND_STAT("cmd_getattr", "%"PRIu64, thread_stats.cmd_getattr);
    APPEND_STAT("cmd_setattr", "%"PRIu64, thread_stats.cmd_setattr);
    APPEND_STAT("get_hits", "%"PRIu64, thread_stats.get_hits);
    APPEND_STAT("get_misses", "%"PRIu64, thread_stats.get_misses);
    APPEND_STAT("incr_hits", "%"PRIu64, thread_stats.incr_hits);
    APPEND_STAT("incr_misses", "%"PRIu64, thread_stats.incr_misses);
    APPEND_STAT("decr_hits", "%"PRIu64, thread_stats.decr_hits);
    APPEND_STAT("decr_misses", "%"PRIu64, thread_stats.decr_misses);
    APPEND_STAT("delete_hits", "%"PRIu64, thread_stats.delete_hits);
    APPEND_STAT("delete_misses", "%"PRIu64, thread_stats.delete_misses);
    APPEND_STAT("cas_hits", "%"PRIu64, thread_stats.cas_hits);
    APPEND_STAT("cas_badval", "%"PRIu64, thread_stats.cas_badval);
    APPEND_STAT("cas_misses", "%"PRIu64, thread_stats.cas_misses);
    APPEND_STAT("auth_errors", "%"PRIu64, thread_stats.auth_errors);
    APPEND_STAT("lop_create_oks", "%"PRIu64, thread_stats.lop_create_oks);
    APPEND_STAT("lop_insert_misses", "%"PRIu64, thread_stats.lop_insert_misses);
    APPEND_STAT("lop_insert_hits", "%"PRIu64, thread_stats.lop_insert_hits);
    APPEND_STAT("lop_delete_misses", "%"PRIu64, thread_stats.lop_delete_misses);
    APPEND_STAT("lop_delete_elem_hits", "%"PRIu64, thread_stats.lop_delete_elem_hits);
    APPEND_STAT("lop_delete_none_hits", "%"PRIu64, thread_stats.lop_delete_none_hits);
    APPEND_STAT("lop_get_misses", "%"PRIu64, thread_stats.lop_get_misses);
    APPEND_STAT("lop_get_elem_hits", "%"PRIu64, thread_stats.lop_get_elem_hits);
    APPEND_STAT("lop_get_none_hits", "%"PRIu64, thread_stats.lop_get_none_hits);
    APPEND_STAT("sop_create_oks", "%"PRIu64, thread_stats.sop_create_oks);
    APPEND_STAT("sop_insert_misses", "%"PRIu64, thread_stats.sop_insert_misses);
    APPEND_STAT("sop_insert_hits", "%"PRIu64, thread_stats.sop_insert_hits);
    APPEND_STAT("sop_delete_misses", "%"PRIu64, thread_stats.sop_delete_misses);
    APPEND_STAT("sop_delete_elem_hits", "%"PRIu64, thread_stats.sop_delete_elem_hits);
    APPEND_STAT("sop_delete_none_hits", "%"PRIu64, thread_stats.sop_delete_none_hits);
    APPEND_STAT("sop_get_misses", "%"PRIu64, thread_stats.sop_get_misses);
    APPEND_STAT("sop_get_elem_hits", "%"PRIu64, thread_stats.sop_get_elem_hits);
    APPEND_STAT("sop_get_none_hits", "%"PRIu64, thread_stats.sop_get_none_hits);
    APPEND_STAT("sop_exist_misses", "%"PRIu64, thread_stats.sop_exist_misses);
    APPEND_STAT("sop_exist_hits", "%"PRIu64, thread_stats.sop_exist_hits);
    APPEND_STAT("mop_create_oks", "%"PRIu64, thread_stats.mop_create_oks);
    APPEND_STAT("mop_insert_misses", "%"PRIu64, thread_stats.mop_insert_misses);
    APPEND_STAT("mop_insert_hits", "%"PRIu64, thread_stats.mop_insert_hits);
    APPEND_STAT("mop_update_misses", "%"PRIu64, thread_stats.mop_update_misses);
    APPEND_STAT("mop_update_elem_hits", "%"PRIu64, thread_stats.mop_update_elem_hits);
    APPEND_STAT("mop_update_none_hits", "%"PRIu64, thread_stats.mop_update_none_hits);
    APPEND_STAT("mop_delete_misses", "%"PRIu64, thread_stats.mop_delete_misses);
    APPEND_STAT("mop_delete_elem_hits", "%"PRIu64, thread_stats.mop_delete_elem_hits);
    APPEND_STAT("mop_delete_none_hits", "%"PRIu64, thread_stats.mop_delete_none_hits);
    APPEND_STAT("mop_get_misses", "%"PRIu64, thread_stats.mop_get_misses);
    APPEND_STAT("mop_get_elem_hits", "%"PRIu64, thread_stats.mop_get_elem_hits);
    APPEND_STAT("mop_get_none_hits", "%"PRIu64, thread_stats.mop_get_none_hits);
    APPEND_STAT("bop_create_oks", "%"PRIu64, thread_stats.bop_create_oks);
    APPEND_STAT("bop_insert_misses", "%"PRIu64, thread_stats.bop_insert_misses);
    APPEND_STAT("bop_insert_hits", "%"PRIu64, thread_stats.bop_insert_hits);
    APPEND_STAT("bop_update_misses", "%"PRIu64, thread_stats.bop_update_misses);
    APPEND_STAT("bop_update_elem_hits", "%"PRIu64, thread_stats.bop_update_elem_hits);
    APPEND_STAT("bop_update_none_hits", "%"PRIu64, thread_stats.bop_update_none_hits);
    APPEND_STAT("bop_delete_misses", "%"PRIu64, thread_stats.bop_delete_misses);
    APPEND_STAT("bop_delete_elem_hits", "%"PRIu64, thread_stats.bop_delete_elem_hits);
    APPEND_STAT("bop_delete_none_hits", "%"PRIu64, thread_stats.bop_delete_none_hits);
    APPEND_STAT("bop_get_misses", "%"PRIu64, thread_stats.bop_get_misses);
    APPEND_STAT("bop_get_elem_hits", "%"PRIu64, thread_stats.bop_get_elem_hits);
    APPEND_STAT("bop_get_none_hits", "%"PRIu64, thread_stats.bop_get_none_hits);
    APPEND_STAT("bop_count_misses", "%"PRIu64, thread_stats.bop_count_misses);
    APPEND_STAT("bop_count_hits", "%"PRIu64, thread_stats.bop_count_hits);
    APPEND_STAT("bop_position_misses", "%"PRIu64, thread_stats.bop_position_misses);
    APPEND_STAT("bop_position_elem_hits", "%"PRIu64, thread_stats.bop_position_elem_hits);
    APPEND_STAT("bop_position_none_hits", "%"PRIu64, thread_stats.bop_position_none_hits);
    APPEND_STAT("bop_pwg_misses", "%"PRIu64, thread_stats.bop_pwg_misses);
    APPEND_STAT("bop_pwg_elem_hits", "%"PRIu64, thread_stats.bop_pwg_elem_hits);
    APPEND_STAT("bop_pwg_none_hits", "%"PRIu64, thread_stats.bop_pwg_none_hits);
    APPEND_STAT("bop_gbp_misses", "%"PRIu64, thread_stats.bop_gbp_misses);
    APPEND_STAT("bop_gbp_elem_hits", "%"PRIu64, thread_stats.bop_gbp_elem_hits);
    APPEND_STAT("bop_gbp_none_hits", "%"PRIu64, thread_stats.bop_gbp_none_hits);
#ifdef SUPPORT_BOP_MGET
    APPEND_STAT("bop_mget_oks", "%"PRIu64, thread_stats.bop_mget_oks);
#endif
#ifdef SUPPORT_BOP_SMGET
    APPEND_STAT("bop_smget_oks", "%"PRIu64, thread_stats.bop_smget_oks);
#endif
    APPEND_STAT("bop_incr_elem_hits", "%"PRIu64, thread_stats.bop_incr_elem_hits);
    APPEND_STAT("bop_incr_none_hits", "%"PRIu64, thread_stats.bop_incr_none_hits);
    APPEND_STAT("bop_incr_misses", "%"PRIu64, thread_stats.bop_incr_misses);
    APPEND_STAT("bop_decr_elem_hits", "%"PRIu64, thread_stats.bop_decr_elem_hits);
    APPEND_STAT("bop_decr_none_hits", "%"PRIu64, thread_stats.bop_decr_none_hits);
    APPEND_STAT("bop_decr_misses", "%"PRIu64, thread_stats.bop_decr_misses);
    APPEND_STAT("getattr_misses", "%"PRIu64, thread_stats.getattr_misses);
    APPEND_STAT("getattr_hits", "%"PRIu64, thread_stats.getattr_hits);
    APPEND_STAT("setattr_misses", "%"PRIu64, thread_stats.setattr_misses);
    APPEND_STAT("setattr_hits", "%"PRIu64, thread_stats.setattr_hits);
    APPEND_STAT("stat_prefixes", "%"PRIu64, stats_prefix_count());
    APPEND_STAT("bytes_read", "%"PRIu64, thread_stats.bytes_read);
    APPEND_STAT("bytes_written", "%"PRIu64, thread_stats.bytes_written);
    APPEND_STAT("limit_maxbytes", "%"PRIu64, settings.maxbytes);
    APPEND_STAT("limit_maxconns", "%d", settings.maxconns);
    APPEND_STAT("threads", "%d", settings.num_threads);
    APPEND_STAT("conn_yields", "%"PRIu64, thread_stats.conn_yields);
    UNLOCK_STATS();
}

void process_stats_settings(ADD_STAT add_stats, void *c)
{
    assert(add_stats);
#ifdef ENABLE_ZK_INTEGRATION
    arcus_hb_confs hb_confs;
    arcus_hb_get_confs(&hb_confs);
#endif
    APPEND_STAT("maxbytes", "%llu", (unsigned long long)settings.maxbytes);
    APPEND_STAT("maxconns", "%d", settings.maxconns);
    APPEND_STAT("tcpport", "%d", settings.port);
    APPEND_STAT("udpport", "%d", settings.udpport);
    APPEND_STAT("sticky_limit", "%llu", (unsigned long long)settings.sticky_limit);
    APPEND_STAT("inter", "%s", settings.inter ? settings.inter : "NULL");
    APPEND_STAT("verbosity", "%d", settings.verbose);
    APPEND_STAT("oldest", "%lu", (unsigned long)settings.oldest_live);
    APPEND_STAT("evictions", "%s", settings.evict_to_free ? "on" : "off");
    APPEND_STAT("domain_socket", "%s",
                settings.socketpath ? settings.socketpath : "NULL");
    APPEND_STAT("umask", "%o", settings.access);
    APPEND_STAT("growth_factor", "%.2f", settings.factor);
    APPEND_STAT("chunk_size", "%d", settings.chunk_size);
    APPEND_STAT("num_threads", "%d", settings.num_threads);
    APPEND_STAT("stat_key_prefix", "%c", settings.prefix_delimiter);
    APPEND_STAT("detail_enabled", "%s",
                settings.detail_enabled ? "yes" : "no");
    APPEND_STAT("allow_detailed", "%s",
                settings.allow_detailed ? "yes" : "no");
    APPEND_STAT("reqs_per_event", "%d", settings.reqs_per_event);
    APPEND_STAT("cas_enabled", "%s", settings.use_cas ? "yes" : "no");
    APPEND_STAT("tcp_backlog", "%d", settings.backlog);
    APPEND_STAT("binding_protocol", "%s",
                prot_text(settings.binding_protocol));
#ifdef SASL_ENABLED
    APPEND_STAT("auth_enabled_sasl", "%s", "yes");
#else
    APPEND_STAT("auth_enabled_sasl", "%s", "no");
#endif

#ifdef ENABLE_ISASL
    APPEND_STAT("auth_sasl_engine", "%s", "isasl");
#elif defined(ENABLE_SASL)
    APPEND_STAT("auth_sasl_engine", "%s", "cyrus");
#else
    APPEND_STAT("auth_sasl_engine", "%s", "none");
#endif
    APPEND_STAT("auth_required_sasl", "%s", settings.require_sasl ? "yes" : "no");
    APPEND_STAT("item_size_max", "%llu", settings.item_size_max);
    APPEND_STAT("max_list_size", "%u", settings.max_list_size);
    APPEND_STAT("max_set_size", "%u", settings.max_set_size);
    APPEND_STAT("max_map_size", "%u", settings.max_map_size);
    APPEND_STAT("max_btree_size", "%u", settings.max_btree_size);
    APPEND_STAT("max_element_bytes", "%u", settings.max_element_bytes);
    APPEND_STAT("scrub_count", "%u", settings.scrub_count);
    APPEND_STAT("topkeys", "%d", settings.topkeys);
#ifdef ENABLE_ZK_INTEGRATION
    APPEND_STAT("hb_timeout", "%u", hb_confs.timeout);
    APPEND_STAT("hb_failstop", "%u", hb_confs.failstop);
#endif

    for (EXTENSION_DAEMON_DESCRIPTOR *ptr = settings.extensions.daemons;
         ptr != NULL;
         ptr = ptr->next) {
        APPEND_STAT("extension", "%s", ptr->get_name());
    }

    APPEND_STAT("logger", "%s", mc_logger->get_name());

    for (EXTENSION_ASCII_PROTOCOL_DESCRIPTOR *ptr = settings.extensions.ascii;
         ptr != NULL;
         ptr = ptr->next) {
        APPEND_STAT("ascii_extension", "%s", ptr->get_name(ptr->cookie));
    }
}

#ifdef ENABLE_ZK_INTEGRATION
void process_stats_zookeeper(ADD_STAT add_stats, void *c)
{
    assert(add_stats);
    arcus_zk_confs zk_confs;
    arcus_zk_stats zk_stats;
    arcus_zk_get_confs(&zk_confs);
    arcus_zk_get_stats(&zk_stats);

    APPEND_STAT("zk_libversion", "%s", zk_confs.zk_libversion);
    APPEND_STAT("zk_timeout", "%u", zk_confs.zk_timeout);
    APPEND_STAT("zk_failstop", "%s", zk_confs.zk_failstop ? "on" : "off");
    APPEND_STAT("zk_connected", "%s", zk_stats.zk_connected ? "true" : "false");
    APPEND_STAT("zk_ready", "%s", zk_stats.zk_ready ? "true" : "false");
#ifdef ENABLE_ZK_RECONFIG
    APPEND_STAT("zk_reconfig_needed", "%s", zk_stats.zk_reconfig_needed ? "on" : "off");
    if (zk_stats.zk_reconfig_needed) {
        APPEND_STAT("zk_reconfig_enabled", "%s", zk_stats.zk_reconfig_enabled ? "on" : "off");
        APPEND_STAT("zk_reconfig_version", "%" PRIx64, zk_stats.zk_reconfig_version);
    }
#endif
}
#endif

void update_stat_cas(conn *c, ENGINE_ERROR_CODE ret)
{
    switch (ret) {
    case ENGINE_SUCCESS:
        STATS_HITS(c, cas, c->hinfo.key, c->hinfo.nkey);
        break;
    case ENGINE_KEY_EEXISTS:
        STATS_BADVAL(c, cas, c->hinfo.key, c->hinfo.nkey);
        break;
    case ENGINE_KEY_ENOENT:
    case ENGINE_EBADTYPE:
        STATS_MISSES(c, cas, c->hinfo.key, c->hinfo.nkey);
        break;
    default:
        STATS_CMD_NOKEY(c, cas);
    }
}

