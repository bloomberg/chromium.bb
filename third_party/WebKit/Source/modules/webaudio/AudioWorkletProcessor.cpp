// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletProcessor.h"

#include "core/messaging/MessagePort.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioWorkletGlobalScope.h"

namespace blink {

AudioWorkletProcessor* AudioWorkletProcessor::Create(
    ExecutionContext* context) {
  AudioWorkletGlobalScope* global_scope = ToAudioWorkletGlobalScope(context);
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());

  // Get the stored initialization parameter from the global scope.
  ProcessorCreationParams* params = global_scope->GetProcessorCreationParams();
  DCHECK(params);

  MessagePort* port = MessagePort::Create(*global_scope);
  port->Entangle(std::move(params->PortChannel()));
  return new AudioWorkletProcessor(global_scope, params->Name(), port);
}

AudioWorkletProcessor::AudioWorkletProcessor(
    AudioWorkletGlobalScope* global_scope,
    const String& name,
    MessagePort* port)
    : global_scope_(global_scope), processor_port_(port), name_(name) {}

bool AudioWorkletProcessor::Process(
    Vector<AudioBus*>* input_buses,
    Vector<AudioBus*>* output_buses,
    HashMap<String, std::unique_ptr<AudioFloatArray>>* param_value_map,
    double current_time) {
  DCHECK(global_scope_->IsContextThread());
  DCHECK(IsRunnable());
  return global_scope_->Process(this, input_buses, output_buses,
                                param_value_map, current_time);
}

MessagePort* AudioWorkletProcessor::port() const {
  return processor_port_.Get();
}

void AudioWorkletProcessor::Trace(blink::Visitor* visitor) {
  visitor->Trace(global_scope_);
  visitor->Trace(processor_port_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
