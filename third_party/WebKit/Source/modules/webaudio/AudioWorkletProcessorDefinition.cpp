// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletProcessorDefinition.h"

namespace blink {

AudioWorkletProcessorDefinition* AudioWorkletProcessorDefinition::create(
    v8::Isolate* isolate,
    const String& name,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> process) {
  DCHECK(!isMainThread());
  return new AudioWorkletProcessorDefinition(isolate, name, constructor,
                                             process);
}

AudioWorkletProcessorDefinition::AudioWorkletProcessorDefinition(
    v8::Isolate* isolate,
    const String& name,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> process)
    : m_name(name),
      m_constructor(isolate, constructor),
      m_process(isolate, process) {}

AudioWorkletProcessorDefinition::~AudioWorkletProcessorDefinition() {}

v8::Local<v8::Function> AudioWorkletProcessorDefinition::constructorLocal(
    v8::Isolate* isolate) {
  DCHECK(!isMainThread());
  return m_constructor.newLocal(isolate);
}

v8::Local<v8::Function> AudioWorkletProcessorDefinition::processLocal(
    v8::Isolate* isolate) {
  DCHECK(!isMainThread());
  return m_process.newLocal(isolate);
}

}  // namespace blink
