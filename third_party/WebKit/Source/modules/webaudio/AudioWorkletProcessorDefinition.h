// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletProcessorDefinition_h
#define AudioWorkletProcessorDefinition_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"
#include "wtf/text/WTFString.h"

namespace blink {

// Represents a JavaScript class definition registered in the
// AudioWorkletGlobalScope. After the registration, a definition class contains
// the V8 representation of class components (constructor, process callback,
// prototypes and parameter descriptors).
//
// This is constructed and destroyed on a worker thread, and all methods also
// must be called on the worker thread.
class MODULES_EXPORT AudioWorkletProcessorDefinition final
    : public GarbageCollectedFinalized<AudioWorkletProcessorDefinition> {
 public:
  static AudioWorkletProcessorDefinition* Create(
      v8::Isolate*,
      const String& name,
      v8::Local<v8::Function> constructor,
      v8::Local<v8::Function> process);

  virtual ~AudioWorkletProcessorDefinition();

  const String& GetName() const { return name_; }
  v8::Local<v8::Function> ConstructorLocal(v8::Isolate*);
  v8::Local<v8::Function> ProcessLocal(v8::Isolate*);

  DEFINE_INLINE_TRACE(){};

 private:
  AudioWorkletProcessorDefinition(v8::Isolate*,
                                  const String& name,
                                  v8::Local<v8::Function> constructor,
                                  v8::Local<v8::Function> process);

  const String name_;

  // The definition is per global scope. The active instance of
  // |AudioProcessorWorklet| should be passed into these to perform JS function.
  ScopedPersistent<v8::Function> constructor_;
  ScopedPersistent<v8::Function> process_;

  // TODO(hongchan): A container for AudioParamDescriptor objects.
  // ScopedPersistent<v8::Array> m_parameterDescriptors;
};

}  // namespace blink

#endif  // AudioWorkletProcessorDefinition_h
