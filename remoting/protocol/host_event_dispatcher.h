// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_EVENT_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_EVENT_DISPATCHER_H_

#include <stdint.h>

#include "base/macros.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/protobuf_message_parser.h"

namespace remoting {
namespace protocol {

class EventMessage;
class InputStub;

// HostEventDispatcher dispatches incoming messages on the event
// channel to InputStub.
class HostEventDispatcher : public ChannelDispatcherBase {
 public:
  typedef base::Callback<void(int64_t)> OnInputEventCallback;

  HostEventDispatcher();
  ~HostEventDispatcher() override;

  // Set InputStub that will be called for each incoming input
  // message. Doesn't take ownership of |input_stub|. It must outlive
  // the dispatcher.
  void set_input_stub(InputStub* input_stub) { input_stub_ = input_stub; }

  // Set callback to notify of each message's sequence number. The
  // callback cannot tear down this object.
  void set_on_input_event_callback(const OnInputEventCallback& value) {
    on_input_event_callback_ = value;
  }

 private:
  void OnMessageReceived(scoped_ptr<EventMessage> message);

  InputStub* input_stub_;
  OnInputEventCallback on_input_event_callback_;

  ProtobufMessageParser<EventMessage> parser_;

  DISALLOW_COPY_AND_ASSIGN(HostEventDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_EVENT_DISPATCHER_H_
