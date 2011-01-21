// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_
#define REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_

#include <list>

#include "base/ref_counted.h"
#include "base/synchronization/lock.h"
#include "net/base/io_buffer.h"
#include "net/socket/socket.h"

class MessageLoop;
class Task;

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
  typedef Callback1<int>::Type WriteFailedCallback;

  explicit BufferedSocketWriterBase();
  virtual ~BufferedSocketWriterBase();

  // Initializes the writer. Must be called on the thread that will be used
  // to access the socket in the future. |callback| will be called after each
  // failed write.
  void Init(net::Socket* socket, WriteFailedCallback* callback);

  // Puts a new data chunk in the buffer. Returns false and doesn't enqueue
  // the data if called before Init(). Can be called on any thread.
  bool Write(scoped_refptr<net::IOBufferWithSize> buffer, Task* done_task);

  // Returns current size of the buffer. Can be called on any thread.
  int GetBufferSize();

  // Returns number of chunks that are currently in the buffer waiting
  // to be written. Can be called on any thread.
  int GetBufferChunks();

  // Stops writing and drops current buffers.
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
  MessageLoop* message_loop_;
  scoped_ptr<WriteFailedCallback> write_failed_callback_;

  bool write_pending_;

  net::CompletionCallbackImpl<BufferedSocketWriterBase> written_callback_;

  bool closed_;
};

class BufferedSocketWriter : public BufferedSocketWriterBase {
 public:
  BufferedSocketWriter();
  virtual ~BufferedSocketWriter();

 protected:
  virtual void GetNextPacket_Locked(net::IOBuffer** buffer, int* size);
  virtual void AdvanceBufferPosition_Locked(int written);
  virtual void OnError_Locked(int result);

 private:
  scoped_refptr<net::DrainableIOBuffer> current_buf_;
};

class BufferedDatagramWriter : public BufferedSocketWriterBase {
 public:
  BufferedDatagramWriter();
  virtual ~BufferedDatagramWriter();

 protected:
  virtual void GetNextPacket_Locked(net::IOBuffer** buffer, int* size);
  virtual void AdvanceBufferPosition_Locked(int written);
  virtual void OnError_Locked(int result);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_BUFFERED_SOCKET_WRITER_H_
