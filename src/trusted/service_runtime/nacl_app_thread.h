/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Server Runtime user thread state.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_APP_THREAD_H__
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_APP_THREAD_H__ 1

#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"


EXTERN_C_BEGIN

struct NaClApp;

/*
 * The thread hosting the NaClAppThread may change suspend_state
 * between NACL_APP_THREAD_TRUSTED and NACL_APP_THREAD_UNTRUSTED using
 * NaClAppThreadSetSuspendState().
 *
 * Another thread may change this from:
 *   NACL_APP_THREAD_UNTRUSTED
 *   -> NACL_APP_THREAD_UNTRUSTED | NACL_APP_THREAD_SUSPENDING
 * or
 *   NACL_APP_THREAD_TRUSTED
 *   -> NACL_APP_THREAD_TRUSTED | NACL_APP_THREAD_SUSPENDING
 * and back again.
 */
enum NaClSuspendState {
  NACL_APP_THREAD_UNTRUSTED = 1,
  NACL_APP_THREAD_TRUSTED = 2,
  NACL_APP_THREAD_SUSPENDING = 4
};

/*
 * Generally, only the thread itself will need to manipulate this
 * structure, but occasionally we may need to blow away a thread for
 * some reason, and look inside.  While a thread is in the NaCl
 * application running untrusted code, the lock must *not* be held.
 */
struct NaClAppThread {
  struct NaClMutex          mu;
  struct NaClCondVar        cv;

  uint32_t                  sysret;

  /*
   * The NaCl app that contains this thread.  The app must exist as
   * long as a thread is still alive.
   */
  struct NaClApp            *nap;

  int                       thread_num;  /* index into nap->threads */
  /*
   * sys_tls and tls2 are TLS values used by user code and the
   * integrated runtime (IRT) respectively.  The first TLS area may be
   * accessed via the %gs segment register on x86-32 so must point
   * into untrusted address space; we store it as a system pointer.
   * The second TLS may be an arbitrary value.
   */
  uintptr_t                 sys_tls;  /* first saved TLS ptr */
  uint32_t                  *usr_tlsp;
  uint32_t                  tls2;     /* second saved TLS value */

  struct NaClThread         thread;  /* low level thread representation */

  /*
   * Thread suspension is currently only needed and implemented for
   * Windows, for preventing race conditions when opening up holes in
   * untrusted address space during mmap/munmap.
   */
#if NACL_WINDOWS
  enum NaClSuspendState     suspend_state;
#endif

  struct NaClThreadContext  user;
  struct NaClThreadContext  sys;
  /*
   * NaClThread abstraction allows us to specify the stack size
   * (NACL_KERN_STACK_SIZE), but not its location.  The underlying
   * implementation takes care of finding memory for the thread stack,
   * and when the thread exits (they're not joinable), the stack
   * should be automatically released.
   */

  uintptr_t                 usr_syscall_args;
  /*
   * user's syscall argument address, relative to untrusted user
   * address.  the syscall arglist is on the untrusted stack.
   */

  /* Stack for signal handling, registered with sigaltstack(). */
  void                      *signal_stack;

  /*
   * exception_stack is the address of the top of the untrusted
   * exception handler stack, or 0 if no such stack is registered for
   * this thread.
   */
  uint32_t                  exception_stack;
  /*
   * exception_flag is a boolean.  When it is 1, untrusted exception
   * handling is disabled for this thread.  It is set to 1 when the
   * exception handler is called, in order to prevent the exception
   * handler from being re-entered.
   */
  uint32_t                  exception_flag;

  /*
   * The last generation this thread reported into the service runtime
   * Protected by mu
   */
  int                       dynamic_delete_generation;
};

void NaClAppThreadTeardown(struct NaClAppThread *natp);

/*
 * Low level initialization of thread, with validated values.  The
 * usr_entry and usr_stack_ptr values are directly used to initialize the
 * user register values; the sys_tls_base is the system address for
 * allocating a %gs thread descriptor block base.  The caller is
 * responsible for error checking: usr_entry is a valid entry point (0
 * mod N) and sys_tls_base is in the NaClApp's address space.
 */
int NaClAppThreadCtor(struct NaClAppThread  *natp,
                      struct NaClApp        *nap,
                      uintptr_t             usr_entry,
                      uintptr_t             usr_stack_ptr,
                      uintptr_t             sys_tls,
                      uint32_t              usr_tls2) NACL_WUR;

void NaClAppThreadDtor(struct NaClAppThread *natp);

/*
 * This function can be called from the thread hosting the
 * NaClAppThread.  It can be used to switch between the states
 * NACL_APP_THREAD_TRUSTED and NACL_APP_THREAD_UNTRUSTED.
 */
void NaClAppThreadSetSuspendState(struct NaClAppThread *natp,
                                  enum NaClSuspendState old_state,
                                  enum NaClSuspendState new_state);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_APP_THREAD_H__ */
