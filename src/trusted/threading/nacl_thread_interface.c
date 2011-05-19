/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/threading/nacl_thread_interface.h"

#include "native_client/src/include/nacl_compiler_annotations.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/nacl_base/nacl_refcount.h"

void WINAPI NaClThreadInterfaceStart(void *data) {
  struct NaClThreadInterface  *tif =
      (struct NaClThreadInterface *) data;
  void                        *thread_return;

  (*NACL_VTBL(NaClThreadInterface, tif)->LaunchCallback)(tif);
  thread_return = (*tif->fn_ptr)(tif);
  (*NACL_VTBL(NaClThreadInterface, tif)->Exit)(tif, thread_return);
  NaClLog(LOG_FATAL,
          "NaClThreadInterface: Exit member function did not exit thread\n");
}

int NaClThreadInterfaceThreadPlacementFactory(
    NaClThreadIfFactoryFunction   factory,
    void                          *factory_data,
    NaClThreadIfStartFunction     fn_ptr,
    void                          *thread_data,
    size_t                        thread_stack_size,
    struct NaClRefCountVtbl const *vtbl_ptr,
    struct NaClThreadInterface    *in_out_new_thread) {
  int rv;

  NaClLog(3, "Entered NaClThreadInterfaceThreadPlacementFactory\n");
  if (!NaClRefCountCtor((struct NaClRefCount *) in_out_new_thread)) {
    return 0;
  }

  in_out_new_thread->factory = factory;
  in_out_new_thread->factory_data = factory_data;
  in_out_new_thread->fn_ptr = fn_ptr;
  in_out_new_thread->thread_data = thread_data;
  NACL_VTBL(NaClRefCount, in_out_new_thread) = vtbl_ptr;
  rv = NaClThreadCtor(&in_out_new_thread->thread,
                      NaClThreadInterfaceStart,
                      in_out_new_thread,
                      thread_stack_size);
  if (!rv) {
    in_out_new_thread->fn_ptr = NULL;
    in_out_new_thread->thread_data = NULL;
    NACL_VTBL(NaClRefCount, in_out_new_thread) = &kNaClRefCountVtbl;
    (*NACL_VTBL(NaClRefCount, in_out_new_thread)->Dtor)(
        (struct NaClRefCount *) in_out_new_thread);
  }
  NaClLog(3,
          "Leaving NaClThreadInterfaceThreadPlacementFactory, returning %d\n",
          rv);
  return rv;
}

int NaClThreadInterfaceThreadFactory(
    void                        *factory_data,
    NaClThreadIfStartFunction   fn_ptr,
    void                        *thread_data,
    size_t                      thread_stack_size,
    struct NaClThreadInterface  **out_new_thread) {
  struct NaClThreadInterface  *new_thread;
  int                         rv;

  NaClLog(2, "Entered NaClThreadInterfaceThreadFactory\n");
  new_thread = malloc(sizeof *new_thread);
  if (NULL == new_thread) {
    NaClLog(2, "NaClThreadInterfaceThreadFactory: no memory!\n");
    return 0;
  }
  if (0 != (rv =
            NaClThreadInterfaceThreadPlacementFactory(
                NaClThreadInterfaceThreadFactory,
                factory_data,
                fn_ptr,
                thread_data,
                thread_stack_size,
                (struct NaClRefCountVtbl const *) &kNaClThreadInterfaceVtbl,
                new_thread))) {
    *out_new_thread = new_thread;
    NaClLog(2,
            "NaClThreadInterfaceThreadFactory: new thread 0x%"NACL_PRIxPTR"\n",
            (uintptr_t) new_thread);
    new_thread = NULL;
  }
  free(new_thread);
  NaClLog(2,
          "Leaving NaClThreadInterfaceThreadFactory, returning %d\n",
          rv);
  return rv;
}

void NaClThreadInterfaceDtor(struct NaClRefCount *vself) {
  struct NaClThreadInterface *self =
      (struct NaClThreadInterface *) vself;
  self->fn_ptr = NULL;
  self->thread_data = NULL;
  NACL_VTBL(NaClRefCount, self) = &kNaClRefCountVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

/*
 * Default LaunchCallback does nothing.  We could have made this "pure
 * virtual" by doing NaClLog(LOG_FATAL, ...) in the body (at least
 * detected at runtime).
 */
void NaClThreadInterfaceLaunchCallback(struct NaClThreadInterface *self) {
  NaClLog(3,
          "NaClThreadInterfaceLaunchCallback: thread 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self);
}

void NaClThreadInterfaceExit(struct NaClThreadInterface *self,
                             void                       *exit_code) {
  NaClLog(3,
          "NaClThreadInterfaceExit: thread 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self);
  NaClRefCountUnref((struct NaClRefCount *) self);
  /* only keep low order bits of the traditional void* return */
  NaClThreadExit((int) (uintptr_t) exit_code);
}

struct NaClThreadInterfaceVtbl const kNaClThreadInterfaceVtbl = {
  {
    NaClThreadInterfaceDtor,
  },
  NaClThreadInterfaceLaunchCallback,
  NaClThreadInterfaceExit,
};
