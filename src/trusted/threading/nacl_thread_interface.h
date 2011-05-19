/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_THREADING_NACL_THREAD_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_THREADING_NACL_THREAD_INTERFACE_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/nacl_base/nacl_refcount.h"

EXTERN_C_BEGIN

struct NaClThreadInterface;
struct NaClThreadInterfaceVtbl;

typedef void *(*NaClThreadIfStartFunction)(struct NaClThreadInterface *tdb);

/*
 * Thread Factory.  Returns bool; used to get more threads.  In
 * object-oriented terms, this would be a "static virtual" except C++
 * doesn't support such.  It is considered okay for this to fail,
 * e.g., when shutdown is already in progress.  This interface is
 * intended to permit the following uses:
 *
 * (1) Keeping track of the number of threads running.  This is used
 * in the simple_service library so that (especially reverse services)
 * a plugin shutdown can wait for service threads to shutdown prior to
 * releasing shared resources.
 *
 * (2) In the service runtime, ensure that the VM lock is held prior
 * to spawning a new thread (and allocating a thread stack as a side
 * effect).  The LaunchCallback method can signal the thread launcher
 * that the thread stack allocation is complete and that the VM lock
 * may be dropped.  This is important for Address Space Squatting in
 * Windows to cover the temporary VM hole that can be created due to
 * mmap-like operations, and serves to ensure that trusted thread
 * stacks don't get allocated inside the untrusted address space.
 */
typedef int (*NaClThreadIfFactoryFunction)(
    void                        *factory_data,
    NaClThreadIfStartFunction   fn_ptr,
    void                        *thread_data,
    size_t                      thread_stack_size,
    struct NaClThreadInterface  **out_new_thread);

struct NaClThreadInterface {
  struct NaClRefCount             base NACL_IS_REFCOUNT_SUBCLASS;
  NaClThreadIfFactoryFunction     factory;
  void                            *factory_data;
  struct NaClThread               thread;
  NaClThreadIfStartFunction       fn_ptr;
  void                            *thread_data;
};

struct NaClThreadInterfaceVtbl {
  struct NaClRefCountVtbl       vbase;

  /*
   * Will be invoked in new thread prior to fn_ptr.
   */
  void                          (*LaunchCallback)(
      struct NaClThreadInterface *self);

  /*
   * Performs cleanup -- unref the NaClThreadInterface object (which
   * may in turn invoke the Dtor) and then invoke NaClThreadExit
   * without accessing any instance members.  Should not return.
   */
  void                          (*Exit)(
      struct NaClThreadInterface  *self,
      void                        *thread_return);
};

extern struct NaClThreadInterfaceVtbl const kNaClThreadInterfaceVtbl;

/*
 * The PlacementFactory is intended for use by subclasses of
 * NaClThreadInterface that only need to initialize a few data members
 * etc that can be done *before* the base class factory runs.  The
 * reason that it cannot be used in the standard/conventional way is
 * that the thread is started before the PlacementFactory returns, so
 * unless there is additional synchronization, the subclass factory
 * cannot do further initialization after invoking PlacementFactory.
 * NB: the subclass can provide a chained fn_ptr that finishes the
 * subclass object initialization before calling the real user fn_ptr,
 * but that's a little ugly.
 */
int NaClThreadInterfaceThreadPlacementFactory(
    NaClThreadIfFactoryFunction   factory,
    void                          *factory_data,
    NaClThreadIfStartFunction     fn_ptr,
    void                          *thread_data,
    size_t                        thread_stack_size,
    struct NaClRefCountVtbl const *vtbl_ptr,
    struct NaClThreadInterface    *out_new_thread);

int NaClThreadInterfaceThreadFactory(
    void                        *factory_data,
    NaClThreadIfStartFunction   fn_ptr,
    void                        *thread_data,
    size_t                      thread_stack_size,
    struct NaClThreadInterface  **out_new_thread);

EXTERN_C_END

#endif
