/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/generic_container/container_hash_table.h"

#include "native_client/src/include/portability.h"

static struct NaClContainerVtbl const  kNaClContainerHashTblVtbl = {
  NaClContainerHashTblInsert,
  NaClContainerHashTblFind,
  NaClContainerHashTblDtor,
  sizeof(struct NaClContainerHashTblIter),
  NaClContainerHashTblIterCtor,
};

#define HASH_TABLE_LOAD_FACTOR      2
#define HASH_TABLE_STRIDE           17
/*
 * The stride p is prime, so it is co-prime with any num_bucket value
 * (except for its own multiples) and thus linear probing by
 * increments of the stride will visit every bucket.  (I.e., the
 * stride value is a generator for the additive group Z_k^+, where
 * k=num_bucket.)  Assume that (k,p)=1.
 *
 * (p,+) is a generator is another way of saying that
 * 0,p,2p,3p,...,(k-1)p (mod k) is a permutation of 0,1,...,k-1.
 *
 * assume otherwise, so (p,+) generates a subgroup of Z_k^+.  let n be
 * the least integer greater than 0 where np=0 mod k, i.e., 0 < n < k.
 * since (p) doesn't generate Z_k^+, we have n < k.  then np=mk for
 * some integer m>0.  since p is prime and p \not| k but p divides mk,
 * p must divide m.  write m=pq.  then np = pqk, so n = qk.  =><=
 *
 * The co-primality of the stride and number of bucket is a
 * precondition for the insertion algorithm terminating.  (Another
 * precondition is that there is an unused slot.)
 *
 * We expand the hash table by doubling the number of buckets.  Thus,
 * if num_bucket starts off co-prime with the stride, it will remain
 * so.
 */
#define HASH_TABLE_MIN_BUCKETS      32
/*
 * must be greater than HASH_TABLE_STRIDE and not a multiple so that
 * they're co-prime.
 */

static struct NaClContainerHashTblEntry *NaClContainerHashTblMakeBucket(
    size_t size) {
  struct NaClContainerHashTblEntry    *bucket;

  if (SIZE_T_MAX / sizeof *bucket < size) {
    return NULL;
  }
  bucket = calloc(size, sizeof *bucket);
  return bucket;
}


/*
 * The functor_obj is the comparison and hash functor.  It is always
 * passed as the first arguments to cmp_fn and hash_fn to allow these
 * member functions to use its members to determine how to
 * compare/hash.  Arguably we should define a class with a vtable
 * containing two virtual functions, but we don't expect there to be
 * many object instances.
 */
int NaClContainerHashTblCtor(struct NaClContainerHashTbl  *self,
                             struct NaClHashFunctor       *hash_functor,
                             size_t                       num_buckets) {
  int success;

  self->base.vtbl = (struct NaClContainerVtbl *) NULL;

  /* silently increase number of buckets if too small */
  if (num_buckets < HASH_TABLE_MIN_BUCKETS) {
    num_buckets = HASH_TABLE_MIN_BUCKETS;
  }
  /* if a multiple of stride, silently increase it */
  if (0 == (num_buckets % HASH_TABLE_STRIDE)) {
    ++num_buckets;
  }

  self->hash_functor = hash_functor;
  self->num_buckets = num_buckets;
  self->num_entries = 0;
  self->bucket = NaClContainerHashTblMakeBucket(num_buckets);

  success = self->bucket != 0;

  if (success) {
    self->base.vtbl = &kNaClContainerHashTblVtbl;
  }
  return success;
}


int NaClContainerHashTblCheckLoad(struct NaClContainerHashTbl *self) {
  struct NaClContainerHashTblEntry  *entry;
  struct NaClContainerHashTblEntry  *new_table;
  struct NaClContainerHashTblEntry  *old_table;
  size_t                            new_size;
  size_t                            old_size;
  size_t                            i;

  /*
   * Check load factor.  If greater than THRESHOLD, double number of
   * buckets and rehash.
   */
  if (HASH_TABLE_LOAD_FACTOR * self->num_entries < self->num_buckets)
    return 1;

  old_size = self->num_buckets;
  new_size = 2 * old_size;
  /*
   * arithmetic overflow?!?  we normally would not have enough memory
   */
  if (new_size < old_size)
    return 0;

  new_table = NaClContainerHashTblMakeBucket(new_size);
  if (!new_table)
    return 0;

  old_table = self->bucket;
  self->bucket = new_table;

  self->num_buckets = new_size;
  self->num_entries = 0;

  /*
   * The rehash/transfer to the new, expanded table should not fail,
   * since the Insert method (below) does not fail.
   */
  for (i = 0; i < old_size; i++) {
    entry = &old_table[i];
    if (NACL_CHTE_USED != entry->flags) {
      continue;
    }
    (*self->base.vtbl->Insert)((struct NaClContainer *) self,
                               entry->datum);
  }

  free(old_table);
  return 1;
}


static uintptr_t HashNext(uintptr_t idx,
                          size_t    modulus) {
  idx += HASH_TABLE_STRIDE;
  while (idx >= modulus) {
    idx -= modulus;
  }
  return idx;
}


int NaClContainerHashTblInsert(struct NaClContainer *vself,
                               void                 *obj) {
  struct NaClContainerHashTbl *self = (struct NaClContainerHashTbl *) vself;
  uintptr_t                   idx;

  for (idx = (*self->hash_functor->vtbl->Hash)(self->hash_functor,
                                               obj) % self->num_buckets;
       NACL_CHTE_USED == self->bucket[idx].flags;
       idx = HashNext(idx, self->num_buckets)) {
  }
  /* unused or deleted */
  self->bucket[idx].datum = obj;
  self->bucket[idx].flags = NACL_CHTE_USED;
  ++self->num_entries;

  return NaClContainerHashTblCheckLoad(self);
}


/* fwd */
static struct NaClContainerIterVtbl const kNaClContainerHashTblIterVtbl;


struct NaClContainerIter *NaClContainerHashTblFind(
    struct NaClContainer      *vself,
    void                      *key,
    struct NaClContainerIter  *vout) {
  struct NaClContainerHashTbl     *self
      = (struct NaClContainerHashTbl *) vself;
  uintptr_t                       idx;
  struct NaClContainerHashTblIter *out
      = (struct NaClContainerHashTblIter *) vout;

  for (idx = (*self->hash_functor->vtbl->Hash)(self->hash_functor,
                                               key) % self->num_buckets;
       0 != self->bucket[idx].flags;
       idx = HashNext(idx, self->num_buckets)) {
    if (0 != (self->bucket[idx].flags & NACL_CHTE_USED)
        && ((*self->hash_functor->vtbl->base.OrderCmp)
            ((struct NaClCmpFunctor *) self->hash_functor,
             self->bucket[idx].datum, key)) == 0) {
      break;
    }
  }
  if (0 == self->bucket[idx].flags) {
    /* not found */
    idx = self->num_buckets;
  }

  out->htbl = self;
  out->idx = idx;

  vout->vtbl = &kNaClContainerHashTblIterVtbl;
  return vout;
}


void NaClContainerHashTblDtor(struct NaClContainer *vself) {
  struct NaClContainerHashTbl *self = (struct NaClContainerHashTbl *) vself;
  unsigned int                idx;

  for (idx = 0; idx < self->num_buckets; ++idx) {
    if (self->bucket[idx].flags & NACL_CHTE_USED) {
      free(self->bucket[idx].datum);
    }
  }
  free(self->bucket);
  self->bucket = 0;
}


int NaClContainerHashTblIterAtEnd(struct NaClContainerIter  *vself) {
  struct NaClContainerHashTblIter *self
      = (struct NaClContainerHashTblIter *) vself;

  return self->idx >= self->htbl->num_buckets;
}


void *NaClContainerHashTblIterStar(struct NaClContainerIter  *vself) {
  struct NaClContainerHashTblIter *self
      = (struct NaClContainerHashTblIter *) vself;

  return self->htbl->bucket[self->idx].datum;
}


void NaClContainerHashTblIterIncr(struct NaClContainerIter   *vself) {
  struct NaClContainerHashTblIter *self
      = (struct NaClContainerHashTblIter *) vself;

  while (++self->idx < self->htbl->num_buckets) {
    if (self->htbl->bucket[self->idx].flags == NACL_CHTE_USED)
      return;
  }
  /* self->idx == self->htbl->num_buckets imply AtEnd */
  return;
}


void NaClContainerHashTblIterErase(struct NaClContainerIter   *vself) {
  struct NaClContainerHashTblIter *self
      = (struct NaClContainerHashTblIter *) vself;

  if (self->htbl->bucket[self->idx].flags == NACL_CHTE_USED) {
    --self->htbl->num_entries;
  }
  self->htbl->bucket[self->idx].flags = NACL_CHTE_DELETED;
  free(self->htbl->bucket[self->idx].datum);
  self->htbl->bucket[self->idx].datum = 0;
  (*vself->vtbl->Incr)(vself);
}


void *NaClContainerHashTblIterExtract(struct NaClContainerIter   *vself) {
  struct NaClContainerHashTblIter *self
      = (struct NaClContainerHashTblIter *) vself;
  void                        *datum;

  if (self->htbl->bucket[self->idx].flags == NACL_CHTE_USED) {
    --self->htbl->num_entries;
  }
  self->htbl->bucket[self->idx].flags = NACL_CHTE_DELETED;
  datum = self->htbl->bucket[self->idx].datum;
  self->htbl->bucket[self->idx].datum = 0;
  (*vself->vtbl->Incr)(vself);

  return datum;
}


static struct NaClContainerIterVtbl const kNaClContainerHashTblIterVtbl = {
  NaClContainerHashTblIterAtEnd,
  NaClContainerHashTblIterStar,
  NaClContainerHashTblIterIncr,
  NaClContainerHashTblIterErase,
  NaClContainerHashTblIterExtract,
};


int NaClContainerHashTblIterCtor(struct NaClContainer     *vself,
                                 struct NaClContainerIter *viter) {
  struct NaClContainerHashTbl     *self
      = (struct NaClContainerHashTbl *) vself;
  struct NaClContainerHashTblIter *iter
      = (struct NaClContainerHashTblIter *) viter;

  viter->vtbl = &kNaClContainerHashTblIterVtbl;
  iter->htbl = self;
  iter->idx = 0;
  if (0 == (self->bucket[0].flags & NACL_CHTE_USED)) {
    NaClContainerHashTblIterIncr(viter);
  }
  return 1;
}
