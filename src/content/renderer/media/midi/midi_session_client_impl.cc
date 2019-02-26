// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/midi/midi_session_client_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_logging.h"
#include "services/service_manager/public/cpp/connector.h"

using base::AutoLock;
using blink::WebString;
using midi::mojom::PortState;

// The maximum number of bytes which we're allowed to send to the browser
// before getting acknowledgement back from the browser that they've been
// successfully sent.
static const size_t kMaxUnacknowledgedBytesSent = 10 * 1024 * 1024;  // 10 MB.

namespace content {

MidiSessionClientImpl::MidiSessionClientImpl()
    : session_result_(midi::mojom::Result::NOT_INITIALIZED),
      unacknowledged_bytes_sent_(0u),
      binding_(this) {}

MidiSessionClientImpl::~MidiSessionClientImpl() {}

void MidiSessionClientImpl::AddInputPort(midi::mojom::PortInfoPtr info) {
  inputs_.push_back(*info);
  const WebString id = WebString::FromUTF8(info->id);
  const WebString manufacturer = WebString::FromUTF8(info->manufacturer);
  const WebString name = WebString::FromUTF8(info->name);
  const WebString version = WebString::FromUTF8(info->version);
  for (auto* client : clients_)
    client->DidAddInputPort(id, manufacturer, name, version, info->state);
}

void MidiSessionClientImpl::AddOutputPort(midi::mojom::PortInfoPtr info) {
  outputs_.push_back(*info);
  const WebString id = WebString::FromUTF8(info->id);
  const WebString manufacturer = WebString::FromUTF8(info->manufacturer);
  const WebString name = WebString::FromUTF8(info->name);
  const WebString version = WebString::FromUTF8(info->version);
  for (auto* client : clients_)
    client->DidAddOutputPort(id, manufacturer, name, version, info->state);
}

void MidiSessionClientImpl::SetInputPortState(uint32_t port,
                                              midi::mojom::PortState state) {
  if (inputs_[port].state == state)
    return;
  inputs_[port].state = state;
  for (auto* client : clients_)
    client->DidSetInputPortState(port, state);
}

void MidiSessionClientImpl::SetOutputPortState(uint32_t port,
                                               midi::mojom::PortState state) {
  if (outputs_[port].state == state)
    return;
  outputs_[port].state = state;
  for (auto* client : clients_)
    client->DidSetOutputPortState(port, state);
}

void MidiSessionClientImpl::SessionStarted(midi::mojom::Result result) {
  TRACE_EVENT0("midi", "MidiSessionClientImpl::OnSessionStarted");
  session_result_ = result;

  // A for-loop using iterators does not work because |client| may touch
  // |clients_waiting_session_queue_| in callbacks.
  while (!clients_waiting_session_queue_.empty()) {
    auto* client = clients_waiting_session_queue_.back();
    clients_waiting_session_queue_.pop_back();
    if (result == midi::mojom::Result::OK) {
      // Add the client's input and output ports.
      for (const auto& info : inputs_) {
        client->DidAddInputPort(WebString::FromUTF8(info.id),
                                WebString::FromUTF8(info.manufacturer),
                                WebString::FromUTF8(info.name),
                                WebString::FromUTF8(info.version), info.state);
      }

      for (const auto& info : outputs_) {
        client->DidAddOutputPort(WebString::FromUTF8(info.id),
                                 WebString::FromUTF8(info.manufacturer),
                                 WebString::FromUTF8(info.name),
                                 WebString::FromUTF8(info.version), info.state);
      }
    }
    client->DidStartSession(result);
    clients_.insert(client);
  }
}

void MidiSessionClientImpl::AcknowledgeSentData(uint32_t bytes_sent) {
  DCHECK_GE(unacknowledged_bytes_sent_, bytes_sent);
  if (unacknowledged_bytes_sent_ >= bytes_sent)
    unacknowledged_bytes_sent_ -= bytes_sent;
}

void MidiSessionClientImpl::DataReceived(uint32_t port,
                                         const std::vector<uint8_t>& data,
                                         base::TimeTicks timestamp) {
  TRACE_EVENT0("midi", "MidiSessionClientImpl::OnDataReceived");
  DCHECK(!data.empty());

  for (auto* client : clients_)
    client->DidReceiveMIDIData(port, &data[0], data.size(), timestamp);
}

void MidiSessionClientImpl::AddClient(blink::WebMIDIAccessorClient* client) {
  TRACE_EVENT0("midi", "MidiSessionClientImpl::AddClient");
  clients_waiting_session_queue_.push_back(client);
  if (session_result_ != midi::mojom::Result::NOT_INITIALIZED) {
    SessionStarted(session_result_);
  } else if (clients_waiting_session_queue_.size() == 1u) {
    midi::mojom::MidiSessionClientPtr client_ptr;
    binding_.Bind(mojo::MakeRequest(&client_ptr));
    midi::mojom::MidiSessionRequest request = mojo::MakeRequest(&midi_session_);
    GetMidiSessionProvider().StartSession(std::move(request),
                                          std::move(client_ptr));
  }
}

void MidiSessionClientImpl::RemoveClient(blink::WebMIDIAccessorClient* client) {
  DCHECK(clients_.find(client) != clients_.end() ||
         base::ContainsValue(clients_waiting_session_queue_, client))
      << "RemoveClient call was not ballanced with AddClient call";
  clients_.erase(client);
  auto it = std::find(clients_waiting_session_queue_.begin(),
                      clients_waiting_session_queue_.end(), client);
  if (it != clients_waiting_session_queue_.end())
    clients_waiting_session_queue_.erase(it);
  if (clients_.empty() && clients_waiting_session_queue_.empty()) {
    session_result_ = midi::mojom::Result::NOT_INITIALIZED;
    inputs_.clear();
    outputs_.clear();
    midi_session_.reset();
    binding_.Close();
  }
}

void MidiSessionClientImpl::SendMidiData(uint32_t port,
                                         const uint8_t* data,
                                         size_t length,
                                         base::TimeTicks timestamp) {
  if ((kMaxUnacknowledgedBytesSent - unacknowledged_bytes_sent_) < length) {
    // TODO(toyoshim): buffer up the data to send at a later time.
    // For now we're just dropping these bytes on the floor.
    return;
  }

  unacknowledged_bytes_sent_ += length;
  std::vector<uint8_t> v(data, data + length);
  GetMidiSession().SendData(port, v, timestamp);
}

midi::mojom::MidiSession& MidiSessionClientImpl::GetMidiSession() {
  DCHECK(midi_session_);
  return *midi_session_;
}

midi::mojom::MidiSessionProvider&
MidiSessionClientImpl::GetMidiSessionProvider() {
  if (!midi_session_provider_) {
    ChildThreadImpl::current()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName, mojo::MakeRequest(&midi_session_provider_));
  }
  return *midi_session_provider_;
}

}  // namespace content
