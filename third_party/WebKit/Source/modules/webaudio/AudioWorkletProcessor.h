// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletProcessor_h
#define AudioWorkletProcessor_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"
#include "wtf/text/WTFString.h"

namespace blink {

class AudioBuffer;
class AudioWorkletGlobalScope;
class AudioWorkletProcessorDefinition;

// AudioWorkletProcessor class represents the active instance created from
// AudioWorkletProcessorDefinition. |AudioWorkletNodeHandler| invokes
// process() method in this object upon graph rendering.
//
// This is constructed and destroyed on a worker thread, and all methods also
// must be called on the worker thread.
class MODULES_EXPORT AudioWorkletProcessor
    : public GarbageCollectedFinalized<AudioWorkletProcessor> {
 public:
  static AudioWorkletProcessor* Create(AudioWorkletGlobalScope*,
                                       const String& name);
  virtual ~AudioWorkletProcessor();

  void SetInstance(v8::Isolate*, v8::Local<v8::Object> instance);

  v8::Local<v8::Object> InstanceLocal(v8::Isolate*);

  // |AudioWorkletHandler| invokes this method to process audio.
  void Process(AudioBuffer* input_buffer, AudioBuffer* output_buffer);

  const String& GetName() const { return name_; }

  DECLARE_TRACE();

 private:
  AudioWorkletProcessor(AudioWorkletGlobalScope*, const String& name);

  Member<AudioWorkletGlobalScope> global_scope_;
  const String name_;
  ScopedPersistent<v8::Object> instance_;
};

}  // namespace blink

#endif  // AudioWorkletProcessor_h
