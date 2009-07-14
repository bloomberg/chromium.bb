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

// Hashtable for XRay

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xray_priv.h"

#if defined(XRAY)

struct XRayHashTableEntry {
  void *data;
  uint32_t key;
};


struct XRayHashTable {
  int size;
  int count;
  struct XRayHashTableEntry *array;
};


XRAY_NO_INSTRUMENT void XRayHashTableGrow(struct XRayHashTable *table);
XRAY_NO_INSTRUMENT uint32_t XRayHashTableHashKey(uint32_t key);
XRAY_NO_INSTRUMENT void XRayHashTableInit(struct XRayHashTable *table,
  int32_t size);

#define HASH_HISTO 1024
int g_hash_histo[HASH_HISTO];


/* hashes a 32bit key into a 32bit value */
uint32_t XRayHashTableHashKey(uint32_t x) {
  uint32_t y = x * 7919;
  uint32_t z;
  size_t c;
  char *s = (char *)&y;
  /* based on djb2 hash function */
  uint32_t h = 5381;
  for (c = 0; c < sizeof(y); ++c) {
    z = s[c];
    h = ((h << 5) + h) + z;
  }
  return h;
}


int XRayHashTableGetSize(struct XRayHashTable *table) {
  return table->size;
}


/* looks up key in hashtable and returns blind data */
void* XRayHashTableLookup(struct XRayHashTable *table, uint32_t key) {
  uint32_t h = XRayHashTableHashKey(key);
  uint32_t m = table->size - 1;
  uint32_t j = h & m;
  uint32_t i;
  int z = 1;
  for (i = 0; i < m; ++i) {
    /* an empty entry means the {key, data} isn't in the table */
    if (NULL == table->array[j].data) {
      ++g_hash_histo[0];
      return NULL;
    }
    /* search for address */
    if (table->array[j].key == key) {
      if (z >= HASH_HISTO)
        z = HASH_HISTO - 1;
      ++g_hash_histo[z];
      return table->array[j].data;
    }
    j = (j + 1) & m;
    ++z;
  }
  /* table was full, and there wasn't a match */
  return NULL;
}


/* inserts key & data into hash table.  No duplicates. */
void* XRayHashTableInsert(struct XRayHashTable *table,
                          void *data, uint32_t key) {
  uint32_t h = XRayHashTableHashKey(key);
  uint32_t m = table->size - 1;
  uint32_t j = h & m;
  uint32_t i;
  for (i = 0; i < m; ++i) {
    /* take the first empty entry */
    /* (the key,data isn't already in the table) */
    if (NULL == table->array[j].data) {
      void *ret;
      float ratio;
      table->array[j].data = data;
      table->array[j].key = key;
      ++table->count;
      ret = data;
      ratio = (float)table->count / (float)table->size;
      /* double the size of the symtable if we've hit the ratio */
      if (ratio > XRAY_SYMBOL_TABLE_MAX_RATIO)
        XRayHashTableGrow(table);
      return ret;
    }
    /* if the key is already present, return the data in the table */
    if (table->array[j].key == key) {
      return table->array[j].data;
    }
    j = (j + 1) & m;
  }
  /* table was full */
  return NULL;
}


void* XRayHashTableAtIndex(struct XRayHashTable *table, int i) {
  if ((i < 0) || (i >= table->size))
    return NULL;
  return table->array[i].data;
}


/* grows the hash table by doubling its size */
/* re-inserts all the elements into the new table */
void XRayHashTableGrow(struct XRayHashTable *table) {
  struct XRayHashTableEntry *old_array = table->array;
  int old_size = table->size;
  int new_size = old_size * 2;
  int i;
  printf("XRay: Growing a hash table...\n");
  XRayHashTableInit(table, new_size);
  for (i = 0; i < old_size; ++i) {
    void *data = old_array[i].data;
    if (NULL != data) {
      uint32_t key = old_array[i].key;
      XRayHashTableInsert(table, data, key);
    }
  }
  XRayFree(old_array);
}


void XRayHashTableInit(struct XRayHashTable *table, int32_t size) {
  size_t bytes;
  if (0 != (size & (size - 1))) {
    printf("Xray: Hash table size should be a power of 2!\n");
    /* round size up to next power of 2 */
    /* see http://aggregate.org/MAGIC/  */
    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size++;
  }
  bytes = sizeof(table->array[0]) * size;
  table->size = size;
  table->count = 0;
  table->array = (struct XRayHashTableEntry *)XRayMalloc(bytes);
}


/* creates & inializes hash table */
struct XRayHashTable* XRayHashTableCreate(int size) {
  struct XRayHashTable *table;
  table = (struct XRayHashTable *)XRayMalloc(sizeof(*table));
  XRayHashTableInit(table, size);
  memset(&g_hash_histo[0], 0, sizeof(g_hash_histo[0]) * HASH_HISTO);
  return table;
}


/* prints hash table performance to file; for debugging */
void XRayHashTableHisto(FILE *f) {
  int i;
  for (i = 0; i < HASH_HISTO; ++i) {
    if (0 != g_hash_histo[i])
      fprintf(f, "hash_iterations[%d] = %d\n", i, g_hash_histo[i]);
  }
}


/* frees hash table */
/* note: does not free what the hash table entries point to */
void XRayHashTableFree(struct XRayHashTable *table) {
  XRayFree(table->array);
  table->size = 0;
  table->count = 0;
  table->array = NULL;
  XRayFree(table);
}

#endif  /* defined(XRAY) */
