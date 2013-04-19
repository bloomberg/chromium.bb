// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_control_dispatcher.h"

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/message_loop_proxy.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

ClientControlDispatcher::ClientControlDispatcher()
    : ChannelDispatcherBase(kControlChannelName),
      client_stub_(NULL),
      clipboard_stub_(NULL) {
}

ClientControlDispatcher::~ClientControlDispatcher() {
  writer_.Close();
}

void ClientControlDispatcher::OnInitialized() {
  // TODO(garykac): Set write failed callback.
  writer_.Init(channel(), BufferedSocketWriter::WriteFailedCallback());
  reader_.Init(channel(), base::Bind(
      &ClientControlDispatcher::OnMessageReceived, base::Unretained(this)));
}

void ClientControlDispatcher::InjectClipboardEvent(
    const ClipboardEvent& event) {
  ControlMessage message;
  message.mutable_clipboard_event()->CopyFrom(event);
  writer_.Write(SerializeAndFrameMessage(message), base::Closure());
}

void ClientControlDispatcher::NotifyClientResolution(
    const ClientResolution& resolution) {
  ControlMessage message;
  message.mutable_client_resolution()->CopyFrom(resolution);
  writer_.Write(SerializeAndFrameMessage(message), base::Closure());
}

void ClientControlDispatcher::ControlVideo(const VideoControl& video_control) {
  ControlMessage message;
  message.mutable_video_control()->CopyFrom(video_control);
  writer_.Write(SerializeAndFrameMessage(message), base::Closure());
}

void ClientControlDispatcher::ControlAudio(const AudioControl& audio_control) {
  ControlMessage message;
  message.mutable_audio_control()->CopyFrom(audio_control);
  writer_.Write(SerializeAndFrameMessage(message), base::Closure());
}

void ClientControlDispatcher::SetCapabilities(
    const Capabilities& capabilities) {
  ControlMessage message;
  message.mutable_capabilities()->CopyFrom(capabilities);
  writer_.Write(SerializeAndFrameMessage(message), base::Closure());
}

void ClientControlDispatcher::OnMessageReceived(
    scoped_ptr<ControlMessage> message, const base::Closure& done_task) {
  DCHECK(client_stub_);
  DCHECK(clipboard_stub_);
  base::ScopedClosureRunner done_runner(done_task);

  if (message->has_clipboard_event()) {
    clipboard_stub_->InjectClipboardEvent(message->clipboard_event());
  } else if (message->has_capabilities()) {
    client_stub_->SetCapabilities(message->capabilities());
  } else if (message->has_cursor_shape()) {
    client_stub_->SetCursorShape(message->cursor_shape());
  } else {
    LOG(WARNING) << "Unknown control message received.";
  }
}

}  // namespace protocol
}  // namespace remoting
