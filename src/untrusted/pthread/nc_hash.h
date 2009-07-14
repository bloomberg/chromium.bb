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
