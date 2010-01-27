/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * A hash table for phtreads library
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_NC_HASH_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_NC_HASH_H_ 1

#include <stdint.h>
#include "pthread.h"

/*
 * Note that this file is implementing templates by macros, as C doesn't
 * support templates.
 */
#define HASH_BUCKETS_NUMBER 50

typedef struct nc_hash_entry {
  LIST_ENTRY(nc_hash_entry) list_entries;
  /* a basic vtbl */
  uint32_t (*id_function)(struct nc_hash_entry *);
} nc_hash_entry_t;

LIST_HEAD(HashListHead, nc_hash_entry);

struct NaClHashTable {
  uint32_t number_of_buckets;
  struct HashListHead buckets[HASH_BUCKETS_NUMBER];
};

int __nacl_hash_init(struct NaClHashTable *table);

void __nacl_hash_destroy(struct NaClHashTable *table);

int __nacl_hash_insert_mu(struct NaClHashTable *table,
                          nc_hash_entry_t *entry);

nc_hash_entry_t *__nacl_hash_find_id_mu(struct NaClHashTable *table,
                                        uint32_t id);

int __nacl_hash_id_exists_mu(struct NaClHashTable *table,
                             uint32_t id);

nc_hash_entry_t *__nacl_hash_remove_mu(struct NaClHashTable  *table,
                                       uint32_t id);

#define HASH_INIT(table)  \
  __nacl_hash_init(table)

#define HASH_TABLE_INSERT(table, entry, hash_entry_name) \
  __nacl_hash_insert_mu(table, &entry->hash_entry_name)

#define HASH_ENTRY_TO_ENTRY_TYPE(hash_entry, entry_type, hash_entry_name) \
  ((hash_entry)?((struct entry_type *) \
  ((char*)hash_entry - offsetof(struct entry_type, hash_entry_name))): NULL)

#define HASH_FIND_ID(table, id, entry_type, hash_entry_name) \
  HASH_ENTRY_TO_ENTRY_TYPE(__nacl_hash_find_id_mu(table, id), \
                           entry_type, hash_entry_name)

#define HASH_ID_EXISTS(table, id) \
  __nacl_hash_id_exists_mu(table, id)

#define HASH_REMOVE(table, id, entry_type, hash_entry_name) \
  HASH_ENTRY_TO_ENTRY_TYPE(__nacl_hash_remove_mu(table, id), \
                           entry_type, hash_entry_name)

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_NC_HASH_H_ */
