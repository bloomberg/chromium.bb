/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Implementation of map from thread ids to thread handles.
 */

#include "native_client/src/trusted/service_runtime/win/thread_handle_map.h"

/*
 * Map from thread IDs to thread handles. Thread handles are owned by this map
 * and so are closed automatically on deletions and overwrites.
 * map->size - number of entries.
 * map->data - pointer to array of entries.
 * map->capacity - number of entries allocated.
 */

/*
 * Allocate map.
 */
ThreadHandleMap *CreateThreadHandleMap(void) {
  ThreadHandleMap *map = malloc(sizeof(ThreadHandleMap));
  if (map == NULL) {
    return NULL;
  }
  map->capacity = THREAD_HANDLE_MAP_INIT_SIZE;
  map->size = 0;
  map->data = malloc(sizeof(ThreadHandleMapEntry) *
                     THREAD_HANDLE_MAP_INIT_SIZE);
  if (map->data == NULL) {
    free(map);
    return NULL;
  }
  return map;
}

/*
 * Free map.
 */
void DestroyThreadHandleMap(ThreadHandleMap *map) {
  int i;
  for (i = 0; i < map->size; ++i) {
    CloseHandle(map->data[i].handle);
  }
  free(map->data);
  free(map);
}

/*
 * Allocate entry.
 * Returns the index of the new entry.
 * Map grows if necessarily.
 */
static int AddEntry(ThreadHandleMap *map) {
  ThreadHandleMapEntry* new_data;
  int new_capacity;
  map->size++;
  if (map->capacity<map->size) {
    new_capacity = 2 * map->size;
    new_data = realloc(map->data, sizeof(ThreadHandleMapEntry) * new_capacity);
    if (new_data == NULL) {
      return -1;
    }
    map->capacity = new_capacity;
    map->data = new_data;
  }
  return map->size - 1;
}

/*
 * Add entry to the free list.
 */
static void FreeEntry(ThreadHandleMap *map, int entry) {
  CloseHandle(map->data[entry].handle);
  map->data[entry] = map->data[map->size - 1];
  map->size--;
}

/*
 * Add mapping from thread_id to thread_handle to the map.
 */
int ThreadHandleMapPut(ThreadHandleMap *map,
                       DWORD thread_id,
                       HANDLE thread_handle) {
  int i;
  for (i = 0; i < map->size; ++i) {
    if (map->data[i].id == thread_id) {
      if (map->data[i].handle != thread_handle) {
        CloseHandle(map->data[i].handle);
      }
      map->data[i].handle = thread_handle;
      return 1;
    }
  }
  i = AddEntry(map);
  if (i < 0) {
    return 0;
  }
  map->data[i].id = thread_id;
  map->data[i].handle = thread_handle;
  return 1;
}

/*
 * Get thread handle for the thread id of INVALID_HANDLE_VALUE if
 * thread handle is not found.
 */
HANDLE ThreadHandleMapGet(ThreadHandleMap *map, DWORD thread_id) {
  int i;
  for (i = 0; i < map->size; ++i) {
    if (map->data[i].id == thread_id) {
      return map->data[i].handle;
    }
  }
  return INVALID_HANDLE_VALUE;
}

/*
 * Remove thread id from the map.
 */
void ThreadHandleMapDelete(ThreadHandleMap *map, DWORD thread_id) {
  int i;
  for (i = 0; i < map->size; ++i) {
    if (map->data[i].id == thread_id) {
      FreeEntry(map, i);
      return;
    }
  }
}