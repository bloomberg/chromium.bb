// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_
#define REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_

#include <list>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "net/base/io_buffer.h"
#include "net/socket/socket.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

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

class BufferedSocketWriterBase
    : public base::RefCountedThreadSafe<BufferedSocketWriterBase> {
 public:
  typedef base::Callback<void(int)> WriteFailedCallback;

  explicit BufferedSocketWriterBase(base::MessageLoopProxy* message_loop);
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
  class PendingPacket;
  typedef std::list<PendingPacket*> DataQueue;

  DataQueue queue_;
  int buffer_size_;

  // Removes element from the front of the queue and calls |done_task|
  // for that element.
  void PopQueue();

  // Following three methods must be implemented in child classes.
  // GetNextPacket() returns next packet that needs to be written to the
  // socket. |buffer| must be set to NULL if there is nothing left in the queue.
  virtual void GetNextPacket_Locked(net::IOBuffer** buffer, int* size) = 0;
  virtual void AdvanceBufferPosition_Locked(int written) = 0;

  // This method is called whenever there is an error writing to the socket.
  virtual void OnError_Locked(int result) = 0;

 private:
  void DoWrite();
  void OnWritten(int result);

  // This method is called when an error is encountered.
  void HandleError(int result);

  // Must be locked when accessing |socket_|, |queue_| and |buffer_size_|;
  base::Lock lock_;

  net::Socket* socket_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  WriteFailedCallback write_failed_callback_;

  bool write_pending_;

  net::OldCompletionCallbackImpl<BufferedSocketWriterBase> written_callback_;

  bool closed_;
};

class BufferedSocketWriter : public BufferedSocketWriterBase {
 public:
  explicit BufferedSocketWriter(base::MessageLoopProxy* message_loop);
  virtual ~BufferedSocketWriter();

 protected:
  virtual void GetNextPacket_Locked(net::IOBuffer** buffer, int* size) OVERRIDE;
  virtual void AdvanceBufferPosition_Locked(int written) OVERRIDE;
  virtual void OnError_Locked(int result) OVERRIDE;

 private:
  scoped_refptr<net::DrainableIOBuffer> current_buf_;
};

class BufferedDatagramWriter : public BufferedSocketWriterBase {
 public:
  explicit BufferedDatagramWriter(base::MessageLoopProxy* message_loop);
  virtual ~BufferedDatagramWriter();

 protected:
  virtual void GetNextPacket_Locked(net::IOBuffer** buffer, int* size) OVERRIDE;
  virtual void AdvanceBufferPosition_Locked(int written) OVERRIDE;
  virtual void OnError_Locked(int result) OVERRIDE;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_
