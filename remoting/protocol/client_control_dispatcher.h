// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_CONTROL_DISPATCHER_H_
#define REMOTING_PROTOCOL_CLIENT_CONTROL_DISPATCHER_H_

#include "base/memory/ref_counted.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/message_reader.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {
namespace protocol {

class ClientStub;
class ControlMessage;
class BufferedSocketWriter;
class Session;

// ClientControlDispatcher dispatches incoming messages on the control
// channel to ClientStub, and also implements HostStub for outgoing
// messages.
class ClientControlDispatcher : public ChannelDispatcherBase, public HostStub {
 public:
  ClientControlDispatcher();
  virtual ~ClientControlDispatcher();

  // Sets ClientStub that will be called for each incoming control
  // message. Doesn't take ownership of |client_stub|. It must outlive
  // this dispatcher.
  void set_client_stub(ClientStub* client_stub) { client_stub_ = client_stub; }

 protected:
  // ChannelDispatcherBase overrides.
  virtual void OnInitialized() OVERRIDE;

 private:
  void OnMessageReceived(ControlMessage* message,
                         const base::Closure& done_task);

  ClientStub* client_stub_;

  ProtobufMessageReader<ControlMessage> reader_;
  scoped_refptr<BufferedSocketWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_CONTROL_DISPATCHER_H_
