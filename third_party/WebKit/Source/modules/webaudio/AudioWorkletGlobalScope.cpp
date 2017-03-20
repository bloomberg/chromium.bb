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

AudioWorkletGlobalScope* AudioWorkletGlobalScope::create(
    const KURL& url,
    const String& userAgent,
    PassRefPtr<SecurityOrigin> securityOrigin,
    v8::Isolate* isolate,
    WorkerThread* thread) {
  return new AudioWorkletGlobalScope(url, userAgent, std::move(securityOrigin),
                                     isolate, thread);
}

AudioWorkletGlobalScope::AudioWorkletGlobalScope(
    const KURL& url,
    const String& userAgent,
    PassRefPtr<SecurityOrigin> securityOrigin,
    v8::Isolate* isolate,
    WorkerThread* thread)
    : ThreadedWorkletGlobalScope(url,
                                 userAgent,
                                 std::move(securityOrigin),
                                 isolate,
                                 thread) {}

AudioWorkletGlobalScope::~AudioWorkletGlobalScope() {}

void AudioWorkletGlobalScope::dispose() {
  DCHECK(isContextThread());
  m_processorDefinitionMap.clear();
  m_processorInstances.clear();
  ThreadedWorkletGlobalScope::dispose();
}

void AudioWorkletGlobalScope::registerProcessor(
    const String& name,
    const ScriptValue& classDefinition,
    ExceptionState& exceptionState) {
  DCHECK(isContextThread());

  if (m_processorDefinitionMap.contains(name)) {
    exceptionState.throwDOMException(
        NotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  // TODO(hongchan): this is not stated in the spec, but seems necessary.
  // https://github.com/WebAudio/web-audio-api/issues/1172
  if (name.isEmpty()) {
    exceptionState.throwTypeError("The empty string is not a valid name.");
    return;
  }

  v8::Isolate* isolate = scriptController()->getScriptState()->isolate();
  v8::Local<v8::Context> context =
      scriptController()->getScriptState()->context();

  if (!classDefinition.v8Value()->IsFunction()) {
    exceptionState.throwTypeError(
        "The processor definition is neither 'class' nor 'function'.");
    return;
  }

  v8::Local<v8::Function> classDefinitionLocal =
      classDefinition.v8Value().As<v8::Function>();

  v8::Local<v8::Value> prototypeValueLocal;
  bool prototypeExtracted =
      classDefinitionLocal->Get(context, v8String(isolate, "prototype"))
          .ToLocal(&prototypeValueLocal);
  DCHECK(prototypeExtracted);

  v8::Local<v8::Object> prototypeObjectLocal =
      prototypeValueLocal.As<v8::Object>();

  v8::Local<v8::Value> processValueLocal;
  bool processExtracted =
      prototypeObjectLocal->Get(context, v8String(isolate, "process"))
          .ToLocal(&processValueLocal);
  DCHECK(processExtracted);

  if (processValueLocal->IsNullOrUndefined()) {
    exceptionState.throwTypeError(
        "The 'process' function does not exist in the prototype.");
    return;
  }

  if (!processValueLocal->IsFunction()) {
    exceptionState.throwTypeError(
        "The 'process' property on the prototype is not a function.");
    return;
  }

  v8::Local<v8::Function> processFunctionLocal =
      processValueLocal.As<v8::Function>();

  AudioWorkletProcessorDefinition* definition =
      AudioWorkletProcessorDefinition::create(
          isolate, name, classDefinitionLocal, processFunctionLocal);
  DCHECK(definition);

  m_processorDefinitionMap.set(name, definition);
}

AudioWorkletProcessor* AudioWorkletGlobalScope::createInstance(
    const String& name) {
  DCHECK(isContextThread());

  AudioWorkletProcessorDefinition* definition = findDefinition(name);
  if (!definition)
    return nullptr;

  // V8 object instance construction: this construction process is here to make
  // the AudioWorkletProcessor class a thin wrapper of V8::Object instance.
  v8::Isolate* isolate = scriptController()->getScriptState()->isolate();
  v8::Local<v8::Object> instanceLocal;
  if (!V8ObjectConstructor::newInstance(isolate,
                                        definition->constructorLocal(isolate))
           .ToLocal(&instanceLocal)) {
    return nullptr;
  }

  AudioWorkletProcessor* processor = AudioWorkletProcessor::create(this, name);
  DCHECK(processor);

  processor->setInstance(isolate, instanceLocal);
  m_processorInstances.push_back(processor);

  return processor;
}

bool AudioWorkletGlobalScope::process(AudioWorkletProcessor* processor,
                                      AudioBuffer* inputBuffer,
                                      AudioBuffer* outputBuffer) {
  CHECK(inputBuffer);
  CHECK(outputBuffer);

  ScriptState* scriptState = scriptController()->getScriptState();
  ScriptState::Scope scope(scriptState);

  v8::Isolate* isolate = scriptState->isolate();
  AudioWorkletProcessorDefinition* definition =
      findDefinition(processor->name());
  DCHECK(definition);

  v8::Local<v8::Value> argv[] = {
      ToV8(inputBuffer, scriptState->context()->Global(), isolate),
      ToV8(outputBuffer, scriptState->context()->Global(), isolate)};

  // TODO(hongchan): Catch exceptions thrown in the process method. The verbose
  // options forces the TryCatch object to save the exception location. The
  // pending exception should be handled later.
  v8::TryCatch block(isolate);
  block.SetVerbose(true);

  // Perform JS function process() in AudioWorkletProcessor instance. The actual
  // V8 operation happens here to make the AudioWorkletProcessor class a thin
  // wrapper of v8::Object instance.
  V8ScriptRunner::callFunction(
      definition->processLocal(isolate), scriptState->getExecutionContext(),
      processor->instanceLocal(isolate), WTF_ARRAY_LENGTH(argv), argv, isolate);

  return !block.HasCaught();
}

AudioWorkletProcessorDefinition* AudioWorkletGlobalScope::findDefinition(
    const String& name) {
  return m_processorDefinitionMap.at(name);
}

DEFINE_TRACE(AudioWorkletGlobalScope) {
  visitor->trace(m_processorDefinitionMap);
  visitor->trace(m_processorInstances);
  ThreadedWorkletGlobalScope::trace(visitor);
}

}  // namespace blink
