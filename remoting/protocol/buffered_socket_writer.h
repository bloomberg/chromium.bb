// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_
#define REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_

#include <list>

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/io_buffer.h"
#include "net/socket/socket.h"

namespace net {
class Socket;
}  // namespace net

namespace remoting {
namespace protocol {

// BufferedSocketWriter and BufferedDatagramWriter implement write data queue
// for stream and datagram sockets. BufferedSocketWriterBase is a base class
// that implements base functionality common for streams and datagrams.
// These classes are particularly useful when data comes from a thread
// that doesn't own the socket, as Write() can be called from any thread.
// Whenever new data is written it is just put in the queue, and then written
// on the thread that owns the socket. GetBufferChunks() and GetBufferSize()
// can be used to throttle writes.

class BufferedSocketWriterBase : public base::NonThreadSafe {
 public:
  typedef base::Callback<void(int)> WriteFailedCallback;

  BufferedSocketWriterBase();
  virtual ~BufferedSocketWriterBase();

  // Initializes the writer. Must be called on the thread that will be used
  // to access the socket in the future. |callback| will be called after each
  // failed write. Caller retains ownership of |socket|.
  // TODO(sergeyu): Change it so that it take ownership of |socket|.
  void Init(net::Socket* socket, const WriteFailedCallback& callback);

  // Puts a new data chunk in the buffer. Returns false and doesn't enqueue
  // the data if called before Init(). Can be called on any thread.
  bool Write(scoped_refptr<net::IOBufferWithSize> buffer,
             const base::Closure& done_task);

  // Returns current size of the buffer. Can be called on any thread.
  int GetBufferSize();

  // Returns number of chunks that are currently in the buffer waiting
  // to be written. Can be called on any thread.
  int GetBufferChunks();

  // Stops writing and drops current buffers. Must be called on the
  // network thread.
  void Close();

 protected:
  struct PendingPacket;
  typedef std::list<PendingPacket*> DataQueue;

  DataQueue queue_;
  int buffer_size_;

  // Removes element from the front of the queue and returns |done_task| for
  // that element. Called from AdvanceBufferPosition() implementation, which
  // then returns result of this function to its caller.
  base::Closure PopQueue();

  // Following three methods must be implemented in child classes.

  // Returns next packet that needs to be written to the socket. Implementation
  // must set |*buffer| to NULL if there is nothing left in the queue.
  virtual void GetNextPacket(net::IOBuffer** buffer, int* size) = 0;

  // Returns closure that must be executed or null closure if the last write
  // didn't complete any messages.
  virtual base::Closure AdvanceBufferPosition(int written) = 0;

  // This method is called whenever there is an error writing to the socket.
  virtual void OnError(int result) = 0;

 private:
  void DoWrite();
  void HandleWriteResult(int result, bool* write_again);
  void OnWritten(int result);

  // This method is called when an error is encountered.
  void HandleError(int result);

  net::Socket* socket_;
  WriteFailedCallback write_failed_callback_;

  bool write_pending_;

  bool closed_;

  bool* destroyed_flag_;
};

class BufferedSocketWriter : public BufferedSocketWriterBase {
 public:
  BufferedSocketWriter();
  virtual ~BufferedSocketWriter();

 protected:
  virtual void GetNextPacket(net::IOBuffer** buffer, int* size) OVERRIDE;
  virtual base::Closure AdvanceBufferPosition(int written) OVERRIDE;
  virtual void OnError(int result) OVERRIDE;

 private:
  scoped_refptr<net::DrainableIOBuffer> current_buf_;
};

class BufferedDatagramWriter : public BufferedSocketWriterBase {
 public:
  BufferedDatagramWriter();
  virtual ~BufferedDatagramWriter();

 protected:
  virtual void GetNextPacket(net::IOBuffer** buffer, int* size) OVERRIDE;
  virtual base::Closure AdvanceBufferPosition(int written) OVERRIDE;
  virtual void OnError(int result) OVERRIDE;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_
