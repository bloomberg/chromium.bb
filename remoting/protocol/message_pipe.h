// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MESSAGE_PIPE_H_
#define REMOTING_PROTOCOL_MESSAGE_PIPE_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace remoting {

class CompoundBuffer;

namespace protocol {

// Represents a bi-directional pipe that allows to send and receive messages.
class MessagePipe {
 public:
  typedef base::Callback<void(scoped_ptr<CompoundBuffer> message)>
      MessageReceivedCallback;

  virtual ~MessagePipe() {}

  // Starts receiving incoming messages and calls |callback| for each message.
  virtual void StartReceiving(const MessageReceivedCallback& callback) = 0;

  // Sends a message. |done| is called when the message has been sent to the
  // client, but it doesn't mean that the client has received it. |done| is
  // never called if the message is never sent (e.g. if the pipe is destroyed
  // before the message is sent).
  virtual void Send(google::protobuf::MessageLite* message,
                    const base::Closure& done) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MESSAGE_PIPE_H_
