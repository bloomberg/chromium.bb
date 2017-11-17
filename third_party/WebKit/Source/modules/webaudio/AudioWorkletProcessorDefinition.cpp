// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletProcessorDefinition.h"

namespace blink {

AudioWorkletProcessorDefinition* AudioWorkletProcessorDefinition::Create(
    v8::Isolate* isolate,
    const String& name,
    v8::Local<v8::Object> constructor,
    v8::Local<v8::Function> process) {
  DCHECK(!IsMainThread());
  return new AudioWorkletProcessorDefinition(isolate, name, constructor,
                                             process);
}

AudioWorkletProcessorDefinition::AudioWorkletProcessorDefinition(
    v8::Isolate* isolate,
    const String& name,
    v8::Local<v8::Object> constructor,
    v8::Local<v8::Function> process)
    : name_(name),
      constructor_(isolate, this, constructor),
      process_(isolate, this, process) {}

AudioWorkletProcessorDefinition::~AudioWorkletProcessorDefinition() {}

v8::Local<v8::Object> AudioWorkletProcessorDefinition::ConstructorLocal(
    v8::Isolate* isolate) {
  DCHECK(!IsMainThread());
  return constructor_.NewLocal(isolate);
}

v8::Local<v8::Function> AudioWorkletProcessorDefinition::ProcessLocal(
    v8::Isolate* isolate) {
  DCHECK(!IsMainThread());
  return process_.NewLocal(isolate);
}

void AudioWorkletProcessorDefinition::SetAudioParamDescriptors(
    const HeapVector<AudioParamDescriptor>& descriptors) {
  audio_param_descriptors_ = descriptors;
}

const Vector<String>
    AudioWorkletProcessorDefinition::GetAudioParamDescriptorNames() const {
  Vector<String> names;
  for (const auto& descriptor : audio_param_descriptors_) {
    names.push_back(descriptor.name());
  }
  return names;
}

const AudioParamDescriptor*
    AudioWorkletProcessorDefinition::GetAudioParamDescriptor (
        const String& key) const {
  for (const auto& descriptor : audio_param_descriptors_) {
    if (descriptor.name() == key)
      return &descriptor;
  }
  return nullptr;
}

void AudioWorkletProcessorDefinition::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(constructor_.Cast<v8::Value>());
  visitor->TraceWrappers(process_.Cast<v8::Value>());
}

}  // namespace blink
