/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime Object Proxy.  Used for the NaCl file system
 * and as a generic object proxy mechanism.
 */

#include "native_client/src/include/portability.h"

#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/fs/obj_proxy.h"

#define PARANOID  1
/* #define DEBUG  1 */

/* private */
struct NaClObjProxyEntry {
  void    *obj;
  uint8_t name[1];  /* NB: actual allocated size is larger */
};


/*
 * The hash functor class for object->name mapping is the trivial
 * subclass of NaClHashFunctor: the virtual functions do not need
 * access to any instance data.
 */

static void NaClObjProxyFunctorDtor(struct NaClCmpFunctor *vself) {
  UNREFERENCED_PARAMETER(vself);
  return;
}


static int NaClObjProxyObjCmp(struct NaClCmpFunctor *vself,
                              void                  *vlhs,
                              void                  *vrhs) {
  struct NaClObjProxyEntry *lhs = (struct NaClObjProxyEntry *) vlhs;
  struct NaClObjProxyEntry *rhs = (struct NaClObjProxyEntry *) vrhs;

  UNREFERENCED_PARAMETER(vself);
  return lhs->obj != rhs->obj;
}


static uintptr_t NaClObjProxyObjHash(struct NaClHashFunctor *vself,
                                     void                   *ventry) {
  struct NaClObjProxyEntry *entry = (struct NaClObjProxyEntry *) ventry;

  UNREFERENCED_PARAMETER(vself);
  return (uintptr_t) entry->obj;
}


static struct NaClHashFunctorVtbl const kNaClObjProxyObjHashFunctorVtbl = {
  {
    NaClObjProxyFunctorDtor,  /* .base.Dtor */
    NaClObjProxyObjCmp,       /* .base.OrderCmp */
  },
  NaClObjProxyObjHash,        /* .Hash */
};


static int NaClObjProxyObjFunctorCtor(struct NaClHashFunctor  *self) {
  self->vtbl = &kNaClObjProxyObjHashFunctorVtbl;
  return 1;
}

/*
 * The hash functor for the name->object mapping requires instance
 * data: the virtual functions need to know the name length.
 */

struct NaClObjProxyNameFunctor {
  struct NaClHashFunctor  base;
  struct NaClObjProxy     *info;
};


static int NaClObjProxyNameCmp(struct NaClCmpFunctor *vself,
                               void *vlhs, void *vrhs) {
  struct NaClObjProxyNameFunctor *self
      = (struct NaClObjProxyNameFunctor *) vself;

  struct NaClObjProxyEntry *lhs = (struct NaClObjProxyEntry *) vlhs;
  struct NaClObjProxyEntry *rhs = (struct NaClObjProxyEntry *) vrhs;

  return memcmp(lhs->name, rhs->name, self->info->name_bytes);
}

static uintptr_t NaClObjProxyNameHash(struct NaClHashFunctor *vself,
                                      void *ventry) {
  struct NaClObjProxyNameFunctor *self
      = (struct NaClObjProxyNameFunctor *) vself;
  struct NaClObjProxyEntry *entry = (struct NaClObjProxyEntry *) ventry;

  uintptr_t h;
  int       i;

  for (i = self->info->name_bytes, h = 0; --i >= 0; ) {
    h = 17 * h + 3 * entry->name[i];  /* need better mixing? */
  }
  return h;
}


struct NaClHashFunctorVtbl const kNaClObjProxyNameHashFunctorVtbl = {
  {
    NaClObjProxyFunctorDtor,  /* .base.Dtor */
    NaClObjProxyNameCmp,      /* .base.OrderCmp */
  },
  NaClObjProxyNameHash,       /* .Hash */
};


static int NaClObjProxyNameFunctorCtor(struct NaClObjProxyNameFunctor  *self,
                                       struct NaClObjProxy             *info) {
  self->base.vtbl = &kNaClObjProxyNameHashFunctorVtbl;
  self->info = info;
  return 1;
}


int NaClObjProxyCtor(struct NaClObjProxy    *self,
                     struct NaClSecureRngIf *rng,
                     int                    name_bytes) {
  int                             rv = 0;
  struct NaClHashFunctor          *obj2name_functor = NULL;
  struct NaClObjProxyNameFunctor  *name2obj_functor = NULL;
  struct NaClContainerHashTbl     *o2n = NULL;
  struct NaClContainerHashTbl     *n2o = NULL;
  struct NaClObjProxyEntry        *key = NULL;

  self->obj_to_name = NULL;
  self->name_to_obj = NULL;
  self->obj_to_name_functor = NULL;
  self->name_to_obj_functor = NULL;

  if (!NaClMutexCtor(&self->mu)) {
    goto cleanup;
  }

  obj2name_functor = malloc(sizeof *obj2name_functor);
  if (NULL == obj2name_functor) {
    goto cleanup;
  }
  if (!NaClObjProxyObjFunctorCtor(obj2name_functor)) {
    goto cleanup;
  }
  self->obj_to_name_functor = (struct NaClHashFunctor *) obj2name_functor;
  obj2name_functor = NULL;

  name2obj_functor = malloc(sizeof *name2obj_functor);
  if (NULL == name2obj_functor) {
    goto cleanup;
  }
  if (!NaClObjProxyNameFunctorCtor(name2obj_functor, self)) {
    goto cleanup;
  }
  self->name_to_obj_functor = (struct NaClHashFunctor *) name2obj_functor;
  name2obj_functor = NULL;

  o2n = malloc(sizeof *o2n);
  if (NULL == o2n) {
    goto cleanup;
  }
  n2o = malloc(sizeof *n2o);
  if (NULL == n2o) {
    goto cleanup;
  }
  key = malloc(sizeof *key + name_bytes);
  if (!NaClContainerHashTblCtor(o2n, self->obj_to_name_functor, 257)) {
    goto cleanup;
  }
  self->obj_to_name = (struct NaClContainer *) o2n;
  o2n = NULL;
  if (!NaClContainerHashTblCtor(n2o, self->name_to_obj_functor, 257)) {
    goto cleanup;
  }
  self->name_to_obj = (struct NaClContainer *) n2o;
  n2o = NULL;
  self->name_bytes = name_bytes;
  self->key = key;
  self->rng = rng;
  rv = 1;
cleanup:
  if (!rv) {
    free(obj2name_functor);
    if (NULL != self->obj_to_name_functor) {
      ((*self->obj_to_name_functor->vtbl->base.Dtor)
       ((struct NaClCmpFunctor *) self->obj_to_name_functor));
      free(self->obj_to_name_functor);
      self->obj_to_name_functor = NULL;
    }
    free(name2obj_functor);
    if (NULL != self->name_to_obj_functor) {
      ((*self->name_to_obj_functor->vtbl->base.Dtor)
       ((struct NaClCmpFunctor *) self->name_to_obj_functor));
      self->name_to_obj_functor = NULL;
    }
    free(o2n);
    if (NULL != self->obj_to_name) {
      (*self->obj_to_name->vtbl->Dtor)(self->obj_to_name);
      free(self->obj_to_name);
      self->obj_to_name = NULL;
    }
    free(n2o);
    if (NULL != self->name_to_obj) {
      (*self->name_to_obj->vtbl->Dtor)(self->name_to_obj);
      free(self->name_to_obj);
      self->name_to_obj = NULL;
    }
    free(key);
    NaClMutexDtor(&self->mu);
  }
  return rv;
}


void NaClObjProxyDtor(struct NaClObjProxy *self) {
  ((*self->obj_to_name_functor->vtbl->base.Dtor)
   ((struct NaClCmpFunctor *) self->obj_to_name_functor));
  ((*self->name_to_obj_functor->vtbl->base.Dtor)
   ((struct NaClCmpFunctor *) self->name_to_obj_functor));
  (*self->name_to_obj->vtbl->Dtor)(self->name_to_obj);
  free(self->name_to_obj);
  (*self->obj_to_name->vtbl->Dtor)(self->obj_to_name);
  free(self->obj_to_name);
  free(self->key);
  NaClMutexDtor(&self->mu);
}


uint8_t const *NaClObjProxyFindNameByObj(struct NaClObjProxy *self,
                                         void                *obj) {
  struct NaClObjProxyEntry        *found = NULL;
  struct NaClContainerHashTblIter iter;
  uint8_t const                   *rv = NULL;

  NaClMutexLock(&self->mu);
  self->key->obj = obj;
  (*self->obj_to_name->vtbl->Find)(self->obj_to_name,
                                   self->key,
                                   (struct NaClContainerIter *) &iter);
  if (!(*iter.base.vtbl->AtEnd)((struct NaClContainerIter *) &iter)) {
    found = ((struct NaClObjProxyEntry *)
             (*iter.base.vtbl->Star)((struct NaClContainerIter *) &iter));
    rv = found->name;
  }
  NaClMutexUnlock(&self->mu);

  return rv;
}


int NaClObjProxyFindObjByName(struct NaClObjProxy *self,
                              uint8_t const       *name,
                              void                **out) {
  struct NaClObjProxyEntry        *found = NULL;
  struct NaClContainerHashTblIter iter;
  int                             rv = 0;

  NaClMutexLock(&self->mu);
  memcpy(self->key->name, name, self->name_bytes);
  (*self->name_to_obj->vtbl->Find)(self->name_to_obj,
                                   self->key,
                                   (struct NaClContainerIter *) &iter);
  if (!(*iter.base.vtbl->AtEnd)((struct NaClContainerIter *) &iter)) {
    found = ((struct NaClObjProxyEntry *)
             (*iter.base.vtbl->Star)((struct NaClContainerIter *) &iter));
    *out = found->obj;
#if DEBUG
    printf("ObjByName %d: %02x%02x%02x...\n", (int) found->obj,
           self->key->name[0], self->key->name[1], self->key->name[2]);
#endif
    rv = 1;
  }
  NaClMutexUnlock(&self->mu);
  return rv;
}


uint8_t const *NaClObjProxyInsert(struct NaClObjProxy *self, void *obj) {
  struct NaClObjProxyEntry  *entry = NULL;
  struct NaClObjProxyEntry  *entry2 = NULL;
  uint8_t const             *rv = NULL;
#if PARANOID
  void                      *out;

  if (NULL != NaClObjProxyFindNameByObj(self, obj)) {
    return rv;
  }
#endif

  entry = malloc(sizeof *entry + self->name_bytes);
  if (NULL == entry) {
    goto cleanup;
  }
  entry2 = malloc(sizeof *entry2 + self->name_bytes);
  if (NULL == entry2) {
    free(entry);
    goto cleanup;
  }
  entry->obj = obj;
#if PARANOID
  do {
#endif
    (*self->rng->vtbl->GenBytes)(self->rng,
                                 entry->name, self->name_bytes);
#if PARANOID
  } while (NaClObjProxyFindObjByName(self, entry->name, &out));
#endif
  memcpy(entry2, entry, (sizeof *entry) + self->name_bytes);
#if DEBUG
    printf("entry: %d: %02x%02x%02x...\n", (int) entry->obj,
           entry->name[0], entry->name[1], entry->name[2]);
    printf("entry2: %d: %02x%02x%02x...\n", (int) entry2->obj,
           entry2->name[0], entry2->name[1], entry2->name[2]);
#endif

  NaClMutexLock(&self->mu);
  (*self->obj_to_name->vtbl->Insert)(self->obj_to_name, entry);
  (*self->name_to_obj->vtbl->Insert)(self->name_to_obj, entry2);
  NaClMutexUnlock(&self->mu);

  rv = entry->name;
cleanup:
  return rv;
}


int NaClObjProxyRemove(struct NaClObjProxy *self, void *obj) {
  /*
   * must first find the entry so we know its randomly assigned name,
   * then delete from name_to_obj before deleting from obj_to_name.
   */
  struct NaClContainerHashTblIter o2niter;
  struct NaClObjProxyEntry        *entry;
  struct NaClContainerHashTblIter n2oiter;

  self->key->obj = obj;

  (*self->obj_to_name->vtbl->Find)(self->obj_to_name,
                                   self->key,
                                   (struct NaClContainerIter *) &o2niter);
  if ((*o2niter.base.vtbl->AtEnd)((struct NaClContainerIter *) &o2niter)) {
    return 0;  /* not found */
  }
  entry = ((struct NaClObjProxyEntry *)
           (*o2niter.base.vtbl->Star)((struct NaClContainerIter *) &o2niter));
  (*self->name_to_obj->vtbl->Find)(self->name_to_obj,
                                   entry,
                                   (struct NaClContainerIter *) &n2oiter);
  if ((*n2oiter.base.vtbl->AtEnd)((struct NaClContainerIter *) &n2oiter)) {
    NaClLog(LOG_FATAL, "object %08"NACL_PRIxPTR
            " found in o2n tbl, but not in n2o\n",
            (uintptr_t) obj);
    return 0;  /* internal error! */
  }
  (*o2niter.base.vtbl->Erase)((struct NaClContainerIter *) &o2niter);
  (*n2oiter.base.vtbl->Erase)((struct NaClContainerIter *) &n2oiter);

  return 1;
}
