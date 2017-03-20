// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletGlobalScope_h
#define AudioWorkletGlobalScope_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "modules/ModulesExport.h"

namespace blink {

class AudioBuffer;
class AudioWorkletProcessor;
class AudioWorkletProcessorDefinition;
class ExceptionState;

// This is constructed and destroyed on a worker thread, and all methods also
// must be called on the worker thread.
class MODULES_EXPORT AudioWorkletGlobalScope final
    : public ThreadedWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioWorkletGlobalScope* create(const KURL&,
                                         const String& userAgent,
                                         PassRefPtr<SecurityOrigin>,
                                         v8::Isolate*,
                                         WorkerThread*);
  ~AudioWorkletGlobalScope() override;
  void dispose() final;
  bool isAudioWorkletGlobalScope() const final { return true; }
  void registerProcessor(const String& name,
                         const ScriptValue& classDefinition,
                         ExceptionState&);

  // Creates an instance of AudioWorkletProcessor from a registered name. This
  // function may return nullptr when 1) a definition cannot be found or 2) a
  // new V8 object cannot be constructed for some reason.
  AudioWorkletProcessor* createInstance(const String& name);

  // Invokes the JS audio processing function from an instance of
  // AudioWorkletProcessor, along with given AudioBuffer from the audio graph.
  bool process(AudioWorkletProcessor*,
               AudioBuffer* inputBuffer,
               AudioBuffer* outputBuffer);

  AudioWorkletProcessorDefinition* findDefinition(const String& name);

  DECLARE_TRACE();

 private:
  AudioWorkletGlobalScope(const KURL&,
                          const String& userAgent,
                          PassRefPtr<SecurityOrigin>,
                          v8::Isolate*,
                          WorkerThread*);

  typedef HeapHashMap<String, Member<AudioWorkletProcessorDefinition>>
      ProcessorDefinitionMap;
  typedef HeapVector<Member<AudioWorkletProcessor>> ProcessorInstances;

  ProcessorDefinitionMap m_processorDefinitionMap;
  ProcessorInstances m_processorInstances;
};

}  // namespace blink

#endif  // AudioWorkletGlobalScope_h
