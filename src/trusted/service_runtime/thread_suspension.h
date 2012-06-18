/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_THREAD_SUSPENSION_H__
#define NATIVE_CLIENT_SERVICE_RUNTIME_THREAD_SUSPENSION_H__ 1

struct NaClAppThread;
struct NaClSignalContext;

/*
 * NaClUntrustedThreadSuspend() suspends a single thread.  It is not
 * valid to call this if the thread has already been suspended.
 */
void NaClUntrustedThreadSuspend(struct NaClAppThread *natp, int save_registers);

/*
 * NaClUntrustedThreadResume() resumes a single thread that was
 * previously suspended with NaClUntrustedThreadSuspend().
 */
void NaClUntrustedThreadResume(struct NaClAppThread *natp);

#if NACL_LINUX
void NaClSuspendSignalHandler(struct NaClSignalContext *regs);
#endif

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_THREAD_SUSPENSION_H__ */
