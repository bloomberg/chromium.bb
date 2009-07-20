/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl Server Runtime user thread state.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_APP_THREAD_H__
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_APP_THREAD_H__ 1

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/platform/nacl_sync.h"
#include "native_client/src/trusted/platform/nacl_threads.h"

#include "native_client/src/trusted/service_runtime/nacl_bottom_half.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#if NACL_ARM
#include "native_client/src/trusted/service_runtime/arch/arm/sel_rt.h"
#else
#include "native_client/src/trusted/service_runtime/arch/x86/sel_rt.h"
#endif


EXTERN_C_BEGIN

struct NaClApp;

enum NaClThreadState {
  NACL_APP_THREAD_ALIVE,
  /* NACL_APP_THREAD_INTERRUPTIBLE_MUTEX, etc */
  NACL_APP_THREAD_SUICIDE_PENDING,
  NACL_APP_THREAD_DEAD
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

  int                       is_privileged;  /* can make "special" syscalls? */

  uint32_t                  refcount;

  struct NaClClosureResult  result;

  uint32_t                  sysret;

  /*
   * The NaCl app that contains this thread.  The app must exist as
   * long as a thread is still alive.
   */
  struct NaClApp            *nap;

  /*
   * The effector interface object used to manipulate NaCl apps by the
   * objects in the NaClDesc class hierarchy.  Used by this thread when
   * making NaClDesc method calls from syscall handlers.
   */
  struct NaClDescEffector   *effp;

  int                       thread_num;  /* index into nap->threads */

  struct NaClThread         thread;  /* low level thread representation */

  /*
   * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
   *
   * The locking behavior described below is not yet implemented.
   *
   * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
   *
   * When a thread invokes a service call from the NaCl application,
   * it must first grab its own lock (mu) before it executes any code
   * that can grab other service runtime locks/resources.  The thread
   * clears the holding_sr_locks flag as it is about to return from
   * the service call.  Short duration service calls can just hold the
   * lock throughout; potentially blocking service calls must drop the
   * thread lock and reacquire when it can unblock.  Condition
   * variables are used to allow the thread to wake up.  If a thread
   * is blocked waiting for I/O, a central epoll thread is responsible
   * for waking up the thread.  (The epoll thread serves the same
   * purpose as hardware interrupt handlers.)
   *
   * To summarily kill a thread from elsewhere, we check the
   * holding_sr_locks flag after acquiring the target thread's lock.
   * If it is clear, then the thread is running in the application
   * space (or at least it has not yet touched any service runtime
   * resources), and we can directly pthread_kill it before we release
   * the thread lock.  If it is set, we set the state flag to
   * NACL_APP_THREAD_SUICIDE_PENDING and release the thread lock,
   * possibly waking the blocked thread.
   *
   * When the thread is about to leave service runtime code and return
   * to the NaCl application, it should have released all locks to
   * service-runtime resources.  Next, the thread grabs its own lock
   * to clear the holding_sr_locks flag, at which point it examines
   * the suicide flag; if it finds that set, it should gracefully
   * exit.
   */
  int                       holding_sr_locks;

  /*
   * a thread cannot free up its own mutex lock and commit suicide,
   * since another thread may be trying to summarily kill it and is
   * waiting on the lock in order to ask it to commit suicide!
   * instead, the suiciding thread just marks itself as dead, and a
   * periodic thread grabs a global thread table lock to do thread
   * deletion (which the thread requesting summary execution must also
   * grab).
   */
  enum NaClThreadState      state;

  struct NaClThreadContext  user;
  /* sys is only used to hold segment registers */
  struct NaClThreadContext  sys;
  /*
   * NaClThread abstraction allows us to specify the stack size
   * (NACL_KERN_STACK_SIZE), but not its location.  The underlying
   * implementation takes care of finding memory for the thread stack,
   * and when the thread exits (they're not joinable), the stack
   * should be automatically released.
   */

  uint32_t                  *x_esp;
  /*
   * user's %esp translated to system address, used for accessing syscall
   * arguments
   */
};

int NaClAppThreadCtor(struct NaClAppThread  *natp,
                      struct NaClApp        *nap,
                      int                   is_privileged,
                      uintptr_t             entry,
                      uintptr_t             esp,
                      uint16_t              gs);

void NaClAppThreadDtor(struct NaClAppThread *natp);

/*
 * Low level initialization of thread, with validated values.  The
 * usr_entry and usr_esp values are directly used to initialize the
 * user register values; the sys_tdb_base is the system address for
 * allocating a %gs thread descriptor block base.  The caller is
 * responsible for error checking: usr_entry is a valid entry point (0
 * mod N) and sys_tdb_base is in the NaClApp's address space.
 */
int NaClAppThreadAllocSegCtor(struct NaClAppThread  *natp,
                              struct NaClApp        *nap,
                              int                   is_privileged,
                              uintptr_t             usr_entry,
                              uintptr_t             usr_esp,
                              uintptr_t             sys_tdb_base,
                              size_t                tdb_size);

int NaClAppThreadIncRef(struct NaClAppThread *natp);

int NaClAppThreadDecRef(struct NaClAppThread *natp);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_APP_THREAD_H__ */
