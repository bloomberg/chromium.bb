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
class MessagePortChannel;
struct GlobalScopeCreationParams;


// The storage for the construction of AudioWorkletProcessor, contains the
// processor name and MessageChannelPort object.
class MODULES_EXPORT ProcessorCreationParams final {
 public:
  ProcessorCreationParams(const String& name,
                          MessagePortChannel message_port_channel)
      : name_(name), message_port_channel_(message_port_channel) {}

  ~ProcessorCreationParams() = default;

  const String& Name() const { return name_; }
  MessagePortChannel PortChannel() {  return message_port_channel_; }

 private:
  const String name_;
  MessagePortChannel message_port_channel_;
};


// This is constructed and destroyed on a worker thread, and all methods also
// must be called on the worker thread.
class MODULES_EXPORT AudioWorkletGlobalScope final
    : public ThreadedWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioWorkletGlobalScope* Create(
      std::unique_ptr<GlobalScopeCreationParams>,
      v8::Isolate*,
      WorkerThread*);
  ~AudioWorkletGlobalScope() override;
  bool IsAudioWorkletGlobalScope() const final { return true; }
  void Dispose() final;
  bool IsClosing() const final { return is_closing_; }

  void registerProcessor(const String& name,
                         const ScriptValue& class_definition,
                         ExceptionState&);

  // Creates an instance of AudioWorkletProcessor from a registered name.
  // This is invoked by AudioWorkletMessagingProxy upon the construction of
  // AudioWorkletNode.
  //
  // This function may return nullptr when a new V8 object cannot be constructed
  // for some reason.
  AudioWorkletProcessor* CreateProcessor(const String& name,
                                         float sample_rate,
                                         MessagePortChannel);

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

  // Gets |processor_creation_params_| for the processor construction. If there
  // is no on-going processor construction, this MUST return nullptr.
  ProcessorCreationParams* GetProcessorCreationParams();

  // IDL
  double currentTime() const { return current_time_; }
  float sampleRate() const { return sample_rate_; }

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  AudioWorkletGlobalScope(std::unique_ptr<GlobalScopeCreationParams>,
                          v8::Isolate*,
                          WorkerThread*);

  bool is_closing_ = false;

  typedef HeapHashMap<String,
                      TraceWrapperMember<AudioWorkletProcessorDefinition>>
      ProcessorDefinitionMap;
  typedef HeapVector<TraceWrapperMember<AudioWorkletProcessor>>
      ProcessorInstances;

  ProcessorDefinitionMap processor_definition_map_;
  ProcessorInstances processor_instances_;

  // Gets set when the processor construction is invoked, and cleared out after
  // the construction. See the comment in |CreateProcessor()| method for the
  // detail.
  std::unique_ptr<ProcessorCreationParams> processor_creation_params_;

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
