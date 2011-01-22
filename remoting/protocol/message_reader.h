// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MESSAGE_READER_H_
#define REMOTING_PROTOCOL_MESSAGE_READER_H_

#include "base/callback.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "net/base/completion_callback.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/message_decoder.h"

class MessageLoop;

namespace net {
class IOBuffer;
class Socket;
}  // namespace net

namespace remoting {
namespace protocol {

// MessageReader reads data from the socket asynchronously and calls
// callback for each message it receives. It stops calling the
// callback as soon as the socket is closed, so the socket should
// always be closed before the callback handler is destroyed.
//
// In order to throttle the stream, MessageReader doesn't try to read
// new data from the socket until all previously received messages are
// processed by the receiver (|done_task| is called for each message).
// It is still possible that the MessageReceivedCallback is called
// twice (so that there is more than one outstanding message),
// e.g. when we the sender sends multiple messages in one TCP packet.
class MessageReader : public base::RefCountedThreadSafe<MessageReader> {
 public:
  // The callback is given ownership of the second argument
  // (|done_task|).  The buffer (first argument) is owned by
  // MessageReader and is freed when the task specified by the second
  // argument is called.
  typedef Callback2<CompoundBuffer*, Task*>::Type MessageReceivedCallback;

  MessageReader();
  virtual ~MessageReader();

  // Initialize the MessageReader with a socket. If a message is received
  // |callback| is called.
  void Init(net::Socket* socket, MessageReceivedCallback* callback);

 private:
  void DoRead();
  void OnRead(int result);
  void HandleReadResult(int result);
  void OnDataReceived(net::IOBuffer* data, int data_size);
  void OnMessageDone(CompoundBuffer* message);
  void ProcessDoneEvent();

  net::Socket* socket_;

  // The network message loop this object runs on.
  MessageLoop* message_loop_;

  // Set to true, when we have a socket read pending, and expecting
  // OnRead() to be called when new data is received.
  bool read_pending_;

  // Number of messages that we received, but haven't finished
  // processing yet, i.e. |done_task| hasn't been called for these
  // messages.
  int pending_messages_;

  bool closed_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  net::CompletionCallbackImpl<MessageReader> read_callback_;

  MessageDecoder message_decoder_;

  // Callback is called when a message is received.
  scoped_ptr<MessageReceivedCallback> message_received_callback_;
};

// Version of MessageReader for protocol buffer messages, that parses
// each incoming message.
template <class T>
class ProtobufMessageReader {
 public:
  typedef typename Callback2<T*, Task*>::Type MessageReceivedCallback;

  ProtobufMessageReader() { };
  ~ProtobufMessageReader() { };

  void Init(net::Socket* socket, MessageReceivedCallback* callback) {
    message_received_callback_.reset(callback);
    message_reader_ = new MessageReader();
    message_reader_->Init(
        socket, NewCallback(this, &ProtobufMessageReader<T>::OnNewData));
  }

 private:
  void OnNewData(CompoundBuffer* buffer, Task* done_task) {
    T* message = new T();
    CompoundBufferInputStream stream(buffer);
    bool ret = message->ParseFromZeroCopyStream(&stream);
    if (!ret) {
      LOG(WARNING) << "Received message that is not a valid protocol buffer.";
      delete message;
    } else {
      DCHECK_EQ(stream.position(), buffer->total_bytes());
      message_received_callback_->Run(
          message, NewRunnableFunction(
              &ProtobufMessageReader<T>::OnDone, message, done_task));
    }
  }

  static void OnDone(T* message, Task* done_task) {
    delete message;
    done_task->Run();
    delete done_task;
  }

  scoped_refptr<MessageReader> message_reader_;
  scoped_ptr<MessageReceivedCallback> message_received_callback_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MESSAGE_READER_H_
