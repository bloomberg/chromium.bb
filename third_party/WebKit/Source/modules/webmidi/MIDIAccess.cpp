/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webmidi/MIDIAccess.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "modules/webmidi/MIDIAccessInitializer.h"
#include "modules/webmidi/MIDIConnectionEvent.h"
#include "modules/webmidi/MIDIInput.h"
#include "modules/webmidi/MIDIInputMap.h"
#include "modules/webmidi/MIDIOutput.h"
#include "modules/webmidi/MIDIOutputMap.h"
#include "modules/webmidi/MIDIPort.h"
#include "platform/AsyncMethodRunner.h"
#include <memory>

namespace blink {

namespace {

using midi::mojom::PortState;

// Since "open" status is separately managed per MIDIAccess instance, we do not
// expose service level PortState directly.
PortState ToDeviceState(PortState state) {
  if (state == PortState::OPENED)
    return PortState::CONNECTED;
  return state;
}

}  // namespace

MIDIAccess::MIDIAccess(
    std::unique_ptr<MIDIAccessor> accessor,
    bool sysex_enabled,
    const Vector<MIDIAccessInitializer::PortDescriptor>& ports,
    ExecutionContext* execution_context)
    : ContextLifecycleObserver(execution_context),
      accessor_(std::move(accessor)),
      sysex_enabled_(sysex_enabled),
      has_pending_activity_(false) {
  accessor_->SetClient(this);
  for (size_t i = 0; i < ports.size(); ++i) {
    const MIDIAccessInitializer::PortDescriptor& port = ports[i];
    if (port.type == MIDIPort::kTypeInput) {
      inputs_.push_back(MIDIInput::Create(this, port.id, port.manufacturer,
                                          port.name, port.version,
                                          ToDeviceState(port.state)));
    } else {
      outputs_.push_back(MIDIOutput::Create(
          this, outputs_.size(), port.id, port.manufacturer, port.name,
          port.version, ToDeviceState(port.state)));
    }
  }
}

MIDIAccess::~MIDIAccess() {}

void MIDIAccess::Dispose() {
  accessor_.reset();
}

EventListener* MIDIAccess::onstatechange() {
  return GetAttributeEventListener(EventTypeNames::statechange);
}

void MIDIAccess::setOnstatechange(EventListener* listener) {
  has_pending_activity_ = listener;
  SetAttributeEventListener(EventTypeNames::statechange, listener);
}

bool MIDIAccess::HasPendingActivity() const {
  return has_pending_activity_ && GetExecutionContext() &&
         !GetExecutionContext()->IsContextDestroyed();
}

MIDIInputMap* MIDIAccess::inputs() const {
  HeapVector<Member<MIDIInput>> inputs;
  HashSet<String> ids;
  for (size_t i = 0; i < inputs_.size(); ++i) {
    MIDIInput* input = inputs_[i];
    if (input->GetState() != PortState::DISCONNECTED) {
      inputs.push_back(input);
      ids.insert(input->id());
    }
  }
  if (inputs.size() != ids.size()) {
    // There is id duplication that violates the spec.
    inputs.clear();
  }
  return new MIDIInputMap(inputs);
}

MIDIOutputMap* MIDIAccess::outputs() const {
  HeapVector<Member<MIDIOutput>> outputs;
  HashSet<String> ids;
  for (size_t i = 0; i < outputs_.size(); ++i) {
    MIDIOutput* output = outputs_[i];
    if (output->GetState() != PortState::DISCONNECTED) {
      outputs.push_back(output);
      ids.insert(output->id());
    }
  }
  if (outputs.size() != ids.size()) {
    // There is id duplication that violates the spec.
    outputs.clear();
  }
  return new MIDIOutputMap(outputs);
}

void MIDIAccess::DidAddInputPort(const String& id,
                                 const String& manufacturer,
                                 const String& name,
                                 const String& version,
                                 PortState state) {
  DCHECK(IsMainThread());
  MIDIInput* port = MIDIInput::Create(this, id, manufacturer, name, version,
                                      ToDeviceState(state));
  inputs_.push_back(port);
  DispatchEvent(MIDIConnectionEvent::Create(port));
}

void MIDIAccess::DidAddOutputPort(const String& id,
                                  const String& manufacturer,
                                  const String& name,
                                  const String& version,
                                  PortState state) {
  DCHECK(IsMainThread());
  unsigned port_index = outputs_.size();
  MIDIOutput* port = MIDIOutput::Create(this, port_index, id, manufacturer,
                                        name, version, ToDeviceState(state));
  outputs_.push_back(port);
  DispatchEvent(MIDIConnectionEvent::Create(port));
}

void MIDIAccess::DidSetInputPortState(unsigned port_index, PortState state) {
  DCHECK(IsMainThread());
  if (port_index >= inputs_.size())
    return;

  PortState device_state = ToDeviceState(state);
  if (inputs_[port_index]->GetState() != device_state)
    inputs_[port_index]->SetState(device_state);
}

void MIDIAccess::DidSetOutputPortState(unsigned port_index, PortState state) {
  DCHECK(IsMainThread());
  if (port_index >= outputs_.size())
    return;

  PortState device_state = ToDeviceState(state);
  if (outputs_[port_index]->GetState() != device_state)
    outputs_[port_index]->SetState(device_state);
}

void MIDIAccess::DidReceiveMIDIData(unsigned port_index,
                                    const unsigned char* data,
                                    size_t length,
                                    double time_stamp) {
  DCHECK(IsMainThread());
  if (port_index >= inputs_.size())
    return;

  inputs_[port_index]->DidReceiveMIDIData(port_index, data, length, time_stamp);
}

void MIDIAccess::SendMIDIData(unsigned port_index,
                              const unsigned char* data,
                              size_t length,
                              double time_stamp_in_milliseconds) {
  // Do not continue sending when document is going to be closed.
  Document* document = ToDocument(GetExecutionContext());
  DCHECK(document);
  DocumentLoader* loader = document->Loader();
  if (!loader)
    return;

  if (!data || !length || port_index >= outputs_.size())
    return;

  // Convert from a time in milliseconds (a DOMHighResTimeStamp) according to
  // the same time coordinate system as performance.now() into a time in seconds
  // which is based on the time coordinate system of
  // monotonicallyIncreasingTime().
  double time_stamp;

  if (!time_stamp_in_milliseconds) {
    // We treat a value of 0 (which is the default value) as special, meaning
    // "now".  We need to translate it exactly to 0 seconds.
    time_stamp = 0;
  } else {
    double document_start_time = loader->GetTiming().ReferenceMonotonicTime();
    time_stamp = document_start_time + 0.001 * time_stamp_in_milliseconds;
  }

  accessor_->SendMIDIData(port_index, data, length, time_stamp);
}

void MIDIAccess::ContextDestroyed(ExecutionContext*) {
  accessor_.reset();
}

void MIDIAccess::Trace(blink::Visitor* visitor) {
  visitor->Trace(inputs_);
  visitor->Trace(outputs_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
