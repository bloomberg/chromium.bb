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
#include "platform/audio/AudioArray.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class AudioBus;
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
  static AudioWorkletGlobalScope* Create(
      const KURL&,
      const String& user_agent,
      RefPtr<SecurityOrigin> document_security_origin,
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
  AudioWorkletProcessor* CreateInstance(const String& name, float sample_rate);

  // Invokes the JS audio processing function from an instance of
  // AudioWorkletProcessor, along with given AudioBuffer from the audio graph.
  bool Process(
      AudioWorkletProcessor*,
      Vector<AudioBus*>* input_buses,
      Vector<AudioBus*>* output_buses,
      HashMap<String, std::unique_ptr<AudioFloatArray>>* param_value_map,
      double current_time);

  AudioWorkletProcessorDefinition* FindDefinition(const String& name);

  unsigned NumberOfRegisteredDefinitions();

  std::unique_ptr<Vector<CrossThreadAudioWorkletProcessorInfo>>
      WorkletProcessorInfoListForSynchronization();

  // IDL
  double currentTime() const { return current_time_; }
  float sampleRate() const { return sample_rate_; }

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  AudioWorkletGlobalScope(const KURL&,
                          const String& user_agent,
                          RefPtr<SecurityOrigin> document_security_origin,
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
  double current_time_ = 0.0;
  float sample_rate_ = 0.0;
};

DEFINE_TYPE_CASTS(AudioWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsAudioWorkletGlobalScope(),
                  context.IsAudioWorkletGlobalScope());

}  // namespace blink

#endif  // AudioWorkletGlobalScope_h
