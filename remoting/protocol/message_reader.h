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
#include "net/base/io_buffer.h"
#include "remoting/protocol/message_decoder.h"

namespace net {
class Socket;
}  // namespace net

namespace remoting {

class ChromotocolConnection;
class MessageReader;

namespace internal {

template <class T>
class MessageReaderPrivate {
 private:
  friend class remoting::MessageReader;

  typedef typename Callback1<T*>::Type MessageReceivedCallback;

  MessageReaderPrivate(MessageReceivedCallback* callback)
      : message_received_callback_(callback) {
  }

  ~MessageReaderPrivate() { }

  void OnDataReceived(net::IOBuffer* buffer, int data_size) {
    typedef typename std::list<T*>::iterator MessageListIterator;

    std::list<T*> message_list;
    message_decoder_.ParseMessages(buffer, data_size, &message_list);
    for (MessageListIterator it = message_list.begin();
         it != message_list.end(); ++it) {
      message_received_callback_->Run(*it);
    }
  }

  void Destroy() {
    delete this;
  }

  // Message decoder is used to decode bytes into protobuf message.
  MessageDecoder message_decoder_;

  // Callback is called when a message is received.
  scoped_ptr<MessageReceivedCallback> message_received_callback_;
};

}  // namespace internal

// MessageReader reads data from the socket asynchronously and uses
// MessageReaderPrivate to decode the data received.
class MessageReader {
 public:
  MessageReader();
  virtual ~MessageReader();

  // Stops reading. Must be called on the same thread as Init().
  void Close();

  // Initialize the MessageReader with a socket. If a message is received
  // |callback| is called.
  template <class T>
  void Init(net::Socket* socket, typename Callback1<T*>::Type* callback) {
    internal::MessageReaderPrivate<T>* reader =
        new internal::MessageReaderPrivate<T>(callback);
    data_received_callback_.reset(
        ::NewCallback(
            reader, &internal::MessageReaderPrivate<T>::OnDataReceived));
    destruction_callback_.reset(
        ::NewCallback(reader, &internal::MessageReaderPrivate<T>::Destroy));
    Init(socket);
  }

 private:
  void Init(net::Socket* socket);

  void DoRead();
  void OnRead(int result);
  void HandleReadResult(int result);

  net::Socket* socket_;

  bool closed_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  net::CompletionCallbackImpl<MessageReader> read_callback_;

  scoped_ptr<Callback2<net::IOBuffer*, int>::Type> data_received_callback_;
  scoped_ptr<Callback0::Type> destruction_callback_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MESSAGE_READER_H_
