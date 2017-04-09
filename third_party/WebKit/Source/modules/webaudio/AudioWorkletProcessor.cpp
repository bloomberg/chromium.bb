// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletProcessor.h"

#include "modules/webaudio/AudioWorkletGlobalScope.h"

namespace blink {

// This static factory should be called after an instance of |AudioWorkletNode|
// gets created by user-supplied JS code in the main thread. This factory must
// not be called by user in |AudioWorkletGlobalScope|.
AudioWorkletProcessor* AudioWorkletProcessor::Create(
    AudioWorkletGlobalScope* global_scope,
    const String& name) {
  DCHECK(!IsMainThread());
  DCHECK(global_scope);
  return new AudioWorkletProcessor(global_scope, name);
}

AudioWorkletProcessor::AudioWorkletProcessor(
    AudioWorkletGlobalScope* global_scope,
    const String& name)
    : global_scope_(global_scope), name_(name) {}

AudioWorkletProcessor::~AudioWorkletProcessor() {}

void AudioWorkletProcessor::SetInstance(v8::Isolate* isolate,
                                        v8::Local<v8::Object> instance) {
  DCHECK(global_scope_->IsContextThread());
  instance_.Set(isolate, instance);
}

v8::Local<v8::Object> AudioWorkletProcessor::InstanceLocal(
    v8::Isolate* isolate) {
  DCHECK(global_scope_->IsContextThread());
  return instance_.NewLocal(isolate);
}

void AudioWorkletProcessor::Process(AudioBuffer* input_buffer,
                                    AudioBuffer* output_buffer) {
  DCHECK(global_scope_->IsContextThread());
  global_scope_->Process(this, input_buffer, output_buffer);
}

DEFINE_TRACE(AudioWorkletProcessor) {
  visitor->Trace(global_scope_);
}

}  // namespace blink
