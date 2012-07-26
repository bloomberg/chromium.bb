/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_THREAD_SUSPENSION_H__
#define NATIVE_CLIENT_SERVICE_RUNTIME_THREAD_SUSPENSION_H__ 1

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

struct NaClAppThread;
struct NaClSignalContext;

/*
 * NaClUntrustedThreadSuspend() suspends a single thread.  It is not
 * valid to call this if the thread has already been suspended.
 *
 * If the thread is currently executing a NaCl syscall, we tell the
 * thread not to return to untrusted code yet.  If the thread is
 * currently executing untrusted code, we suspend it.
 *
 * Note that if this is called from a NaCl syscall, natp may be the
 * current thread.  That is fine because this thread will be in the
 * NACL_APP_THREAD_TRUSTED state.
 */
void NaClUntrustedThreadSuspend(struct NaClAppThread *natp, int save_registers);

/*
 * NaClUntrustedThreadResume() resumes a single thread that was
 * previously suspended with NaClUntrustedThreadSuspend().
 */
void NaClUntrustedThreadResume(struct NaClAppThread *natp);

/*
 * NaClUntrustedThreadsSuspendAll() ensures that any untrusted code is
 * temporarily suspended.
 *
 * As with NaClUntrustedThreadSuspend(), if a thread is currently
 * executing a NaCl syscall, we tell the thread not to return to
 * untrusted code yet.  If a thread is currently executing untrusted
 * code, we suspend it.
 *
 * This returns with the lock threads_mu held, because we need to pin
 * the list of threads.  NaClUntrustedThreadsResume() must be called
 * to undo this.
 */
void NaClUntrustedThreadsSuspendAll(struct NaClApp *nap, int save_registers);

/*
 * Resumes all threads after they were suspended by
 * NaClUntrustedThreadsSuspendAll().
 */
void NaClUntrustedThreadsResumeAll(struct NaClApp *nap);

/*
 * NaClUntrustedThreadsSuspendAllButOne() is used by the debug stub to suspend
 * all untrusted threads except one that received a trap.
 */
void NaClUntrustedThreadsSuspendAllButOne(struct NaClApp *nap,
                                          struct NaClAppThread *natp_to_skip,
                                          int save_registers);

/*
 * Used by the debug stub to resume threads suspended by
 * NaClUntrustedThreadsSuspendAllButOne().
 */
void NaClUntrustedThreadsResumeAllButOne(struct NaClApp *nap,
                                         struct NaClAppThread *natp_to_skip);

/*
 * Get or modify the untrusted register state of a thread.  Calling
 * these functions is only valid if the thread is currently suspended
 * and was suspended with save_registers=1.
 */
void NaClAppThreadGetSuspendedRegisters(struct NaClAppThread *natp,
                                        struct NaClSignalContext *regs);
void NaClAppThreadSetSuspendedRegisters(struct NaClAppThread *natp,
                                        const struct NaClSignalContext *regs);

#if NACL_LINUX
void NaClSuspendSignalHandler(struct NaClSignalContext *regs);
#endif

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_THREAD_SUSPENSION_H__ */
