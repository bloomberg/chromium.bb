// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletGlobalScope_h
#define AudioWorkletGlobalScope_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "modules/ModulesExport.h"
#include "modules/webaudio/AudioParamDescriptor.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class AudioBuffer;
class AudioWorkletProcessor;
class AudioWorkletProcessorDefinition;
class CrossThreadAudioWorkletProcessorInfo;
class ExceptionState;

// This is constructed and destroyed on a worker thread, and all methods also
// must be called on the worker thread.
class MODULES_EXPORT AudioWorkletGlobalScope final
    : public ThreadedWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioWorkletGlobalScope* Create(const KURL&,
                                         const String& user_agent,
                                         PassRefPtr<SecurityOrigin>,
                                         v8::Isolate*,
                                         WorkerThread*,
                                         WorkerClients*);
  ~AudioWorkletGlobalScope() override;
  bool IsAudioWorkletGlobalScope() const final { return true; }
  void registerProcessor(const String& name,
                         const ScriptValue& class_definition,
                         ExceptionState&);

  // Creates an instance of AudioWorkletProcessor from a registered name. This
  // function may return nullptr when 1) a definition cannot be found or 2) a
  // new V8 object cannot be constructed for some reason.
  AudioWorkletProcessor* CreateInstance(const String& name);

  // Invokes the JS audio processing function from an instance of
  // AudioWorkletProcessor, along with given AudioBuffer from the audio graph.
  bool Process(AudioWorkletProcessor*,
               AudioBuffer* input_buffer,
               AudioBuffer* output_buffer);

  AudioWorkletProcessorDefinition* FindDefinition(const String& name);

  unsigned NumberOfRegisteredDefinitions();

  std::unique_ptr<Vector<CrossThreadAudioWorkletProcessorInfo>>
      WorkletProcessorInfoListForSynchronization();

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  AudioWorkletGlobalScope(const KURL&,
                          const String& user_agent,
                          PassRefPtr<SecurityOrigin>,
                          v8::Isolate*,
                          WorkerThread*,
                          WorkerClients*);

  typedef HeapHashMap<String,
                      TraceWrapperMember<AudioWorkletProcessorDefinition>>
      ProcessorDefinitionMap;
  typedef HeapVector<TraceWrapperMember<AudioWorkletProcessor>>
      ProcessorInstances;

  ProcessorDefinitionMap processor_definition_map_;
  ProcessorInstances processor_instances_;
};

DEFINE_TYPE_CASTS(AudioWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsAudioWorkletGlobalScope(),
                  context.IsAudioWorkletGlobalScope());

}  // namespace blink

#endif  // AudioWorkletGlobalScope_h
