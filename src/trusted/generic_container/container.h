/*
 * Copyright 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Generic containers.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_GENERIC_CONTAINER_CONTAINER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_GENERIC_CONTAINER_CONTAINER_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClCmpFunctorVtbl;

struct NaClCmpFunctor {
  struct NaClCmpFunctorVtbl *vtbl;
};

/**
 * OrderCmp should return 0 if objects are equal (so a 3-way compare
 * function that might be used for a tree container may be re-used
 * here).
 */
struct NaClCmpFunctorVtbl {
  void (*Dtor)(struct NaClCmpFunctor    *vself);
  int (*OrderCmp)(struct NaClCmpFunctor *vself,
                  void                  *left,
                  void                  *right);
};

struct NaClHashFunctorVtbl;

struct NaClHashFunctor {
  struct NaClHashFunctorVtbl const  *vtbl;
};

struct NaClHashFunctorVtbl {
  struct NaClCmpFunctorVtbl base;
  uintptr_t                 (*Hash)(struct NaClHashFunctor  *vself,
                                    void                    *datum);
};

struct NaClContainer;  /* fwd */

struct NaClContainerIter;  /* fwd */

struct NaClContainerVtbl {
  /*
   * Insert passes ownership of obj to the container; obj must be
   * malloc'd.
   */
  int                   (*Insert)(struct NaClContainer    *vself,
                                  void                    *obj);
  /*
   * Find: if returned/constructed iterator is not AtEnd, can Star it
   * to access datum, then Erase the entry to free it.  Erase
   * implicitly increments the iterator.  The out parameter should be
   * a pointer to an object of the appropriate subclass of
   * ContainerIter to be constructed.  It can be malloc'd/aligned
   * memory of iter_size bytes in size; the iterator will be placement
   * new'd in the memory.  All iterator objects are POD, so the dtor
   * is a no-op.
   */
  struct NaClContainerIter  *(*Find)(struct NaClContainer     *vself,
                                     void                     *key,
                                     struct NaClContainerIter *out);
  void                  (*Dtor)(struct NaClContainer          *vself);
  size_t                iter_size;
  int                   (*IterCtor)(struct NaClContainer      *vself,
                                    struct NaClContainerIter  *iter);
};

struct NaClContainer {
  struct NaClContainerVtbl const  *vtbl;
};

struct NaClContainerIterVtbl {
  int     (*AtEnd)(struct NaClContainerIter     *vself);
  void    *(*Star)(struct NaClContainerIter     *vself);
  void    (*Incr)(struct NaClContainerIter      *vself);
  void    (*Erase)(struct NaClContainerIter     *vself);
  void    *(*Extract)(struct NaClContainerIter  *vself);
  /* like erase, but takes ownership back, so no automatic free */
};

struct NaClContainerIter {
  struct NaClContainerIterVtbl const  *vtbl;
};

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_GENERIC_CONTAINER_CONTAINER_H_ */
