// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_CONTROL_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_CONTROL_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/message_reader.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {
namespace protocol {

class BufferedSocketWriter;
class ControlMessage;
class HostStub;
class Session;

// HostControlDispatcher dispatches incoming messages on the control
// channel to HostStub, and also implements ClientStub for outgoing
// messages.
class HostControlDispatcher : public ClientStub {
 public:
  HostControlDispatcher();
  virtual ~HostControlDispatcher();

  // Initialize the control channel and the dispatcher for the
  // |session|. Doesn't take ownership of |session|.
  void Init(Session* session);

  // Sets HostStub that will be called for each incoming control
  // message. Doesn't take ownership of |host_stub|. It must outlive
  // this dispatcher.
  void set_host_stub(HostStub* host_stub) { host_stub_ = host_stub; }

 private:
  // This method is called by |reader_| when a message is received.
  void OnMessageReceived(ControlMessage* message,
                         const base::Closure& done_task);

  HostStub* host_stub_;

  ProtobufMessageReader<ControlMessage> reader_;
  scoped_refptr<BufferedSocketWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(HostControlDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_CONTROL_DISPATCHER_H_
