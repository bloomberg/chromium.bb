// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_control_dispatcher.h"

#include "base/callback_helpers.h"
#include "base/message_loop/message_loop_proxy.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {
namespace protocol {

HostControlDispatcher::HostControlDispatcher()
    : ChannelDispatcherBase(kControlChannelName),
      clipboard_stub_(NULL),
      host_stub_(NULL) {
}

HostControlDispatcher::~HostControlDispatcher() {
  writer_.Close();
}

void HostControlDispatcher::OnInitialized() {
  reader_.Init(channel(), base::Bind(
      &HostControlDispatcher::OnMessageReceived, base::Unretained(this)));
  writer_.Init(channel(), BufferedSocketWriter::WriteFailedCallback());
}

void HostControlDispatcher::SetCapabilities(
    const Capabilities& capabilities) {
  ControlMessage message;
  message.mutable_capabilities()->CopyFrom(capabilities);
  writer_.Write(SerializeAndFrameMessage(message), base::Closure());
}

void HostControlDispatcher::SetPairingResponse(
    const PairingResponse& pairing_response) {
  ControlMessage message;
  message.mutable_pairing_response()->CopyFrom(pairing_response);
  writer_.Write(SerializeAndFrameMessage(message), base::Closure());
}

void HostControlDispatcher::DeliverHostMessage(
    const ExtensionMessage& message) {
  ControlMessage control_message;
  control_message.mutable_extension_message()->CopyFrom(message);
  writer_.Write(SerializeAndFrameMessage(control_message), base::Closure());
}

void HostControlDispatcher::InjectClipboardEvent(const ClipboardEvent& event) {
  ControlMessage message;
  message.mutable_clipboard_event()->CopyFrom(event);
  writer_.Write(SerializeAndFrameMessage(message), base::Closure());
}

void HostControlDispatcher::SetCursorShape(
    const CursorShapeInfo& cursor_shape) {
  ControlMessage message;
  message.mutable_cursor_shape()->CopyFrom(cursor_shape);
  writer_.Write(SerializeAndFrameMessage(message), base::Closure());
}

void HostControlDispatcher::OnMessageReceived(
    scoped_ptr<ControlMessage> message, const base::Closure& done_task) {
  DCHECK(clipboard_stub_);
  DCHECK(host_stub_);

  base::ScopedClosureRunner done_runner(done_task);

  if (message->has_clipboard_event()) {
    clipboard_stub_->InjectClipboardEvent(message->clipboard_event());
  } else if (message->has_client_resolution()) {
    host_stub_->NotifyClientResolution(message->client_resolution());
  } else if (message->has_video_control()) {
    host_stub_->ControlVideo(message->video_control());
  } else if (message->has_audio_control()) {
    host_stub_->ControlAudio(message->audio_control());
  } else if (message->has_capabilities()) {
    host_stub_->SetCapabilities(message->capabilities());
  } else if (message->has_pairing_request()) {
    host_stub_->RequestPairing(message->pairing_request());
  } else if (message->has_extension_message()) {
    host_stub_->DeliverClientMessage(message->extension_message());
  } else {
    LOG(WARNING) << "Unknown control message received.";
  }
}

}  // namespace protocol
}  // namespace remoting
