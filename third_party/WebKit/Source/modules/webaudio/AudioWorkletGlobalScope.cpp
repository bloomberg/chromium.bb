// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletGlobalScope.h"

#include <memory>
#include <utility>

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "bindings/modules/v8/V8AudioParamDescriptor.h"
#include "bindings/modules/v8/V8AudioWorkletProcessor.h"
#include "core/dom/ExceptionCode.h"
#include "core/messaging/MessagePort.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioParamDescriptor.h"
#include "modules/webaudio/AudioWorkletProcessor.h"
#include "modules/webaudio/AudioWorkletProcessorDefinition.h"
#include "modules/webaudio/CrossThreadAudioWorkletProcessorInfo.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

AudioWorkletGlobalScope* AudioWorkletGlobalScope::Create(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    v8::Isolate* isolate,
    WorkerThread* thread) {
  return new AudioWorkletGlobalScope(std::move(creation_params), isolate,
                                     thread);
}

AudioWorkletGlobalScope::AudioWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    v8::Isolate* isolate,
    WorkerThread* thread)
    : ThreadedWorkletGlobalScope(std::move(creation_params), isolate, thread) {}

AudioWorkletGlobalScope::~AudioWorkletGlobalScope() {}

void AudioWorkletGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  is_closing_ = true;
  ThreadedWorkletGlobalScope::Dispose();
}

void AudioWorkletGlobalScope::registerProcessor(
    const String& name,
    const ScriptValue& class_definition,
    ExceptionState& exception_state) {
  DCHECK(IsContextThread());

  if (processor_definition_map_.Contains(name)) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  // TODO(hongchan): this is not stated in the spec, but seems necessary.
  // https://github.com/WebAudio/web-audio-api/issues/1172
  if (name.IsEmpty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  v8::Isolate* isolate = ScriptController()->GetScriptState()->GetIsolate();
  v8::Local<v8::Context> context = ScriptController()->GetContext();

  if (!class_definition.V8Value()->IsFunction()) {
    exception_state.ThrowTypeError(
        "The processor definition is neither 'class' nor 'function'.");
    return;
  }

  v8::Local<v8::Object> class_definition_local =
      v8::Local<v8::Object>::Cast(class_definition.V8Value());

  v8::Local<v8::Value> prototype_value_local;
  bool prototype_extracted =
      class_definition_local->Get(context, V8AtomicString(isolate, "prototype"))
          .ToLocal(&prototype_value_local);
  DCHECK(prototype_extracted);

  v8::Local<v8::Object> prototype_object_local =
      v8::Local<v8::Object>::Cast(prototype_value_local);

  v8::Local<v8::Value> process_value_local;
  bool process_extracted =
      prototype_object_local->Get(context, V8AtomicString(isolate, "process"))
          .ToLocal(&process_value_local);
  DCHECK(process_extracted);

  if (process_value_local->IsNullOrUndefined()) {
    exception_state.ThrowTypeError(
        "The 'process' function does not exist in the prototype.");
    return;
  }

  if (!process_value_local->IsFunction()) {
    exception_state.ThrowTypeError(
        "The 'process' property on the prototype is not a function.");
    return;
  }

  v8::Local<v8::Function> process_function_local =
      v8::Local<v8::Function>::Cast(process_value_local);

  // constructor() and process() functions are successfully parsed from the
  // script code, thus create the definition. The rest of parsing process
  // (i.e. parameterDescriptors) is optional.
  AudioWorkletProcessorDefinition* definition =
      AudioWorkletProcessorDefinition::Create(
          isolate, name, class_definition_local, process_function_local);
  DCHECK(definition);

  v8::Local<v8::Value> parameter_descriptors_value_local;
  bool did_get_parameter_descriptor =
      class_definition_local
          ->Get(context, V8AtomicString(isolate, "parameterDescriptors"))
          .ToLocal(&parameter_descriptors_value_local);

  // If parameterDescriptor() is parsed and has a valid value, create a vector
  // of |AudioParamDescriptor| and pass it to the definition.
  if (did_get_parameter_descriptor &&
      !parameter_descriptors_value_local->IsNullOrUndefined()) {
    HeapVector<AudioParamDescriptor> audio_param_descriptors =
        NativeValueTraits<IDLSequence<AudioParamDescriptor>>::NativeValue(
            isolate, parameter_descriptors_value_local, exception_state);

    if (exception_state.HadException())
      return;

    definition->SetAudioParamDescriptors(audio_param_descriptors);
  }

  processor_definition_map_.Set(name, definition);
}

AudioWorkletProcessor* AudioWorkletGlobalScope::CreateProcessor(
    const String& name,
    float sample_rate,
    MessagePortChannel message_port_channel) {
  DCHECK(IsContextThread());

  // TODO(hongchan): do this only once when the association between
  // BaseAudioContext and AudioWorkletGlobalScope is established.
  sample_rate_ = sample_rate;

  // The registered definition is already checked by AudioWorkletNode
  // construction process, so the |definition| here must be valid.
  AudioWorkletProcessorDefinition* definition = FindDefinition(name);
  DCHECK(definition);

  ScriptState* script_state = ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);

  // V8 object instance construction: this construction process is here to make
  // the AudioWorkletProcessor class a thin wrapper of V8::Object instance.
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch block(isolate);

  // Routes errors/exceptions to the dev console.
  block.SetVerbose(true);

  DCHECK(!processor_creation_params_);
  processor_creation_params_ = std::make_unique<ProcessorCreationParams>(
      name, std::move(message_port_channel));

  // This invokes the static constructor of AudioWorkletProcessor. There is no
  // way to pass additional constructor arguments that are not described in
  // WebIDL, the static constructor will look up |processor_creation_params_| in
  // the global scope to perform the construction properly.
  v8::Local<v8::Value> result;
  bool did_construct =
      V8ScriptRunner::CallAsConstructor(isolate,
                                        definition->ConstructorLocal(isolate),
                                        ExecutionContext::From(script_state),
                                        0, nullptr)
          .ToLocal(&result);
  processor_creation_params_.reset();

  // If 1) the attempt to call the constructor fails, 2) an error was thrown
  // by the user-supplied constructor code. The invalid construction process
  if (!did_construct || block.HasCaught()) {
    return nullptr;
  }

  // ToImplWithTypeCheck() may return nullptr whenthe type does not match.
  AudioWorkletProcessor* processor =
      V8AudioWorkletProcessor::ToImplWithTypeCheck(isolate, result);

  if (processor) {
    processor_instances_.push_back(processor);
  }

  return processor;
}

bool AudioWorkletGlobalScope::Process(
    AudioWorkletProcessor* processor,
    Vector<AudioBus*>* input_buses,
    Vector<AudioBus*>* output_buses,
    HashMap<String, std::unique_ptr<AudioFloatArray>>* param_value_map,
    double current_time) {
  CHECK_GE(input_buses->size(), 0u);
  CHECK_GE(output_buses->size(), 0u);

  // Note that all AudioWorkletProcessors share this method for the processing.
  // AudioWorkletGlobalScope's |current_time_| must be updated only once per
  // render quantum.
  if (current_time_ < current_time) {
    current_time_ = current_time;
  }

  ScriptState* script_state = ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  AudioWorkletProcessorDefinition* definition =
      FindDefinition(processor->Name());
  DCHECK(definition);

  // To expose AudioBuffer on JS side, we have to repackage |Vector<AudioBus*>|
  // to |sequence<sequence<Float32Array>>|.
  HeapVector<HeapVector<Member<DOMFloat32Array>>> inputs;
  HeapVector<HeapVector<Member<DOMFloat32Array>>> outputs;

  for (const auto input_bus : *input_buses) {
    HeapVector<Member<DOMFloat32Array>> input;
    for (unsigned channel_index = 0;
         channel_index < input_bus->NumberOfChannels();
         ++channel_index) {
      DOMFloat32Array* channel_data_array =
          DOMFloat32Array::Create(input_bus->length());
      memcpy(channel_data_array->Data(),
             input_bus->Channel(channel_index)->Data(),
             input_bus->length() * sizeof(float));
      input.push_back(channel_data_array);
    }
    inputs.push_back(input);
  }

  for (const auto output_bus : *output_buses) {
    HeapVector<Member<DOMFloat32Array>> output;
    for (unsigned channel_index = 0;
         channel_index < output_bus->NumberOfChannels();
         ++channel_index) {
      output.push_back(
          DOMFloat32Array::Create(output_bus->length()));
    }
    outputs.push_back(output);
  }

  V8ObjectBuilder param_values(script_state);
  for (const auto& param_name : param_value_map->Keys()) {
    const AudioFloatArray* source_param_array = param_value_map->at(param_name);
    DOMFloat32Array* param_array =
        DOMFloat32Array::Create(source_param_array->size());
    memcpy(param_array->Data(),
           source_param_array->Data(),
           sizeof(float) * source_param_array->size());
    param_values.Add(
        StringView(param_name.IsolatedCopy()),
        ToV8(param_array, script_state->GetContext()->Global(), isolate));
  }

  v8::Local<v8::Value> argv[] = {
    ToV8(inputs, script_state->GetContext()->Global(), isolate),
    ToV8(outputs, script_state->GetContext()->Global(), isolate),
    param_values.V8Value()
  };

  v8::TryCatch block(isolate);
  block.SetVerbose(true);

  // Perform JS function process() in AudioWorkletProcessor instance. The actual
  // V8 operation happens here to make the AudioWorkletProcessor class a thin
  // wrapper of v8::Object instance.
  v8::Local<v8::Value> processor_handle =
      ToV8(processor, script_state->GetContext()->Global(), isolate);
  v8::Local<v8::Value> local_result;
  if (!V8ScriptRunner::CallFunction(definition->ProcessLocal(isolate),
                                    ExecutionContext::From(script_state),
                                    processor_handle,
                                    WTF_ARRAY_LENGTH(argv),
                                    argv,
                                    isolate).ToLocal(&local_result) ||
    block.HasCaught()) {
    // process() method call method call failed for some reason or an exception
    // was thrown by the user supplied code. Disable the processor to exclude
    // it from the subsequent rendering task.
    processor->MarkNonRunnable();
    return false;
  }

  // TODO(hongchan): Sanity check on length, number of channels, and object
  // type.

  // Copy |sequence<sequence<Float32Array>>| back to the original
  // |Vector<AudioBus*>|.
  for (unsigned output_index = 0;
       output_index < output_buses->size();
       ++output_index) {
    HeapVector<Member<DOMFloat32Array>> output = outputs.at(output_index);
    AudioBus* original_output_bus = output_buses->at(output_index);
    for (unsigned channel_index = 0;
         channel_index < original_output_bus->NumberOfChannels();
         ++channel_index) {
      memcpy(original_output_bus->Channel(channel_index)->MutableData(),
             output.at(channel_index)->Data(),
             sizeof(float) * original_output_bus->length());
    }
  }

  // Return the value from the user-supplied |process()| function. It is
  // used to maintain the lifetime of the node and the processor.
  return local_result->IsTrue() && !block.HasCaught();
}

AudioWorkletProcessorDefinition* AudioWorkletGlobalScope::FindDefinition(
    const String& name) {
  return processor_definition_map_.at(name);
}

unsigned AudioWorkletGlobalScope::NumberOfRegisteredDefinitions() {
  return processor_definition_map_.size();
}

std::unique_ptr<Vector<CrossThreadAudioWorkletProcessorInfo>>
AudioWorkletGlobalScope::WorkletProcessorInfoListForSynchronization() {
  auto processor_info_list =
      std::make_unique<Vector<CrossThreadAudioWorkletProcessorInfo>>();
  for (auto definition_entry : processor_definition_map_) {
    if (!definition_entry.value->IsSynchronized()) {
      definition_entry.value->MarkAsSynchronized();
      processor_info_list->emplace_back(*definition_entry.value);
    }
  }
  return processor_info_list;
}

ProcessorCreationParams* AudioWorkletGlobalScope::GetProcessorCreationParams() {
  return processor_creation_params_.get();
}

void AudioWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(processor_definition_map_);
  visitor->Trace(processor_instances_);
  ThreadedWorkletGlobalScope::Trace(visitor);
}

void AudioWorkletGlobalScope::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  for (auto definition : processor_definition_map_)
    visitor->TraceWrappers(definition.value);

  for (auto processor : processor_instances_)
    visitor->TraceWrappers(processor);

  ThreadedWorkletGlobalScope::TraceWrappers(visitor);
}

}  // namespace blink
