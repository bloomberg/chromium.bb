// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webmidi/midi_dispatcher.h"

#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/modules/webmidi/midi_accessor.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {
// The maximum number of bytes which we're allowed to send to the browser
// before getting acknowledgement back from the browser that they've been
// successfully sent.
static const size_t kMaxUnacknowledgedBytesSent = 10 * 1024 * 1024;  // 10 MB.
}  // namespace

MIDIDispatcher::MIDIDispatcher() : binding_(this) {}

MIDIDispatcher::~MIDIDispatcher() = default;

MIDIDispatcher& MIDIDispatcher::Instance() {
  DEFINE_STATIC_LOCAL(Persistent<MIDIDispatcher>, midi_dispatcher,
                      (MakeGarbageCollected<MIDIDispatcher>()));
  return *midi_dispatcher;
}

void MIDIDispatcher::Trace(Visitor* visitor) {}

void MIDIDispatcher::AddAccessor(MIDIAccessor* accessor) {
  TRACE_EVENT0("midi", "MIDIDispatcher::AddAccessor");
  accessors_waiting_session_queue_.push_back(accessor);
  if (session_result_ != midi::mojom::Result::NOT_INITIALIZED) {
    SessionStarted(session_result_);
  } else if (accessors_waiting_session_queue_.size() == 1u) {
    midi::mojom::blink::MidiSessionClientPtr client_ptr;
    binding_.Bind(mojo::MakeRequest(&client_ptr));
    GetMidiSessionProvider().StartSession(mojo::MakeRequest(&midi_session_),
                                          std::move(client_ptr));
  }
}

void MIDIDispatcher::RemoveAccessor(MIDIAccessor* accessor) {
  // |accessor| should either be in |accessors_| or
  // |accessors_waiting_session_queue_|, but not both.
  DCHECK_NE(accessors_.Contains(accessor),
            accessors_waiting_session_queue_.Contains(accessor))
      << "RemoveAccessor call was not balanced with AddAccessor call";
  auto** it = std::find(accessors_.begin(), accessors_.end(), accessor);
  if (it != accessors_.end()) {
    accessors_.erase(it);
  } else {
    accessors_waiting_session_queue_.erase(
        std::find(accessors_waiting_session_queue_.begin(),
                  accessors_waiting_session_queue_.end(), accessor));
  }
  if (accessors_.IsEmpty() && accessors_waiting_session_queue_.IsEmpty()) {
    session_result_ = midi::mojom::Result::NOT_INITIALIZED;
    inputs_.clear();
    outputs_.clear();
    midi_session_.reset();
    midi_session_provider_.reset();
    binding_.Close();
  }
}

void MIDIDispatcher::SendMidiData(uint32_t port,
                                  const uint8_t* data,
                                  wtf_size_t length,
                                  base::TimeTicks timestamp) {
  if ((kMaxUnacknowledgedBytesSent - unacknowledged_bytes_sent_) < length) {
    // TODO(toyoshim): buffer up the data to send at a later time.
    // For now we're just dropping these bytes on the floor.
    return;
  }

  unacknowledged_bytes_sent_ += length;
  Vector<uint8_t> v;
  v.Append(data, length);
  GetMidiSession().SendData(port, std::move(v), timestamp);
}

void MIDIDispatcher::AddInputPort(midi::mojom::blink::PortInfoPtr info) {
  inputs_.push_back(*info);
  // Iterating over a copy of |accessors_| as callback could result in
  // |accessors_| being modified. Applies to for-loops later in this file as
  // well.
  for (auto* accessor : AccessorList(accessors_)) {
    accessor->DidAddInputPort(info->id, info->manufacturer, info->name,
                              info->version, info->state);
  }
}

void MIDIDispatcher::AddOutputPort(midi::mojom::blink::PortInfoPtr info) {
  outputs_.push_back(*info);
  for (auto* accessor : AccessorList(accessors_)) {
    accessor->DidAddOutputPort(info->id, info->manufacturer, info->name,
                               info->version, info->state);
  }
}

void MIDIDispatcher::SetInputPortState(uint32_t port,
                                       midi::mojom::blink::PortState state) {
  if (inputs_[port].state == state)
    return;
  inputs_[port].state = state;
  for (auto* accessor : AccessorList(accessors_))
    accessor->DidSetInputPortState(port, state);
}

void MIDIDispatcher::SetOutputPortState(uint32_t port,
                                        midi::mojom::blink::PortState state) {
  if (outputs_[port].state == state)
    return;
  outputs_[port].state = state;
  for (auto* accessor : AccessorList(accessors_))
    accessor->DidSetOutputPortState(port, state);
}

void MIDIDispatcher::SessionStarted(midi::mojom::blink::Result result) {
  TRACE_EVENT0("midi", "MIDIDispatcher::OnSessionStarted");
  session_result_ = result;

  // A for-loop using iterators does not work because |accessor| may touch
  // |accessors_waiting_session_queue_| in callbacks.
  while (!accessors_waiting_session_queue_.IsEmpty()) {
    auto* accessor = accessors_waiting_session_queue_.back();
    accessors_waiting_session_queue_.pop_back();
    if (result == midi::mojom::blink::Result::OK) {
      // Add the accessor's input and output ports.
      for (const auto& info : inputs_) {
        accessor->DidAddInputPort(info.id, info.manufacturer, info.name,
                                  info.version, info.state);
      }

      for (const auto& info : outputs_) {
        accessor->DidAddOutputPort(info.id, info.manufacturer, info.name,
                                   info.version, info.state);
      }
    }
    accessor->DidStartSession(result);
    accessors_.push_back(accessor);
  }
}

void MIDIDispatcher::AcknowledgeSentData(uint32_t bytes_sent) {
  DCHECK_GE(unacknowledged_bytes_sent_, bytes_sent);
  if (unacknowledged_bytes_sent_ >= bytes_sent)
    unacknowledged_bytes_sent_ -= bytes_sent;
}

void MIDIDispatcher::DataReceived(uint32_t port,
                                  const Vector<uint8_t>& data,
                                  base::TimeTicks timestamp) {
  TRACE_EVENT0("midi", "MIDIDispatcher::DataReceived");
  DCHECK(!data.IsEmpty());

  for (auto* accessor : AccessorList(accessors_))
    accessor->DidReceiveMIDIData(port, &data[0], data.size(), timestamp);
}

midi::mojom::blink::MidiSessionProvider&
MIDIDispatcher::GetMidiSessionProvider() {
  if (!midi_session_provider_) {
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&midi_session_provider_));
  }
  return *midi_session_provider_;
}

midi::mojom::blink::MidiSession& MIDIDispatcher::GetMidiSession() {
  DCHECK(midi_session_);
  return *midi_session_;
}

}  // namespace blink
