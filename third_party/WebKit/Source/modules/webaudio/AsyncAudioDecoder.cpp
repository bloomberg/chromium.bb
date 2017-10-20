/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "modules/webaudio/AsyncAudioDecoder.h"

#include "bindings/modules/v8/v8_decode_error_callback.h"
#include "bindings/modules/v8/v8_decode_success_callback.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioFileReader.h"
#include "platform/threading/BackgroundTaskRunner.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

void AsyncAudioDecoder::DecodeAsync(DOMArrayBuffer* audio_data,
                                    float sample_rate,
                                    V8DecodeSuccessCallback* success_callback,
                                    V8DecodeErrorCallback* error_callback,
                                    ScriptPromiseResolver* resolver,
                                    BaseAudioContext* context) {
  DCHECK(IsMainThread());
  DCHECK(audio_data);
  if (!audio_data)
    return;

  BackgroundTaskRunner::PostOnBackgroundThread(
      BLINK_FROM_HERE,
      CrossThreadBind(&AsyncAudioDecoder::DecodeOnBackgroundThread,
                      WrapCrossThreadPersistent(audio_data), sample_rate,
                      WrapCrossThreadPersistent(success_callback),
                      WrapCrossThreadPersistent(error_callback),
                      WrapCrossThreadPersistent(resolver),
                      WrapCrossThreadPersistent(context)));
}

void AsyncAudioDecoder::DecodeOnBackgroundThread(
    DOMArrayBuffer* audio_data,
    float sample_rate,
    V8DecodeSuccessCallback* success_callback,
    V8DecodeErrorCallback* error_callback,
    ScriptPromiseResolver* resolver,
    BaseAudioContext* context) {
  DCHECK(!IsMainThread());
  scoped_refptr<AudioBus> bus = CreateBusFromInMemoryAudioFile(
      audio_data->Data(), audio_data->ByteLength(), false, sample_rate);

  // Decoding is finished, but we need to do the callbacks on the main thread.
  // A reference to |*bus| is retained by WTF::Function and will be removed
  // after notifyComplete() is done.
  //
  // We also want to avoid notifying the main thread if AudioContext does not
  // exist any more.
  if (context) {
    Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&AsyncAudioDecoder::NotifyComplete,
                        WrapCrossThreadPersistent(audio_data),
                        WrapCrossThreadPersistent(success_callback),
                        WrapCrossThreadPersistent(error_callback),
                        WTF::RetainedRef(std::move(bus)),
                        WrapCrossThreadPersistent(resolver),
                        WrapCrossThreadPersistent(context)));
  }
}

void AsyncAudioDecoder::NotifyComplete(
    DOMArrayBuffer*,
    V8DecodeSuccessCallback* success_callback,
    V8DecodeErrorCallback* error_callback,
    AudioBus* audio_bus,
    ScriptPromiseResolver* resolver,
    BaseAudioContext* context) {
  DCHECK(IsMainThread());

  AudioBuffer* audio_buffer = AudioBuffer::CreateFromAudioBus(audio_bus);

  // If the context is available, let the context finish the notification.
  if (context) {
    context->HandleDecodeAudioData(audio_buffer, resolver, success_callback,
                                   error_callback);
  }
}

}  // namespace blink
