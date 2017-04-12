// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletGlobalScope.h"

#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioWorkletProcessor.h"
#include "modules/webaudio/AudioWorkletProcessorDefinition.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

AudioWorkletGlobalScope* AudioWorkletGlobalScope::Create(
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate,
    WorkerThread* thread) {
  return new AudioWorkletGlobalScope(
      url, user_agent, std::move(security_origin), isolate, thread);
}

AudioWorkletGlobalScope::AudioWorkletGlobalScope(
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate,
    WorkerThread* thread)
    : ThreadedWorkletGlobalScope(url,
                                 user_agent,
                                 std::move(security_origin),
                                 isolate,
                                 thread) {}

AudioWorkletGlobalScope::~AudioWorkletGlobalScope() {}

void AudioWorkletGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  processor_definition_map_.Clear();
  processor_instances_.Clear();
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
  v8::Local<v8::Context> context =
      ScriptController()->GetScriptState()->GetContext();

  if (!class_definition.V8Value()->IsFunction()) {
    exception_state.ThrowTypeError(
        "The processor definition is neither 'class' nor 'function'.");
    return;
  }

  v8::Local<v8::Function> class_definition_local =
      class_definition.V8Value().As<v8::Function>();

  v8::Local<v8::Value> prototype_value_local;
  bool prototype_extracted =
      class_definition_local->Get(context, V8String(isolate, "prototype"))
          .ToLocal(&prototype_value_local);
  DCHECK(prototype_extracted);

  v8::Local<v8::Object> prototype_object_local =
      prototype_value_local.As<v8::Object>();

  v8::Local<v8::Value> process_value_local;
  bool process_extracted =
      prototype_object_local->Get(context, V8String(isolate, "process"))
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
      process_value_local.As<v8::Function>();

  AudioWorkletProcessorDefinition* definition =
      AudioWorkletProcessorDefinition::Create(
          isolate, name, class_definition_local, process_function_local);
  DCHECK(definition);

  processor_definition_map_.Set(name, definition);
}

AudioWorkletProcessor* AudioWorkletGlobalScope::CreateInstance(
    const String& name) {
  DCHECK(IsContextThread());

  AudioWorkletProcessorDefinition* definition = FindDefinition(name);
  if (!definition)
    return nullptr;

  // V8 object instance construction: this construction process is here to make
  // the AudioWorkletProcessor class a thin wrapper of V8::Object instance.
  v8::Isolate* isolate = ScriptController()->GetScriptState()->GetIsolate();
  v8::Local<v8::Object> instance_local;
  if (!V8ObjectConstructor::NewInstance(isolate,
                                        definition->ConstructorLocal(isolate))
           .ToLocal(&instance_local)) {
    return nullptr;
  }

  AudioWorkletProcessor* processor = AudioWorkletProcessor::Create(this, name);
  DCHECK(processor);

  processor->SetInstance(isolate, instance_local);
  processor_instances_.push_back(processor);

  return processor;
}

bool AudioWorkletGlobalScope::Process(AudioWorkletProcessor* processor,
                                      AudioBuffer* input_buffer,
                                      AudioBuffer* output_buffer) {
  CHECK(input_buffer);
  CHECK(output_buffer);

  ScriptState* script_state = ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  AudioWorkletProcessorDefinition* definition =
      FindDefinition(processor->GetName());
  DCHECK(definition);

  v8::Local<v8::Value> argv[] = {
      ToV8(input_buffer, script_state->GetContext()->Global(), isolate),
      ToV8(output_buffer, script_state->GetContext()->Global(), isolate)};

  // TODO(hongchan): Catch exceptions thrown in the process method. The verbose
  // options forces the TryCatch object to save the exception location. The
  // pending exception should be handled later.
  v8::TryCatch block(isolate);
  block.SetVerbose(true);

  // Perform JS function process() in AudioWorkletProcessor instance. The actual
  // V8 operation happens here to make the AudioWorkletProcessor class a thin
  // wrapper of v8::Object instance.
  V8ScriptRunner::CallFunction(
      definition->ProcessLocal(isolate), ExecutionContext::From(script_state),
      processor->InstanceLocal(isolate), WTF_ARRAY_LENGTH(argv), argv, isolate);

  return !block.HasCaught();
}

AudioWorkletProcessorDefinition* AudioWorkletGlobalScope::FindDefinition(
    const String& name) {
  return processor_definition_map_.at(name);
}

DEFINE_TRACE(AudioWorkletGlobalScope) {
  visitor->Trace(processor_definition_map_);
  visitor->Trace(processor_instances_);
  ThreadedWorkletGlobalScope::Trace(visitor);
}

}  // namespace blink
