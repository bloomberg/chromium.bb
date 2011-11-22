// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_INPUT_DISPATCHER_H_
#define REMOTING_PROTOCOL_CLIENT_INPUT_DISPATCHER_H_

#include "base/memory/ref_counted.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {
namespace protocol {

class BufferedSocketWriter;

// ClientEventDispatcher manages the event channel on the client
// side. It implements InputStub for outgoing input messages.
class ClientEventDispatcher : public ChannelDispatcherBase, public InputStub {
 public:
  ClientEventDispatcher();
  virtual ~ClientEventDispatcher();

  // InputStub implementation.
  virtual void InjectKeyEvent(const KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const MouseEvent& event) OVERRIDE;

 protected:
  // ChannelDispatcherBase overrides.
  virtual void OnInitialized() OVERRIDE;

 private:
  scoped_refptr<BufferedSocketWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(ClientEventDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_INPUT_DISPATCHER_H_
