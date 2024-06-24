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
#ifndef VT_ASSOC_H
#define VT_ASSOC_H

typedef struct _slist_elem {
   struct _slist_elem *next;
   struct _slist_elem *up;
   log_item* elem;
} slist_elem;

struct vt_assoc {
   uint32_t list_level; /* skiplist level */
   uint32_t logs_count; /* logs count */
   size_t   list_bytes;

   struct vt_assoc *next;

   slist_elem  head;
   slist_elem* tail;
};

/* associative array */
ENGINE_ERROR_CODE vt_assoc_init(struct vento_engine *engine);
void              vt_assoc_final(struct vento_engine *engine);

log_item *       vt_assoc_find(struct vento_engine *engine, const char *key, const size_t nkey);
int               vt_assoc_insert(struct vento_engine *engine, log_item *it, size_t stotal);
void              vt_assoc_delete(struct vento_engine *engine, const char *key, const size_t nkey);
#endif
