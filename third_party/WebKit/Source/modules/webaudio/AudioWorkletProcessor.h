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
  static AudioWorkletProcessor* create(AudioWorkletGlobalScope*,
                                       const String& name);
  virtual ~AudioWorkletProcessor();

  void setInstance(v8::Isolate*, v8::Local<v8::Object> instance);

  v8::Local<v8::Object> instanceLocal(v8::Isolate*);

  // |AudioWorkletHandler| invokes this method to process audio.
  void process(AudioBuffer* inputBuffer, AudioBuffer* outputBuffer);

  const String& name() const { return m_name; }

  DECLARE_TRACE();

 private:
  AudioWorkletProcessor(AudioWorkletGlobalScope*, const String& name);

  Member<AudioWorkletGlobalScope> m_globalScope;
  const String m_name;
  ScopedPersistent<v8::Object> m_instance;
};

}  // namespace blink

#endif  // AudioWorkletProcessor_h
