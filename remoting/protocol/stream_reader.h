// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_STREAM_READER_H_
#define REMOTING_PROTOCOL_STREAM_READER_H_

#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "remoting/protocol/socket_reader_base.h"

namespace remoting {

class EventStreamReader : public SocketReaderBase {
 public:
  EventStreamReader();
  ~EventStreamReader();

  // The OnMessageCallback is called whenever a new message is received.
  // Ownership of the message is passed the callback.
  typedef Callback1<ChromotingClientMessage*>::Type OnMessageCallback;

  // Initialize the reader and start reading. Must be called on the thread
  // |socket| belongs to. The callback will be called when a new message is
  // received. EventStreamReader owns |on_message_callback|, doesn't own
  // |socket|.
  void Init(net::Socket* socket, OnMessageCallback* on_message_callback);

 protected:
  virtual void OnDataReceived(net::IOBuffer* buffer, int data_size);

 private:
  MessagesDecoder messages_decoder_;
  scoped_ptr<OnMessageCallback> on_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(EventStreamReader);
};

class VideoStreamReader : public SocketReaderBase {
 public:
  VideoStreamReader();
  ~VideoStreamReader();

  // The OnMessageCallback is called whenever a new message is received.
  // Ownership of the message is passed the callback.
  typedef Callback1<ChromotingHostMessage*>::Type OnMessageCallback;

  // Initialize the reader and start reading. Must be called on the thread
  // |socket| belongs to. The callback will be called when a new message is
  // received. VideoStreamReader owns |on_message_callback|, doesn't own
  // |socket|.
  void Init(net::Socket* socket, OnMessageCallback* on_message_callback);

 protected:
  virtual void OnDataReceived(net::IOBuffer* buffer, int data_size);

 private:
  MessagesDecoder messages_decoder_;
  scoped_ptr<OnMessageCallback> on_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(VideoStreamReader);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_STREAM_READER_H_
