/*
 * Copyright 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_GENERIC_CONTAINER_CONTAINER_HASH_TABLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_GENERIC_CONTAINER_CONTAINER_HASH_TABLE_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/generic_container/container.h"

EXTERN_C_BEGIN

struct NaClContainerHashTblEntry {
  int   flags;
  void  *datum;
};

#define NACL_CHTE_USED       (1<<0)
/* slot occupied, so keep probing */

#define NACL_CHTE_DELETED    (1<<1)
/* slot occupied but deleted, keep probing */

struct NaClContainerHashTbl {
  struct NaClContainer              base;
  struct NaClHashFunctor            *hash_functor;
  size_t                            num_buckets;
  size_t                            num_entries;
  struct NaClContainerHashTblEntry  *bucket;
};

int NaClContainerHashTblCtor(struct NaClContainerHashTbl  *self,
                             struct NaClHashFunctor       *hash_functor,
                             size_t                       num_buckets);

int NaClContainerHashTblInsert(struct NaClContainer *vself,
                               void                 *obj);

struct NaClContainerIter *NaClContainerHashTblFind(
    struct NaClContainer      *vself,
    void                      *key,
    struct NaClContainerIter  *out);

void NaClContainerHashTblDtor(struct NaClContainer *vself);

struct NaClContainerHashTblIter {
  struct NaClContainerIter    base;
  struct NaClContainerHashTbl *htbl;
  uintptr_t                   idx;
};

int NaClContainerHashTblIterCtor(struct NaClContainer     *vself,
                                 struct NaClContainerIter *viter);

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_GENERIC_CONTAINER_CONTAINER_HASH_TABLE_H_ */
