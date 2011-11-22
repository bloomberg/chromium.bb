// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_EVENT_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_EVENT_DISPATCHER_H_

#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/message_reader.h"

namespace remoting {
namespace protocol {

class EventMessage;
class InputStub;

// HostEventDispatcher dispatches incoming messages on the event
// channel to InputStub.
class HostEventDispatcher : public ChannelDispatcherBase {
 public:
  typedef base::Callback<void(int64)> SequenceNumberCallback;

  HostEventDispatcher();
  virtual ~HostEventDispatcher();

  // Set InputStub that will be called for each incoming input
  // message. Doesn't take ownership of |input_stub|. It must outlive
  // the dispatcher.
  void set_input_stub(InputStub* input_stub) { input_stub_ = input_stub; }

  // Set callback to notify of each message's sequence number. The
  // callback cannot tear down this object.
  void set_sequence_number_callback(const SequenceNumberCallback& value) {
    sequence_number_callback_ = value;
  }

 protected:
  // ChannelDispatcherBase overrides.
  virtual void OnInitialized() OVERRIDE;

 private:
  void OnMessageReceived(EventMessage* message,
                         const base::Closure& done_task);

  InputStub* input_stub_;
  SequenceNumberCallback sequence_number_callback_;

  ProtobufMessageReader<EventMessage> reader_;

  DISALLOW_COPY_AND_ASSIGN(HostEventDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_EVENT_DISPATCHER_H_
