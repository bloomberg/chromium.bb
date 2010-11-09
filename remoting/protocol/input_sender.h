// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of InputStub using sockets created from jingle connection.
// It sends messages through the socket after serializing it.
//
// Object of this class can only be created by ConnectionToHost.
//
// This class can be used on any thread. An object of socket is given to this
// class, its lifetime is strictly greater than this object.

#ifndef REMOTING_PROTOCOL_INPUT_SENDER_H_
#define REMOTING_PROTOCOL_INPUT_SENDER_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "remoting/protocol/input_stub.h"

class Task;

namespace net {
class Socket;
}  // namespace net

namespace remoting {
namespace protocol {

class BufferedSocketWriter;

class InputSender : public InputStub {
 public:
  // Create a stub using a socket.
  explicit InputSender(net::Socket* socket);
  virtual ~InputSender();

  // InputStub implementation.
  virtual void InjectKeyEvent(const KeyEvent* event, Task* done);
  virtual void InjectMouseEvent(const MouseEvent* event, Task* done);

 private:
  // Helper method to run the task and delete it afterwards.
  void RunTask(Task* done);

  // Buffered socket writer holds the serialized message and sends it on the
  // right thread.
  scoped_refptr<BufferedSocketWriter> buffered_writer_;

  DISALLOW_COPY_AND_ASSIGN(InputSender);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_INPUT_SENDER_H_
