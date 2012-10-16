/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/generic_container/container_list.h"

#include "native_client/src/include/portability.h"

static struct NaClContainerVtbl const  kNaClContainerListVtbl = {
  NaClContainerListInsert,
  NaClContainerListFind,
  NaClContainerListDtor,
  sizeof(struct NaClContainerListIter),
  NaClContainerListIterCtor,
};


/*
 * functor_obj is the comparison functor, and is the first argument to
 * cmp_fn as the "self" (or more commonly "this") argument.  arguably
 * we should create an explicit object with a vtable containing the
 * single comparison function pointer, but this is so simple that that
 * seems overkill.
 */
int NaClContainerListCtor(struct NaClContainerList  *self,
                          struct NaClCmpFunctor     *cmp_functor) {
  self->cmp_functor = cmp_functor;
  self->head = 0;

  self->base.vtbl = &kNaClContainerListVtbl;
  return 1;
}


int NaClContainerListIterAtEnd(struct NaClContainerIter *vself) {
  struct NaClContainerListIter  *self = (struct NaClContainerListIter *) vself;

  return *self->cur == 0;
}


void *NaClContainerListIterStar(struct NaClContainerIter *vself) {
  struct NaClContainerListIter  *self = (struct NaClContainerListIter *) vself;

  return (*self->cur)->datum;
}


void NaClContainerListIterIncr(struct NaClContainerIter  *vself) {
  struct NaClContainerListIter  *self = (struct NaClContainerListIter *) vself;

  self->cur = &(*self->cur)->next;
}


void NaClContainerListIterErase(struct NaClContainerIter  *vself) {
  struct NaClContainerListIter  *self = (struct NaClContainerListIter *) vself;
  struct NaClItemList           *doomed = *self->cur;
  struct NaClItemList           *next = (*self->cur)->next;

  *self->cur = next;
  free(doomed->datum);
  free(doomed);
}


void *NaClContainerListIterExtract(struct NaClContainerIter  *vself) {
  struct NaClContainerListIter  *self = (struct NaClContainerListIter *) vself;
  struct NaClItemList           *doomed = *self->cur;
  struct NaClItemList           *next = (*self->cur)->next;
  void                          *datum;

  *self->cur = next;
  datum = doomed->datum;
  free(doomed);
  return datum;
}

static struct NaClContainerIterVtbl const   kNaClContainerListIterVtbl = {
  NaClContainerListIterAtEnd,
  NaClContainerListIterStar,
  NaClContainerListIterIncr,
  NaClContainerListIterErase,
  NaClContainerListIterExtract,
};


int NaClContainerListInsert(struct NaClContainer  *vself,
                            void                  *obj) {
  struct NaClContainerList    *self = (struct NaClContainerList *) vself;
  struct NaClItemList         *new_entry = malloc(sizeof *new_entry);
  struct NaClItemList         **ipp;
  struct NaClItemList         *ip;

  if (!new_entry)
    return 0;

  new_entry->datum = obj;

  for (ipp = &self->head; 0 != (ip = *ipp); ipp = &ip->next) {
    if ((*self->cmp_functor->vtbl->OrderCmp)(self->cmp_functor,
                                             obj, ip->datum) >= 0) {
      break;
    }
  }

  new_entry->next = ip;
  *ipp = new_entry;
  return 1;
}


struct NaClContainerIter  *NaClContainerListFind(
    struct NaClContainer      *vself,
    void                      *key,
    struct NaClContainerIter  *vout) {
  struct NaClContainerList        *self = (struct NaClContainerList *) vself;
  struct NaClItemList             **ipp;
  struct NaClItemList             *ip;
  struct NaClContainerListIter    *out = (struct NaClContainerListIter *) vout;

  for (ipp = &self->head; (ip = *ipp) != 0; ipp = &ip->next) {
    if ((*self->cmp_functor->vtbl->OrderCmp)(self->cmp_functor,
                                             key, ip->datum) == 0) {
      break;
    }
  }
  out->cur = ipp;

  vout->vtbl = &kNaClContainerListIterVtbl;
  return vout;
}


void NaClContainerListDtor(struct NaClContainer  *vself) {
  struct NaClContainerList    *self = (struct NaClContainerList *) vself;
  struct NaClItemList         *cur, *next;

  for (cur = self->head; cur; cur = next) {
    next = cur->next;
    free(cur->datum);
    free(cur);
  }
}


int NaClContainerListIterCtor(struct NaClContainer      *vself,
                              struct NaClContainerIter  *viter) {
  struct NaClContainerList      *self = (struct NaClContainerList *) vself;
  struct NaClContainerListIter  *iter = (struct NaClContainerListIter *) viter;

  iter->cur = &self->head;

  viter->vtbl = &kNaClContainerListIterVtbl;
  return 1;
}
