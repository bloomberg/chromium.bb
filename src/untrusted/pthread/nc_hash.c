/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * A hash table for pthreads library
 */

#include <unistd.h>
#include "nc_hash.h"
#include "pthread.h"

static uint32_t hash_get_bucket_id(struct NaClHashTable *table,
                                   uint32_t id) {
  return id % table->number_of_buckets;
}

int __nacl_hash_init(struct NaClHashTable *table) {
  uint32_t i = 0;

  table->number_of_buckets = sizeof(table->buckets) / sizeof(table->buckets[0]);
  for (i = 0; i < table->number_of_buckets; ++i) {
    LIST_INIT(&table->buckets[i]);
  }
  return 1;
}

void __nacl_hash_destroy(struct NaClHashTable *table) {
  /*
   * Nothing to free - everything is static
   * TODO(gregoryd): what do we do when there
   * are still some entries in the table?
   */
}

int __nacl_hash_insert_mu(struct NaClHashTable *table,
                          nc_hash_entry_t *entry) {
  uint32_t bucket_id = hash_get_bucket_id(table, entry->id_function(entry));
  LIST_INSERT_HEAD(&table->buckets[bucket_id], entry, list_entries);
  return 0;
}

nc_hash_entry_t *__nacl_hash_find_id_mu(struct NaClHashTable *table,
                                        uint32_t id) {
  uint32_t bucket_id = hash_get_bucket_id(table, id);
  nc_hash_entry_t *iter;
  LIST_FOREACH(iter, &table->buckets[bucket_id], list_entries) {
    if (iter->id_function(iter) == id) {
      return iter;
    }
  }
  return NULL;
}

int __nacl_hash_id_exists_mu(struct NaClHashTable *table,
                             uint32_t id) {
  if (NULL == __nacl_hash_find_id_mu(table, id)) {
    return 0;
  }
  return 1;
}

nc_hash_entry_t *__nacl_hash_remove_mu(struct NaClHashTable  *table,
                                       uint32_t id) {
  uint32_t bucket_id = hash_get_bucket_id(table, id);
  nc_hash_entry_t *iter;
  LIST_FOREACH(iter, &table->buckets[bucket_id], list_entries) {
    if (iter->id_function(iter) == id) {
      LIST_REMOVE(iter, list_entries);
      return iter;
    }
  }
  return NULL;
}
