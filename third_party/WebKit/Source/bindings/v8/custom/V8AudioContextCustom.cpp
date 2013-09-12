/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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
#include "V8AudioContext.h"

#include "V8AudioBuffer.h"
#include "V8OfflineAudioContext.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/custom/V8ArrayBufferCustom.h"
#include "core/dom/Document.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioContext.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "wtf/ArrayBuffer.h"

namespace WebCore {

void V8AudioContext::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    Document* document = currentDocument();

    RefPtr<AudioContext> audioContext;

    if (!args.Length()) {
        // Constructor for default AudioContext which talks to audio hardware.
        audioContext = AudioContext::create(document);
        if (!audioContext.get()) {
            throwError(v8SyntaxError, "audio resources unavailable for AudioContext construction", args.GetIsolate());
            return;
        }
    } else {
        // Constructor for offline (render-target) AudioContext which renders into an AudioBuffer.
        // new AudioContext(in unsigned long numberOfChannels, in unsigned long numberOfFrames, in float sampleRate);
        document->addConsoleMessage(JSMessageSource, WarningMessageLevel,
            "Deprecated AudioContext constructor: use OfflineAudioContext instead");

        V8OfflineAudioContext::constructorCallback(args);
        return;
    }

    if (!audioContext.get()) {
        throwError(v8SyntaxError, "Error creating AudioContext", args.GetIsolate());
        return;
    }

    // Transform the holder into a wrapper object for the audio context.
    v8::Handle<v8::Object> wrapper = args.Holder();
    V8DOMWrapper::associateObjectWithWrapper<V8AudioContext>(audioContext.release(), &info, wrapper, args.GetIsolate(), WrapperConfiguration::Dependent);
    args.GetReturnValue().Set(wrapper);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
