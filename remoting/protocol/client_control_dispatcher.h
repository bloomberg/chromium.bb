// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_CONTROL_DISPATCHER_H_
#define REMOTING_PROTOCOL_CLIENT_CONTROL_DISPATCHER_H_

#include "base/memory/ref_counted.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/message_reader.h"

namespace remoting {
namespace protocol {

class ClientStub;
class ControlMessage;
class Session;

// ClientControlDispatcher dispatches incoming messages on the control
// channel to ClientStub, ClipboardStub or CursorShapeStub.
// It also implements ClipboardStub and HostStub for outgoing messages.
class ClientControlDispatcher : public ChannelDispatcherBase,
                                public ClipboardStub,
                                public HostStub {
 public:
  ClientControlDispatcher();
  virtual ~ClientControlDispatcher();

  // ClipboardStub implementation.
  virtual void InjectClipboardEvent(const ClipboardEvent& event) OVERRIDE;

  // HostStub implementation.
  virtual void NotifyClientResolution(
      const ClientResolution& resolution) OVERRIDE;
  virtual void ControlVideo(const VideoControl& video_control) OVERRIDE;
  virtual void ControlAudio(const AudioControl& audio_control) OVERRIDE;
  virtual void SetCapabilities(const Capabilities& capabilities) OVERRIDE;
  virtual void RequestPairing(const PairingRequest& pairing_request) OVERRIDE;
  virtual void DeliverClientMessage(const ExtensionMessage& message) OVERRIDE;

  // Sets the ClientStub that will be called for each incoming control
  // message. |client_stub| must outlive this object.
  void set_client_stub(ClientStub* client_stub) { client_stub_ = client_stub; }

  // Sets the ClipboardStub that will be called for each incoming clipboard
  // message. |clipboard_stub| must outlive this object.
  void set_clipboard_stub(ClipboardStub* clipboard_stub) {
    clipboard_stub_ = clipboard_stub;
  }

 protected:
  // ChannelDispatcherBase overrides.
  virtual void OnInitialized() OVERRIDE;

 private:
  void OnMessageReceived(scoped_ptr<ControlMessage> message,
                         const base::Closure& done_task);

  ClientStub* client_stub_;
  ClipboardStub* clipboard_stub_;

  ProtobufMessageReader<ControlMessage> reader_;
  BufferedSocketWriter writer_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_CONTROL_DISPATCHER_H_
