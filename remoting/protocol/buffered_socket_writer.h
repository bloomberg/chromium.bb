// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_
#define REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_

#include <deque>

#include "base/lock.h"
#include "base/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/socket/socket.h"

class MessageLoop;

namespace net {
class Socket;
}  // namespace net

namespace remoting {

class BufferedSocketWriter
    : public base::RefCountedThreadSafe<BufferedSocketWriter> {
 public:
  typedef Callback1<int>::Type WriteFailedCallback;

  BufferedSocketWriter();
  virtual ~BufferedSocketWriter();

  // Initializes the writer. Must be called on the thread that will be used
  // to access the socket in the future. |callback| will be called after each
  // failed write.
  void Init(net::Socket* socket, WriteFailedCallback* callback);

  // Puts a new data chunk in the buffer. Returns false and doesn't enqueue
  // the data if called before Init(). Can be called on any thread.
  bool Write(scoped_refptr<net::IOBufferWithSize> buffer);

  // Returns current size of the buffer. Can be called on any thread.
  int GetBufferSize();

  // Returns number of chunks that are currently in the buffer waiting
  // to be written. Can be called on any thread.
  int GetBufferChunks();

  // Stops writing and drops current buffers.
  void Close();

 private:
  typedef std::deque<scoped_refptr<net::IOBufferWithSize> > DataQueue;

  void DoWrite();
  void OnWritten(int result);

  net::Socket* socket_;
  MessageLoop* message_loop_;
  scoped_ptr<WriteFailedCallback> write_failed_callback_;

  Lock lock_;

  DataQueue queue_;
  int buffer_size_;
  scoped_refptr<net::DrainableIOBuffer> current_buf_;
  bool write_pending_;

  net::CompletionCallbackImpl<BufferedSocketWriter> written_callback_;

  bool closed_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_
