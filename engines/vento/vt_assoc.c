/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * arcus-memcached - Arcus memory cache server
 * Copyright 2016 JaM2in Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "vento_engine.h"

static int MAX_BYTES_MEMTABLE = 64000000;
static int MAX_COUNT_MEMTABLE = 3;

static EXTENSION_LOGGER_DESCRIPTOR *logger;

static struct vt_assoc *active = NULL;
static struct vt_assoc *immutable = NULL;

ENGINE_ERROR_CODE vt_assoc_init(struct vento_engine *engine)
{
    srand((unsigned)time(NULL));
    struct vt_assoc *memtable = (struct vt_assoc *)malloc(sizeof(struct vt_assoc) * MAX_COUNT_MEMTABLE);
    if (memtable == NULL) return ENGINE_ENOMEM;

    logger = engine->server.log->get_logger();

    for (int i = 0; i < MAX_COUNT_MEMTABLE; i++) {
        memtable[i].list_level = 1;
        memtable[i].logs_count = 0;
        memtable[i].list_bytes = sizeof(slist_elem);
        memtable[i].tail = &memtable[i]->head;
        memtable[i].next = active;
        active = &memtable[i];
    }

    engine->assoc = memtable;

    logger->log(EXTENSION_LOG_INFO, NULL, "VENTO ASSOC module initialized.\n");
    return ENGINE_SUCCESS;
}

void vt_assoc_final(struct vento_engine *engine)
{

    /* later .. free slist item*/

    free(engine->assoc);
}

static log_item *do_assoc_find(slist_elem *curr, const char *key, const size_t nkey)
{
    log_item *it = NULL;

    while(curr->next != NULL) {
        if (memcmp(key, vt_item_get_key(curr->next->elem), nkey) > 0) {
            curr = curr->next;
        } else if (curr->up != NULL) {
            curr = curr->up;
        } else {
            break;
        }
    }

    if (curr->next != NULL) {
        curr = curr->next;
        if (memcmp(key, vt_item_get_key(curr->elem), nkey) == 0) {
            it = curr->elem;
        }
    }

    return it;
}

log_item *vt_assoc_find(struct vento_engine *engine, const char *key, const size_t nkey)
{
    log_item *it = NULL;

    it = do_assoc_find(active->tail, key, nkey);

    if (it == NULL) {
        struct vt_assoc *curr = immutable;
        while (curr != NULL) {
            it = do_assoc_find(curr->tail, key, nkey);
            if (it == NULL) {
                curr = curr->next;
            } else {
                break;
            }
        }
    }

    return it;
}

static int compare_key_with_seq(const log_item* first, const log_item* second) {
    int ret = memcmp(vt_item_get_key(first), vt_item_get_key(second), first->nkey);
    if (ret == 0) {
        ret = first->seqnum < second->seqnum ? 1 : -1;
    }
    return ret;
}

static void print_debug(void) {
    slist_elem *curr = active->tail;
    char buf[7];
    printf("<DEBUG total bytes = %zu\n", active->list_bytes);
    while(curr != NULL) {
        for(slist_elem *p = curr; p != NULL; p = p->next) {
            if (p->elem) {
                bool over = (p->elem->nkey + 1) > 7;
                int sz = over ? 7 : (p->elem->nkey + 1);
                snprintf(buf, sz, "%s", (char *)vt_item_get_key(p->elem));
                if (over) buf[6] = buf[5] = '.';
            } else {
                memset(buf, 0, sizeof(buf));
            }
            if (p->elem) printf("[ %s : %d ] ", buf, p->elem->seqnum);
            else printf("[ ] ");
        }
        printf("\n");
        curr = curr->up;
    }
}

static int do_assoc_insert(log_item* item, slist_elem *curr,
                           slist_elem **below, uint32_t *level) {
    slist_elem *add = NULL;
    int res = ENGINE_SUCCESS;

    if (curr != NULL) {
        while (curr->next != NULL &&
               compare_key_with_seq(item, curr->next->elem) > 0) {
            curr = curr->next;
        }
        res = do_assoc_insert(item, curr->up, &add, level);
    }

    if (res == ENGINE_SUCCESS) {
        if (curr == NULL || add != NULL) {
            if (!(rand() % (1 << *level))) {
                *below = (slist_elem *)malloc(sizeof(slist_elem));
                if (*below != NULL) {
                    (*below)->up = add;
                    (*below)->elem = item;
                    *level += 1;
                } else {
                    while (add != NULL) {
                        slist_elem *ptr = add;
                        add = add->up;
                        curr->next = ptr->next;
                        free(ptr);
                    }
                    return ENGINE_ENOMEM;
                }
            }
        }
        if (add != NULL) {
            add->next = curr->next;
            curr->next = add;
        }
    }

    print_debug();

    return res;
}

/* Note: this isn't an assoc_update.  The key must not already exist to call this */
int vt_assoc_insert(struct vento_engine *engine, log_item *it, size_t stotal)
{
    slist_elem *new_elem = NULL;
    uint32_t level = 0;
    int res = ENGINE_SUCCESS;

    if (active == NULL) return ENGINE_EWOULDBLOCK;

    it->seqnum = active->logs_count;

    res = do_assoc_insert(it, active->tail, &new_elem, &level);

    if (res == ENGINE_SUCCESS && level > active->list_level) {
        assert(new_elem != NULL);
        slist_elem *new_tail = (slist_elem *)malloc(sizeof(slist_elem));
        if (new_tail != NULL) {
            new_tail->up   = active->tail;
            new_tail->next = new_elem;
            active->tail = new_tail;
            active->list_level = level;
        } else {
            free(new_elem);
            res = ENGINE_ENOMEM;
        }
    }

    if (res == ENGINE_SUCCESS) {
        active->list_bytes += level * sizeof(slist_elem) + stotal;
        active->logs_count += 1;
        if (active->list_bytes >= MAX_BYTES_MEMTABLE) {
            struct vt_assoc *curr = active;
            curr->next = immutable;
            immutable = curr;
            active = active->next;
        }
    }

    return res;
}

void vt_assoc_delete(struct vento_engine *engine, const char *key, const size_t nkey)
{
    /*log_item *curr=NULL;
    log_item *prev=NULL;
    struct vt_assoc *assoc = &engine->assoc;
    uint32_t bucket = GET_HASH_BUCKET(hash, assoc->hashmask);

    while ((curr = assoc->hashtable[bucket]) != NULL) {
        if (nkey == curr->nkey && hash == curr->hval &&
            memcmp(key, vt_item_get_key(curr), nkey) == 0)
            break;
        prev = curr;
        curr = curr->h_next;
    }
    if (curr != NULL) {
        if (prev == NULL)
            assoc->hashtable[bucket] = curr->h_next;
        else
            prev->h_next = curr->h_next;
        assoc->log_items--;
    }*/
}
