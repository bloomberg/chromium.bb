// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletProcessorDefinition.h"

namespace blink {

AudioWorkletProcessorDefinition* AudioWorkletProcessorDefinition::Create(
    v8::Isolate* isolate,
    const String& name,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> process) {
  DCHECK(!IsMainThread());
  return new AudioWorkletProcessorDefinition(isolate, name, constructor,
                                             process);
}

AudioWorkletProcessorDefinition::AudioWorkletProcessorDefinition(
    v8::Isolate* isolate,
    const String& name,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> process)
    : name_(name),
      constructor_(isolate, constructor),
      process_(isolate, process) {}

AudioWorkletProcessorDefinition::~AudioWorkletProcessorDefinition() {}

v8::Local<v8::Function> AudioWorkletProcessorDefinition::ConstructorLocal(
    v8::Isolate* isolate) {
  DCHECK(!IsMainThread());
  return constructor_.NewLocal(isolate);
}

v8::Local<v8::Function> AudioWorkletProcessorDefinition::ProcessLocal(
    v8::Isolate* isolate) {
  DCHECK(!IsMainThread());
  return process_.NewLocal(isolate);
}

}  // namespace blink
