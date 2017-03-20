// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletProcessor.h"

#include "modules/webaudio/AudioWorkletGlobalScope.h"

namespace blink {

// This static factory should be called after an instance of |AudioWorkletNode|
// gets created by user-supplied JS code in the main thread. This factory must
// not be called by user in |AudioWorkletGlobalScope|.
AudioWorkletProcessor* AudioWorkletProcessor::create(
    AudioWorkletGlobalScope* globalScope,
    const String& name) {
  DCHECK(!isMainThread());
  DCHECK(globalScope);
  return new AudioWorkletProcessor(globalScope, name);
}

AudioWorkletProcessor::AudioWorkletProcessor(
    AudioWorkletGlobalScope* globalScope,
    const String& name)
    : m_globalScope(globalScope), m_name(name) {}

AudioWorkletProcessor::~AudioWorkletProcessor() {}

void AudioWorkletProcessor::setInstance(v8::Isolate* isolate,
                                        v8::Local<v8::Object> instance) {
  DCHECK(m_globalScope->isContextThread());
  m_instance.set(isolate, instance);
}

v8::Local<v8::Object> AudioWorkletProcessor::instanceLocal(
    v8::Isolate* isolate) {
  DCHECK(m_globalScope->isContextThread());
  return m_instance.newLocal(isolate);
}

void AudioWorkletProcessor::process(AudioBuffer* inputBuffer,
                                    AudioBuffer* outputBuffer) {
  DCHECK(m_globalScope->isContextThread());
  m_globalScope->process(this, inputBuffer, outputBuffer);
}

DEFINE_TRACE(AudioWorkletProcessor) {
  visitor->trace(m_globalScope);
}

}  // namespace blink
