// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_INPUT_DISPATCHER_H_
#define REMOTING_PROTOCOL_CLIENT_INPUT_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "remoting/protocol/input_stub.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {
namespace protocol {

class BufferedSocketWriter;
class Session;

// ClientEventDispatcher manages the event channel on the client
// side. It implements InputStub for outgoing input messages.
class ClientEventDispatcher : public InputStub {
 public:
  ClientEventDispatcher();
  virtual ~ClientEventDispatcher();

  // Initialize the event channel and the dispatcher for the
  // |session|.
  void Init(Session* session);

  // InputStub implementation.
  virtual void InjectKeyEvent(const KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const MouseEvent& event) OVERRIDE;

 private:
  scoped_refptr<BufferedSocketWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(ClientEventDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_INPUT_DISPATCHER_H_
