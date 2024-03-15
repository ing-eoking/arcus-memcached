#define STAT_KEY_LEN 128
#define STAT_VAL_LEN 128

/** Append a simple stat with a stat name, value format and value */
#define APPEND_STAT(name, fmt, val) \
    append_stat(name, add_stats, c, fmt, val);

/** Append an indexed stat with a stat name (with format), value format
    and value */
#define APPEND_NUM_FMT_STAT(name_fmt, num, name, fmt, val)          \
    klen = snprintf(key_str, STAT_KEY_LEN, name_fmt, num, name);    \
    vlen = snprintf(val_str, STAT_VAL_LEN, fmt, val);               \
    add_stats(key_str, klen, val_str, vlen, c);

/** Common APPEND_NUM_FMT_STAT format. */
#define APPEND_NUM_STAT(num, name, fmt, val) \
    APPEND_NUM_FMT_STAT("%d:%s", num, name, fmt, val)

/*
 * Macros for incrementing thread_stats
 */
#define MY_THREAD_STATS(c) (&default_thread_stats[(c)->thread->index])

#define STATS_CMD(c, op, key, nkey) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_ONE(my_thread_stats, cmd_##op); \
    TK(default_topkeys, cmd_##op, key, nkey, get_current_time()); \
}

#define STATS_OKS(c, op, key, nkey) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_TWO(my_thread_stats, op##_oks, cmd_##op); \
    TK(default_topkeys, op##_oks, key, nkey, get_current_time()); \
}

#define STATS_HITS(c, op, key, nkey) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_TWO(my_thread_stats, op##_hits, cmd_##op); \
    TK(default_topkeys, op##_hits, key, nkey, get_current_time()); \
}

#define STATS_ELEM_HITS(c, op, key, nkey) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_TWO(my_thread_stats, op##_elem_hits, cmd_##op); \
    TK(default_topkeys, op##_elem_hits, key, nkey, get_current_time()); \
}

#define STATS_NONE_HITS(c, op, key, nkey) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_TWO(my_thread_stats, op##_none_hits, cmd_##op); \
    TK(default_topkeys, op##_none_hits, key, nkey, get_current_time()); \
}

#define STATS_MISSES(c, op, key, nkey) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_TWO(my_thread_stats, op##_misses, cmd_##op); \
    TK(default_topkeys, op##_misses, key, nkey, get_current_time()); \
}

#define STATS_BADVAL(c, op, key, nkey) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_TWO(my_thread_stats, op##_badval, cmd_##op); \
    TK(default_topkeys, op##_badval, key, nkey, get_current_time()); \
}

#define STATS_CMD_NOKEY(c, op) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_ONE(my_thread_stats, cmd_##op); \
}

#define STATS_OKS_NOKEY(c, op) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_TWO(my_thread_stats, op##_oks, cmd_##op); \
}

#define STATS_ERRORS_NOKEY(c, op) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_TWO(my_thread_stats, op##_errors, cmd_##op); \
}

#define STATS_ADD(c, op, amt) { \
    struct thread_stats *my_thread_stats = MY_THREAD_STATS(c); \
    THREAD_STATS_INCR_AMT(my_thread_stats, op, amt); \
}

void LOCK_STATS(void);
void UNLOCK_STATS(void);

void append_stat(const char *name, ADD_STAT add_stats, conn *c,
                 const char *fmt, ...);

/* stats */
void stats_init(void);
void server_stats(ADD_STAT add_stats, conn *c, bool aggregate);
void process_stats_settings(ADD_STAT add_stats, void *c);
#ifdef ENABLE_ZK_INTEGRATION
void process_stats_zookeeper(ADD_STAT add_stats, void *c);
#endif
void update_stat_cas(conn *c, ENGINE_ERROR_CODE ret);
void stats_reset(const void *cookie);


