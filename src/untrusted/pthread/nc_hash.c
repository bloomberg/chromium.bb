/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
