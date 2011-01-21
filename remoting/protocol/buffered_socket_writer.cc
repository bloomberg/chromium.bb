// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/buffered_socket_writer.h"

#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "net/base/net_errors.h"

namespace remoting {
namespace protocol {

class BufferedSocketWriterBase::PendingPacket {
 public:
  PendingPacket(scoped_refptr<net::IOBufferWithSize> data, Task* done_task)
      : data_(data),
        done_task_(done_task) {
  }
  ~PendingPacket() {
    if (done_task_.get())
      done_task_->Run();
  }

  net::IOBufferWithSize* data() {
    return data_;
  }

 private:
  scoped_refptr<net::IOBufferWithSize> data_;
  scoped_ptr<Task> done_task_;

  DISALLOW_COPY_AND_ASSIGN(PendingPacket);
};

BufferedSocketWriterBase::BufferedSocketWriterBase()
    : buffer_size_(0),
      socket_(NULL),
      message_loop_(NULL),
      write_pending_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          written_callback_(this, &BufferedSocketWriterBase::OnWritten)),
      closed_(false) {
}

BufferedSocketWriterBase::~BufferedSocketWriterBase() { }

void BufferedSocketWriterBase::Init(net::Socket* socket,
                                    WriteFailedCallback* callback) {
  // TODO(garykac) Save copy of WriteFailedCallback.
  base::AutoLock auto_lock(lock_);
  message_loop_ = MessageLoop::current();
  socket_ = socket;
  DCHECK(socket_);
}

bool BufferedSocketWriterBase::Write(
    scoped_refptr<net::IOBufferWithSize> data, Task* done_task) {
  base::AutoLock auto_lock(lock_);
  if (!socket_)
    return false;
  queue_.push_back(new PendingPacket(data, done_task));
  buffer_size_ += data->size();
  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &BufferedSocketWriterBase::DoWrite));
  return true;
}

void BufferedSocketWriterBase::DoWrite() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(socket_);

  // Don't try to write if there is another write pending.
  if (write_pending_)
    return;

  // Don't write after Close().
  {
    base::AutoLock auto_lock(lock_);
    if (closed_)
      return;
  }

  while (true) {
    net::IOBuffer* current_packet;
    int current_packet_size;
    {
      base::AutoLock auto_lock(lock_);
      GetNextPacket_Locked(&current_packet, &current_packet_size);
    }

    // Return if the queue is empty.
    if (!current_packet)
      return;

    int result = socket_->Write(current_packet, current_packet_size,
                                &written_callback_);
    if (result >= 0) {
      base::AutoLock auto_lock(lock_);
      AdvanceBufferPosition_Locked(result);
    } else {
      if (result == net::ERR_IO_PENDING) {
        write_pending_ = true;
      } else {
        HandleError(result);
        if (write_failed_callback_.get())
          write_failed_callback_->Run(result);
      }
      return;
    }
  }
}

void BufferedSocketWriterBase::OnWritten(int result) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  write_pending_ = false;

  if (result < 0) {
    HandleError(result);
    if (write_failed_callback_.get())
      write_failed_callback_->Run(result);
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    AdvanceBufferPosition_Locked(result);
  }

  // Schedule next write.
  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &BufferedSocketWriterBase::DoWrite));
}

void BufferedSocketWriterBase::HandleError(int result) {
  base::AutoLock auto_lock(lock_);
  closed_ = true;
  STLDeleteElements(&queue_);

  // Notify subclass that an error is received.
  OnError_Locked(result);
}

int BufferedSocketWriterBase::GetBufferSize() {
  base::AutoLock auto_lock(lock_);
  return buffer_size_;
}

int BufferedSocketWriterBase::GetBufferChunks() {
  base::AutoLock auto_lock(lock_);
  return queue_.size();
}

void BufferedSocketWriterBase::Close() {
  base::AutoLock auto_lock(lock_);
  closed_ = true;
}

void BufferedSocketWriterBase::PopQueue() {
  // This also calls |done_task|.
  delete queue_.front();
  queue_.pop_front();
}

BufferedSocketWriter::BufferedSocketWriter() { }

BufferedSocketWriter::~BufferedSocketWriter() {
  STLDeleteElements(&queue_);
}

void BufferedSocketWriter::GetNextPacket_Locked(
    net::IOBuffer** buffer, int* size) {
  if (!current_buf_) {
    if (queue_.empty()) {
      *buffer = NULL;
      return;  // Nothing to write.
    }
    current_buf_ = new net::DrainableIOBuffer(
        queue_.front()->data(), queue_.front()->data()->size());
  }

  *buffer = current_buf_;
  *size = current_buf_->BytesRemaining();
}

void BufferedSocketWriter::AdvanceBufferPosition_Locked(int written) {
  buffer_size_ -= written;
  current_buf_->DidConsume(written);

  if (current_buf_->BytesRemaining() == 0) {
    PopQueue();
    current_buf_ = NULL;
  }
}

void BufferedSocketWriter::OnError_Locked(int result) {
  current_buf_ = NULL;
}

BufferedDatagramWriter::BufferedDatagramWriter() { }
BufferedDatagramWriter::~BufferedDatagramWriter() { }

void BufferedDatagramWriter::GetNextPacket_Locked(
    net::IOBuffer** buffer, int* size) {
  if (queue_.empty()) {
    *buffer = NULL;
    return;  // Nothing to write.
  }
  *buffer = queue_.front()->data();
  *size = queue_.front()->data()->size();
}

void BufferedDatagramWriter::AdvanceBufferPosition_Locked(int written) {
  DCHECK_EQ(written, queue_.front()->data()->size());
  buffer_size_ -= queue_.front()->data()->size();
  PopQueue();
}

void BufferedDatagramWriter::OnError_Locked(int result) {
  // Nothing to do here.
}

}  // namespace protocol
}  // namespace remoting
