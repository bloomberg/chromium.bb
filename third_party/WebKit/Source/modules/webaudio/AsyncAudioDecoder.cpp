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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "modules/webaudio/AsyncAudioDecoder.h"

#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioBufferCallback.h"
#include "platform/Task.h"
#include "public/platform/Platform.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/MainThread.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

AsyncAudioDecoder::AsyncAudioDecoder()
    : m_thread(adoptPtr(WebKit::Platform::current()->createThread("Audio Decoder")))
{
}

AsyncAudioDecoder::~AsyncAudioDecoder()
{
}

void AsyncAudioDecoder::decodeAsync(ArrayBuffer* audioData, float sampleRate, PassRefPtr<AudioBufferCallback> successCallback, PassRefPtr<AudioBufferCallback> errorCallback)
{
    ASSERT(isMainThread());
    ASSERT(audioData);
    if (!audioData)
        return;

    // Add a ref to keep audioData alive until completion of decoding.
    RefPtr<ArrayBuffer> audioDataRef(audioData);

    // The leak references to successCallback and errorCallback are picked up on notifyComplete.
    m_thread->postTask(new Task(WTF::bind(&AsyncAudioDecoder::decode, audioDataRef.release().leakRef(), sampleRate, successCallback.leakRef(), errorCallback.leakRef())));
}

void AsyncAudioDecoder::decode(ArrayBuffer* audioData, float sampleRate, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback)
{
    // Do the actual decoding and invoke the callback.
    RefPtr<AudioBuffer> audioBuffer = AudioBuffer::createFromAudioFileData(audioData->data(), audioData->byteLength(), false, sampleRate);

    // Decoding is finished, but we need to do the callbacks on the main thread.
    // The leaked reference to audioBuffer is picked up in notifyComplete.
    callOnMainThread(WTF::bind(&AsyncAudioDecoder::notifyComplete, audioData, successCallback, errorCallback, audioBuffer.release().leakRef()));
}

void AsyncAudioDecoder::notifyComplete(ArrayBuffer* audioData, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback, AudioBuffer* audioBuffer)
{
    // Adopt references, so everything gets correctly dereffed.
    RefPtr<ArrayBuffer> audioDataRef = adoptRef(audioData);
    RefPtr<AudioBufferCallback> successCallbackRef = adoptRef(successCallback);
    RefPtr<AudioBufferCallback> errorCallbackRef = adoptRef(errorCallback);
    RefPtr<AudioBuffer> audioBufferRef = adoptRef(audioBuffer);

    if (audioBuffer && successCallback)
        successCallback->handleEvent(audioBuffer);
    else if (errorCallback)
        errorCallback->handleEvent(audioBuffer);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
