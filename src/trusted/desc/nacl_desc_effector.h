/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_EFFECTOR_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_EFFECTOR_H_

/*
 * This file defines an interface class that the nacl_desc* routines
 * use to manipulate record keeping data structures (if any).  This
 * eliminates the need for the nrd_xfer library to directly manipulate
 * NaClApp or NaClAppThread contents, so trusted code that wish to use
 * the nrd_xfer library can provide their own NaClDescEffector
 * implementation to stub out, for example, recording of virtual
 * memory map changes, pre-mmap unmmaping of 64K allocations according
 * to the memory object backing the page, etc.
 */

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/include/portability.h"  /* uintptr_t, off_t, etc */

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescEffectorVtbl;

/* virtual base class; no ctor, no vtbl */
struct NaClDescEffector {
  struct NaClDescEffectorVtbl *vtbl;
};

/*
 * Virtual functions use the kernel return interface: non-negative
 * values are success codes, (small) negative values are negated
 * NACL_ABI_* errno returns.
 */

struct NaClDescEffectorVtbl {
  void (*Dtor)(struct NaClDescEffector *vself);

  /*
   * Return a constructed NaClDesc object to the calling environment
   * through the effector interface.  Takes ownership of the NaClDesc
   * object.
   *
   * For the service runtime, this insert the newly created NaClDesc
   * object into open-file table, returning index (descriptor).
   *
   * For trusted application code, each call returns a NaClDesc, but
   * in methods that may return more than one, dynamic cast style
   * checks are needed to determine which NaClDesc is which (assuming
   * that their types differ), or be determined by the order in which
   * they'red produced (which is fragile, since the implementation
   * might change the order in which the NaClDesc objects are
   * produced).  The returned indices are used for the return value
   * from the NaClDesc virtual function (or as output arguments) and
   * should not be negative; otherwise the value is immaterial.
   *
   * For example, NaClDescConnCapConnectAddr and NaClDescImcBoundDesc
   * uses this method since they are factories that produce
   * NaClDescImcDesc objects.
   */
  int (*ReturnCreatedDesc)(struct NaClDescEffector  *vself,
                           struct NaClDesc          *ndp);

  /*
   * Update addrss map for NaClApp.  The address is the system (flat
   * 32) address, and the virtual function is responsible for
   * translating it back to user (segment base relative) addresses.
   * The sys_prot used are host OS memory protection bits, though on
   * windows these are just a copy of *x values since there's no mmap
   * interface there.  backing_desc is the NaClDesc that is providing
   * backing store for the virtual memory.  offset_bytes is the offset
   * into the backing_desc and must be a multiple of allocation size.
   * the size of the mapping from the backing_desc is nbytes.
   *
   * NB: the calling code will ensure that offset_bytes + nbytes will
   * be at most NaClRoundPage(file size).  If NaClRoundAllocPage(file
   * size) is greater, then additional calls to put in padding pages
   * -- with a NULL backing_desc (backed by system paging file) will
   * be made for the difference.
   *
   * If delete_mem is non-zero, then the memory range specified by
   * [sysaddr,sysaddr+nbytes) should be removed from the address map,
   * and the other arguments are irrelevant.
   *
   * For trusted application code, this can be a no-op routine (a stub
   * that does nothing; all method function pointers must be
   * non-NULL).
   *
   * Note that because windows map things in 64K granularity and in
   * order to allow independent overlapping allocations, we map the
   * object in 64K chunks at a time.  Thus, it is critical that the
   * unmapping is done via the corresponding unmap function using the
   * descriptor object that was used to create the mapping in the
   * first place, so some recording keeping is still needed for
   * trusted application code.
   */
  void (*UpdateAddrMap)(struct NaClDescEffector *vself,
                        uintptr_t               sysaddr,  /* flat 32 addr */
                        size_t                  nbytes,
                        int                     sysprot,
                        struct NaClDesc         *backing_desc,
                        size_t                  backing_obj_size_bytes,
                        off_t                   offset_bytes,
                        int                     delete_mem);

  /*
   * For service runtime, the NaClDesc's Map virtual function will
   * call this to unmap any existing memory before mapping new pages
   * in on top.  This method should handle the necessary unmapping
   * (figure out the memory object that is backing the memory and call
   * UnmapViewOfFile or VirtualFree as appropriate).  On linux and
   * osx, this can be a no-op since mmap will override existing
   * mappings in an atomic fashion (yay!).  The sysaddr will be a
   * multiple of allocation size, as will be nbytes.
   *
   * For trusted applications, this can also be a no-op as long as the
   * application chooses a valid (not committed nor reserved) address
   * range.
   *
   * Note that UnmapMemory may be called without a corresponding
   * UpdateAddrmap (w/ delete_mem=1), since that may be deferred until
   * the memory hole has been populated with something else.
   *
   * This is NOT used by the NaClDesc's own Unmap method.
   */
  int (*UnmapMemory)(struct NaClDescEffector  *vself,
                     uintptr_t                sysaddr,
                     size_t                   nbytes);

  /*
   * Map anonymous memory -- from paging space -- into address space
   * at requested sysaddr of nbytes in length.  prot may be
   * NACL_ABI_PROT_NONE for address space squatting purposes.
   */
  uintptr_t (*MapAnonymousMemory)(struct NaClDescEffector *vself,
                                  uintptr_t               sysaddr,
                                  size_t                  nbytes,
                                  int                     prot);

  /*
   * The bound IMC socket to use as the sender when making a
   * connection.  Does not transfer ownership nor change refcount, so
   * returned value's lifetime should be lower-bounded by the lifetime
   * of the NaClDescEffector object.
   */
  struct NaClDescImcBoundDesc *(*SourceSock)(struct NaClDescEffector *vself);
};

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_EFFECTOR_H_
