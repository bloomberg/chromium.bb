// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SOCKET_READER_BASE_H_
#define REMOTING_PROTOCOL_SOCKET_READER_BASE_H_

#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "remoting/protocol/messages_decoder.h"

namespace net {
class Socket;
}  // namespace net

namespace remoting {

class SocketReaderBase {
 public:
  SocketReaderBase();
  virtual ~SocketReaderBase();

  // Stops reading. Must be called on the same thread as Init().
  void Close();

 protected:
  void Init(net::Socket* socket);
  virtual void OnDataReceived(net::IOBuffer* buffer, int data_size) = 0;

 private:
  void DoRead();
  void OnRead(int result);
  void HandleReadResult(int result);

  net::Socket* socket_;
  bool closed_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  net::CompletionCallbackImpl<SocketReaderBase> read_callback_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SOCKET_READER_BASE_H_
