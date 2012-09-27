/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Simple/secure ELF loader (NaCl SEL) memory map.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_SEL_MEM_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_SEL_MEM_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

/*
 * Interface is based on setting properties and query properties by
 * page numbers (addr >> NACL_PAGESHIFT) and the number of pages
 * affected (for setting properties).
 *
 * Initially, the address space is empty, with all memory
 * inaccessible.  As the program is loaded, pages are marked
 * accessible -- text pages are non-writable, data and stack are
 * writable.  At runtime, shared memory buffers are allocated by
 * looking at the first memory hole that fits, starting down from the
 * stack.
 *
 * The simple data structure that we use is a sorted array of valid
 * memory regions.
 */

/*
 * NaClVmmapEntryType exists for Windows support. NACL_VMMAP_ENTRY_ANONYMOUS
 * indicates anonymous memory mappings that were created with VirtualAlloc()
 * and must therefore be unmapped using VirtualFree(). NACL_VMMAP_ENTRY_MAPPED
 * indicates mappings that were created with MapViewOfFileEx() and must
 * therefore be unmapped using UnmapViewOfFile().
 */
enum NaClVmmapEntryType {
  NACL_VMMAP_ENTRY_ANONYMOUS = 1,
  NACL_VMMAP_ENTRY_MAPPED
};

struct NaClVmmapEntry {
  uintptr_t               page_num;   /* base virtual addr >> NACL_PAGESHIFT */
  size_t                  npages;     /* number of pages */
  int                     prot;       /* mprotect attribute */
  int                     max_prot;   /* maximum protection */
  enum NaClVmmapEntryType vmmap_type; /* memory entry type */
  int                     removed;    /* flag set in NaClVmmapUpdate */
};

struct NaClVmmap {
  struct NaClVmmapEntry **vmentry;       /* must not overlap */
  size_t                nvalid, size;
  int                   is_sorted;
};

void NaClVmmapDebug(struct NaClVmmap  *self,
                    char              *msg);

/*
 * iterators methods are final for now.
 */
struct NaClVmmapIter {
  struct NaClVmmap      *vmmap;
  size_t                 entry_ix;
};

int                   NaClVmmapIterAtEnd(struct NaClVmmapIter *nvip);
struct NaClVmmapEntry *NaClVmmapIterStar(struct NaClVmmapIter *nvip);
void                  NaClVmmapIterIncr(struct NaClVmmapIter *nvip);
void                  NaClVmmapIterErase(struct NaClVmmapIter *nvip);

int   NaClVmmapCtor(struct NaClVmmap  *self) NACL_WUR;

void  NaClVmmapDtor(struct NaClVmmap  *self);

/*
 * NaClVmmapAdd does not check whether the newly mapped region overlaps
 * with any existing ones. This function is intended for sandbox startup
 * only when non-overlapping mappings are being added.
 */
void  NaClVmmapAdd(struct NaClVmmap         *self,
                   uintptr_t                page_num,
                   size_t                   npages,
                   int                      prot,
                   int                      max_prot,
                   enum NaClVmmapEntryType  vmmap_type);

/*
 * NaClVmmapAddWithOverwrite checks the existing mappings and resizes
 * them if necessary to fit in the newly mapped region.
 */
void  NaClVmmapAddWithOverwrite(struct NaClVmmap          *self,
                                uintptr_t                 page_num,
                                size_t                    npages,
                                int                       prot,
                                int                       max_prot,
                                enum NaClVmmapEntryType   vmmap_type);

/*
 * NaClVmmapRemove modifies the specified region and updates the existing
 * mappings if necessary.
 */
void  NaClVmmapRemove(struct NaClVmmap          *self,
                      uintptr_t                 page_num,
                      size_t                    npages,
                      enum NaClVmmapEntryType   vmmap_type);

/*
 * NaClVmmapChangeProt updates the protection bits for the specified region.
 */
int NaClVmmapChangeProt(struct NaClVmmap  *self,
                        uintptr_t         page_num,
                        size_t            npages,
                        int               prot);

/*
 * NaClVmmapCheckMapping checks whether there is an existing mapping with
 * maximum protection equivalent or higher to the given one.
 */
int NaClVmmapCheckExistingMapping(struct NaClVmmap  *self,
                                  uintptr_t         page_num,
                                  size_t            npages,
                                  int               prot);

/*
 * NaClVmmapFindPage and NaClVmmapFindPageIter only works if pnum is
 * in the NaClVmmap.  If not, NULL and an AtEnd iterator is returned.
 */
struct NaClVmmapEntry const *NaClVmmapFindPage(struct NaClVmmap *self,
                                               uintptr_t        pnum);

struct NaClVmmapIter *NaClVmmapFindPageIter(struct NaClVmmap      *self,
                                            uintptr_t             pnum,
                                            struct NaClVmmapIter  *space);

/*
 * Visitor pattern, call fn on every entry.
 */
void  NaClVmmapVisit(struct NaClVmmap   *self,
                     void               (*fn)(void                  *state,
                                              struct NaClVmmapEntry *entry),
                     void               *state);

/*
 * Returns page number starting at which there is a hole of at least
 * num_pages in size.  Linear search from high addresses on down.
 */
uintptr_t NaClVmmapFindSpace(struct NaClVmmap *self,
                             size_t           num_pages);

/*
 * Just lke NaClVmmapFindSpace, except usage is intended for
 * NaClHostDescMap, so the starting address of the region found must
 * be NACL_MAP_PAGESIZE aligned.
 */
uintptr_t NaClVmmapFindMapSpace(struct NaClVmmap *self,
                                size_t           num_pages);

uintptr_t NaClVmmapFindMapSpaceAboveHint(struct NaClVmmap *self,
                                         uintptr_t        uaddr,
                                         size_t           num_pages);

void NaClVmmapMakeSorted(struct NaClVmmap  *self);

EXTERN_C_END

#endif
