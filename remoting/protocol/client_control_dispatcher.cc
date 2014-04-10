// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_control_dispatcher.h"

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop_proxy.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {
namespace protocol {

namespace {

// 32-bit BGRA is 4 bytes per pixel.
const int kBytesPerPixel = 4;

bool CursorShapeIsValid(const CursorShapeInfo& cursor_shape) {
  if (!cursor_shape.has_data() ||
      !cursor_shape.has_width() ||
      !cursor_shape.has_height() ||
      !cursor_shape.has_hotspot_x() ||
      !cursor_shape.has_hotspot_y()) {
    LOG(ERROR) << "Cursor shape is missing required fields.";
    return false;
  }

  int width = cursor_shape.width();
  int height = cursor_shape.height();

  // Verify that |width| and |height| are within sane limits. Otherwise integer
  // overflow can occur while calculating |cursor_total_bytes| below.
  if (width <= 0 || width > (SHRT_MAX / 2) ||
      height <= 0 || height > (SHRT_MAX / 2)) {
    LOG(ERROR) << "Cursor dimensions are out of bounds for SetCursor: "
               << width << "x" << height;
    return false;
  }

  uint32 cursor_total_bytes = width * height * kBytesPerPixel;
  if (cursor_shape.data().size() < cursor_total_bytes) {
    LOG(ERROR) << "Expected " << cursor_total_bytes << " bytes for a "
               << width << "x" << height << " cursor. Only received "
               << cursor_shape.data().size() << " bytes";
    return false;
  }

  return true;
}

}  // namespace

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

void ClientControlDispatcher::RequestPairing(
    const PairingRequest& pairing_request) {
  ControlMessage message;
  message.mutable_pairing_request()->CopyFrom(pairing_request);
  writer_.Write(SerializeAndFrameMessage(message), base::Closure());
}

void ClientControlDispatcher::DeliverClientMessage(
    const ExtensionMessage& message) {
  ControlMessage control_message;
  control_message.mutable_extension_message()->CopyFrom(message);
  writer_.Write(SerializeAndFrameMessage(control_message), base::Closure());
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
    if (CursorShapeIsValid(message->cursor_shape()))
      client_stub_->SetCursorShape(message->cursor_shape());
  } else if (message->has_pairing_response()) {
    client_stub_->SetPairingResponse(message->pairing_response());
  } else if (message->has_extension_message()) {
    client_stub_->DeliverHostMessage(message->extension_message());
  } else {
    LOG(WARNING) << "Unknown control message received.";
  }
}

}  // namespace protocol
}  // namespace remoting
