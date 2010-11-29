// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_STREAM_WRITER_H_
#define REMOTING_PROTOCOL_STREAM_WRITER_H_

#include "base/ref_counted.h"
#include "remoting/proto/internal.pb.h"

namespace net {
class Socket;
}  // namespace net

namespace remoting {
namespace protocol {

class BufferedSocketWriter;

class StreamWriterBase {
 public:
  StreamWriterBase();
  virtual ~StreamWriterBase();

  // Initializes the writer. Must be called on the thread the |socket| belongs
  // to.
  void Init(net::Socket* socket);

  // Return current buffer state. Can be called from any thread.
  int GetBufferSize();
  int GetPendingMessages();

  // Stop writing and drop pending data. Must be called from the same thread as
  // Init().
  void Close();

 protected:
  net::Socket* socket_;
  scoped_refptr<BufferedSocketWriter> buffered_writer_;
};

class EventStreamWriter : public StreamWriterBase {
 public:
  // Sends the |message| or returns false if called before Init().
  // Can be called on any thread.
  bool SendMessage(const EventMessage& message);
};

class ControlStreamWriter : public StreamWriterBase {
 public:
  // Sends the |message| or returns false if called before Init().
  // Can be called on any thread.
  bool SendMessage(const ControlMessage& message);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_STREAM_WRITER_H_
