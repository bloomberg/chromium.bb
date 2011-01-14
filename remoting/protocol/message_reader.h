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

namespace net {
class IOBuffer;
class Socket;
}  // namespace net

namespace remoting {
namespace protocol {

// MessageReader reads data from the socket asynchronously and calls
// callback for each message it receives
class MessageReader {
 public:
  typedef Callback1<CompoundBuffer*>::Type MessageReceivedCallback;

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

  net::Socket* socket_;

  bool closed_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  net::CompletionCallbackImpl<MessageReader> read_callback_;

  MessageDecoder message_decoder_;

  // Callback is called when a message is received.
  scoped_ptr<MessageReceivedCallback> message_received_callback_;
};

template <class T>
class ProtobufMessageReader {
 public:
  typedef typename Callback1<T*>::Type MessageReceivedCallback;

  ProtobufMessageReader() { };
  ~ProtobufMessageReader() { };

  void Init(net::Socket* socket, MessageReceivedCallback* callback) {
    message_received_callback_.reset(callback);
    message_reader_.Init(
        socket, NewCallback(this, &ProtobufMessageReader<T>::OnNewData));
  }

 private:
  void OnNewData(CompoundBuffer* buffer) {
    T* message = new T();
    CompoundBufferInputStream stream(buffer);
    bool ret = message->ParseFromZeroCopyStream(&stream);
    if (!ret) {
      delete message;
    } else {
      message_received_callback_->Run(message);
    }
  }

  MessageReader message_reader_;
  scoped_ptr<MessageReceivedCallback> message_received_callback_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MESSAGE_READER_H_
