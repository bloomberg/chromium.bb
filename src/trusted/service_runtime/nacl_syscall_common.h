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
 * NaCl service run-time, non-platform specific system call helper routines.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SYSCALL_COMMON_H__
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SYSCALL_COMMON_H__ 1

#include "native_client/src/include/portability.h"

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/service_runtime/include/sys/time.h"

#if defined(HAVE_SDL)
#include "native_client/src/trusted/service_runtime/include/sys/audio_video.h"
#endif

EXTERN_C_BEGIN

struct NaClApp;
struct NaClAppThread;
struct NaClSocketAddress;
struct NaClDesc;
struct NaClImcMsgHdr;
struct nacl_abi_stat;

int32_t NaClSetBreak(struct NaClAppThread *natp,
                     uintptr_t            new_break);

/*
 * Entering kernel mode from user mde.  Makes thread unkillable
 * because while switched to kernel mode, we may be holding kernel
 * locks and in the middle of mutating protected objects, and killing
 * the thread will leave the service runtime in an inconsistent state.
 * Must invoke the correspondng Leave function, except if the thread
 * is exiting anyway.
 *
 * Not all syscalls need to invoke the Enter function -- those that we
 * know definitely does not grab any locks (even implicitly, in libc
 * routines such as malloc).
 */
void NaClSysCommonThreadSyscallEnter(struct NaClAppThread *natp);

/*
 * Thread is leaving kernel mode and returning to user mode.  Checks
 * if a kill request was made, and if so commit suicide.  Must be
 * invoked if there was a corresponding Enter; not harmful if called
 * without a corresponding Enter.
 */
void NaClSysCommonThreadSyscallLeave(struct NaClAppThread *natp);

int NaClHighResolutionTimerEnabled(void);

int32_t NaClOpenAclCheck(struct NaClApp *nap,
                         char const     *path,
                         int            flags,
                         int            mode);

int32_t NaClStatAclCheck(struct NaClApp *nap,
                         char const     *path);

int32_t NaClCommonSysExit(struct NaClAppThread  *natp,
                          int                   status);

int32_t NaClCommonSysThreadExit(struct NaClAppThread  *natp,
                                int32_t               *stack_flag);

void NaClInsecurelyBypassAllAclChecks(void);

int32_t NaClCommonSysOpen(struct NaClAppThread  *natp,
                          char                  *pathname,
                          int                   flags,
                          int                   mode);

int32_t NaClCommonSysClose(struct NaClAppThread *natp,
                           int                  d);

int32_t NaClCommonSysRead(struct NaClAppThread  *natp,
                          int                   d,
                          void                  *buf,
                          size_t                count);

int32_t NaClCommonSysWrite(struct NaClAppThread *natp,
                           int                  d,
                           void                 *buf,
                           size_t               count);

int32_t NaClCommonSysLseek(struct NaClAppThread *natp,
                           int                  d,
                           off_t                offset,
                           int                  whence);

int32_t NaClCommonSysIoctl(struct NaClAppThread *natp,
                           int                  d,
                           int                  request,
                           void                 *arg);

int32_t NaClCommonSysFstat(struct NaClAppThread *natp,
                           int                  d,
                           struct nacl_abi_stat *nasp);

int32_t NaClCommonSysStat(struct NaClAppThread *natp,
                          const char           *path,
                          struct nacl_abi_stat *nasp);

void NaClCommonUtilUpdateAddrMap(struct NaClAppThread *natp,
                                 uintptr_t            sysaddr,
                                 size_t               nbytes,
                                 int                  sysprot,
                                 struct NaClDesc      *backing_desc,
                                 size_t               backing_bytes,
                                 off_t                offset_bytes,
                                 int                  delete_mem);

/* bool */
int NaClSysCommonAddrRangeContainsExecutablePages_mu(struct NaClApp *nap,
                                                     uintptr_t      usraddr,
                                                     size_t         length);

int32_t NaClCommonSysMmap(struct NaClAppThread  *natp,
                          void                  *start,
                          size_t                length,
                          int                   prot,
                          int                   flags,
                          int                   d,
                          nacl_abi_off_t        offset);

int32_t NaClSysMmap(struct NaClAppThread  *natp,
                    void                  *start,
                    size_t                length,
                    int                   prot,
                    int                   flags,
                    int                   d,
                    nacl_abi_off_t        offset);

int32_t NaClSysMunmap(struct NaClAppThread  *natp,
                      void                  *start,
                      size_t                length);

int32_t NaClCommonSysGetdents(struct NaClAppThread  *natp,
                              int                   d,
                              void                  *dirp,
                              size_t                count);

int32_t NaClCommonSysImc_MakeBoundSock(struct NaClAppThread *natp,
                                       int                  *sap);

int32_t NaClCommonSysImc_Accept(struct NaClAppThread  *natp,
                                int                   d);

int32_t NaClCommonSysImc_Connect(struct NaClAppThread *natp,
                                 int                  d);

int32_t NaClCommonSysImc_Sendmsg(struct NaClAppThread *natp,
                                 int                  d,
                                 struct NaClImcMsgHdr *nimhp,
                                 int                  flags);

int32_t NaClCommonSysImc_Recvmsg(struct NaClAppThread *natp,
                                 int                  d,
                                 struct NaClImcMsgHdr *nimhp,
                                 int                  flags);

int32_t NaClCommonSysImc_Mem_Obj_Create(struct NaClAppThread  *natp,
                                        size_t                size);

int32_t NaClCommonSysTls_Init(struct NaClAppThread  *natp,
                              void                  *tdb,
                              size_t                size);

int32_t NaClCommonSysThread_Create(struct NaClAppThread *natp,
                                   void                 *eip,
                                   void                 *stack_ptr,
                                   void                 *tdb,
                                   size_t               tdb_size);

#if defined(HAVE_SDL)

int32_t NaClCommonSysMultimedia_Init(struct NaClAppThread *natp,
                                     int                  subsys);

int32_t NaClCommonSysMultimedia_Shutdown(struct NaClAppThread *natp);

int32_t NaClCommonSysVideo_Init(struct NaClAppThread *natp,
                                int                  width,
                                int                  height);

int32_t NaClCommonSysVideo_Shutdown(struct NaClAppThread *natp);

int32_t NaClCommonSysVideo_Update(struct NaClAppThread *natp,
                                  const void           *data);

int32_t NaClCommonSysVideo_Poll_Event(struct NaClAppThread *natp,
                                      union NaClMultimediaEvent *event);

int32_t NaClCommonSysAudio_Init(struct NaClAppThread *natp,
                                enum NaClAudioFormat format,
                                int                  desired_samples,
                                int                  *obtained_samples);

int32_t NaClCommonSysAudio_Stream(struct NaClAppThread *natp,
                                  const void           *data,
                                  size_t               *size);

int32_t NaClCommonSysAudio_Shutdown(struct NaClAppThread *natp);

#endif /* HAVE_SDL */

/* mutex */

int32_t NaClCommonSysMutex_Create(struct NaClAppThread *natp);

int32_t NaClCommonSysMutex_Lock(struct NaClAppThread *natp,
                                int32_t              mutex_handle);

int32_t NaClCommonSysMutex_Unlock(struct NaClAppThread *natp,
                                  int32_t              mutex_handle);

int32_t NaClCommonSysMutex_Trylock(struct NaClAppThread *natp,
                                   int32_t              mutex_handle);

/* condition variable */

int32_t NaClCommonSysCond_Create(struct NaClAppThread *natp);

int32_t NaClCommonSysCond_Wait(struct NaClAppThread *natp,
                               int32_t              cond_handle,
                               int32_t              mutex_handle);

int32_t NaClCommonSysCond_Signal(struct NaClAppThread *natp,
                                 int32_t              cond_handle);

int32_t NaClCommonSysCond_Broadcast(struct NaClAppThread *natp,
                                    int32_t              cond_handle);

int32_t NaClCommonSysCond_Timed_Wait_Rel(struct NaClAppThread     *natp,
                                         int32_t                  cond_handle,
                                         int32_t                  mutex_handle,
                                         struct nacl_abi_timespec *ts);

int32_t NaClCommonSysCond_Timed_Wait_Abs(struct NaClAppThread     *natp,
                                         int32_t                  cond_handle,
                                         int32_t                  mutex_handle,
                                         struct nacl_abi_timespec *ts);

int32_t NaClCommonDescSocketPair(struct NaClDesc      **pair);

int32_t NaClCommonSysImc_SocketPair(struct NaClAppThread *natp,
                                    int32_t              *d_out);
/* Semaphores */
int32_t NaClCommonSysSem_Create(struct NaClAppThread *natp,
                                int32_t              init_value);

int32_t NaClCommonSysSem_Wait(struct NaClAppThread *natp,
                              int32_t              sem_handle);

int32_t NaClCommonSysSem_Post(struct NaClAppThread *natp,
                              int32_t              sem_handle);

int32_t NaClCommonSysSem_Get_Value(struct NaClAppThread *natp,
                                   int32_t              sem_handle);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SYSCALL_COMMON_H__ */
