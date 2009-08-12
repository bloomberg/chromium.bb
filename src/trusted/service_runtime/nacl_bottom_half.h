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
 * NaCl service runtime.  Bottom half (asynchronous device handling) code.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_BOTTOM_HALF_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_BOTTOM_HALF_H_

#include "native_client/src/include/portability.h"  /* uintptr_t */
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/trusted/service_runtime/include/sys/audio_video.h"

struct NaClAppThread;
struct NaClClosure;

struct NaClClosureResult {
  struct NaClMutex    mu;
  struct NaClCondVar  cv;
  int                 result_valid;
  void                *rv;
};

int NaClClosureResultCtor(struct NaClClosureResult *self) NACL_WUR;

void NaClClosureResultDtor(struct NaClClosureResult *self);

void *NaClClosureResultWait(struct NaClClosureResult *self);

void NaClClosureResultDone(struct NaClClosureResult *self,
                           void                     *rv);

void NaClStartAsyncOp(struct NaClAppThread  *natp,
                      struct NaClClosure    *ncp);

int32_t NaClWaitForAsyncOp(struct NaClAppThread *natp) NACL_WUR;

#if defined(HAVE_SDL)

void NaClBotSysMultimedia_Init(struct NaClAppThread *natp,
                               int                  subsys);

void NaClBotSysMultimedia_Shutdown(struct NaClAppThread *natp);

void NaClBotSysVideo_Init(struct NaClAppThread *natp,
                          int                  width,
                          int                  height);

void NaClBotSysVideo_Shutdown(struct NaClAppThread *natp);

void NaClBotSysVideo_Update(struct NaClAppThread *natp,
                            const void           *data);

void NaClBotSysVideo_Poll_Event(struct NaClAppThread *natp,
                                union NaClMultimediaEvent *event);

void NaClBotSysAudio_Init(struct NaClAppThread *natp,
                          enum NaClAudioFormat format,
                          int                  desired_samples,
                          int                  *obtained_samples);

void NaClBotSysAudio_Shutdown(struct NaClAppThread *natp);

int32_t NaClSliceSysAudio_Stream(struct NaClAppThread *natp,
                                 const void           *data,
                                 size_t               *size) NACL_WUR;

#endif  /* defined(HAVE_SDL) */


#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_BOTTOM_HALF_H_ */
