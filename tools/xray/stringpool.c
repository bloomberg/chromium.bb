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

// XRay string pool

/* string pool holds a large pile of strings */
/* it is up to higher level data structures to avoid duplicates */
/* it is up to higher level data structures to provide fast lookups */

#include <stdlib.h>
#include <string.h>
#include "xray_priv.h"


struct XRayStringPoolNode {
  struct XRayStringPoolNode *next;
  char strings[XRAY_STRING_POOL_NODE_SIZE];
};


struct XRayStringPool {
  struct XRayStringPoolNode *head;
  struct XRayStringPoolNode *current;
  int index;
};


static struct XRayStringPoolNode* XRayStringPoolAllocNode() {
  struct XRayStringPoolNode *s;
  s = (struct XRayStringPoolNode *)XRayMalloc(sizeof(*s));
  s->next = NULL;
  return s;
}


static int XRayStringPoolCurrentNodeSpaceAvail(struct XRayStringPool *pool) {
  int i = pool->index;
  return (XRAY_STRING_POOL_NODE_SIZE - i) - 1;
}


/* append a string to the string pool */
char* XRayStringPoolAppend(struct XRayStringPool *pool, const char *src) {
  /* add +1 to STRING_POOL_NODE_SIZE to detect large strings */
  /* add +1 to strnlen result to account for string termination */
  int n = strnlen(src, XRAY_STRING_POOL_NODE_SIZE + 1) + 1;
  int a = XRayStringPoolCurrentNodeSpaceAvail(pool);
  char *dst;
  /* don't accept strings larger than the pool node */
  if (n >= (XRAY_STRING_POOL_NODE_SIZE - 1))
    return NULL;
  /* if string doesn't fit, alloc a new node */
  if (n > a) {
    pool->current->next = XRayStringPoolAllocNode();
    pool->current = pool->current->next;
    pool->index = 0;
  }
  /* copy string and return a pointer to copy */
  dst = &pool->current->strings[pool->index];
  strcpy(dst, src);
  pool->index += n;
  return dst;
}


/* creates & initializes a string pool instance */
struct XRayStringPool* XRayStringPoolCreate() {
  struct XRayStringPool *pool;
  pool = (struct XRayStringPool *)XRayMalloc(sizeof(*pool));
  pool->head = XRayStringPoolAllocNode();
  pool->current = pool->head;
  return pool;
}


/* frees string pool */
void XRayStringPoolFree(struct XRayStringPool *pool) {
  struct XRayStringPoolNode *n = pool->head;
  while (NULL != n) {
    struct XRayStringPoolNode *c = n;
    n = n->next;
    XRayFree(c);
  }
  XRayFree(pool);
}
