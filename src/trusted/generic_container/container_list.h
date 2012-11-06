/*
 * Copyright 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_GENERIC_CONTAINER_CONTAINER_LIST_H_
#define NATIVE_CLIENT_SRC_TRUSTED_GENERIC_CONTAINER_CONTAINER_LIST_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/generic_container/container.h"

EXTERN_C_BEGIN

struct NaClItemList {
  struct NaClItemList *next;
  void                *datum; /* dynamically allocated, pod */
};

struct NaClContainerList {
  struct NaClContainer  base;
  struct NaClCmpFunctor *cmp_functor;
  struct NaClItemList   *head;
};

int NaClContainerListCtor(struct NaClContainerList  *clp,
                          struct NaClCmpFunctor     *cmp_functor);

int NaClContainerListInsert(struct NaClContainer  *base_pointer,
                            void              *obj);

struct NaClContainerIter *NaClContainerListFind(
    struct NaClContainer      *base_pointer,
    void                      *key,
    struct NaClContainerIter  *out);

void NaClContainerListDtor(struct NaClContainer  *vself);

struct NaClContainerListIter {
  struct NaClContainerIter    base;
  struct NaClItemList         **cur;
};

int NaClContainerListIterCtor(struct NaClContainer      *vself,
                              struct NaClContainerIter  *viter);

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_GENERIC_CONTAINER_CONTAINER_LIST_H_ */
