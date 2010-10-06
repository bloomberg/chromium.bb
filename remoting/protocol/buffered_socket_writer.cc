// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/buffered_socket_writer.h"

#include "base/message_loop.h"
#include "net/base/net_errors.h"

namespace remoting {

BufferedSocketWriter::BufferedSocketWriter()
   : socket_(NULL),
     message_loop_(NULL),
     buffer_size_(0),
     write_pending_(false),
     ALLOW_THIS_IN_INITIALIZER_LIST(
         written_callback_(this, &BufferedSocketWriter::OnWritten)),
     closed_(false) {
}

BufferedSocketWriter::~BufferedSocketWriter() { }

void BufferedSocketWriter::Init(net::Socket* socket,
                                WriteFailedCallback* callback) {
  AutoLock auto_lock(lock_);
  message_loop_ = MessageLoop::current();
  socket_ = socket;
}

bool BufferedSocketWriter::Write(scoped_refptr<net::IOBufferWithSize> data) {
  AutoLock auto_lock(lock_);
  if (!socket_)
    return false;
  queue_.push_back(data);
  buffer_size_ += data->size();
  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &BufferedSocketWriter::DoWrite));
  return true;
}

void BufferedSocketWriter::DoWrite() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(socket_);

  // Don't try to write if the writer not initialized or closed, or
  // there is already a write pending.
  if (write_pending_ || closed_)
    return;

  while (true) {
    while (!current_buf_ || current_buf_->BytesRemaining() == 0) {
      AutoLock auto_lock(lock_);
      if (queue_.empty())
        return;  // Nothing to write.
      current_buf_ =
          new net::DrainableIOBuffer(queue_.front(), queue_.front()->size());
      queue_.pop_front();
    }

    int result = socket_->Write(current_buf_, current_buf_->BytesRemaining(),
                                &written_callback_);
    if (result >= 0) {
      {
        AutoLock auto_lock(lock_);
        buffer_size_ -= result;
      }
      current_buf_->DidConsume(result);
    } else {
      if (result == net::ERR_IO_PENDING) {
        write_pending_ = true;
      } else {
        if (write_failed_callback_.get())
          write_failed_callback_->Run(result);
      }

      return;
    }
  }
}

void BufferedSocketWriter::OnWritten(int result) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  write_pending_ = false;

  if (result < 0) {
    if (write_failed_callback_.get())
      write_failed_callback_->Run(result);
  }

  {
    AutoLock auto_lock(lock_);
    buffer_size_ -= result;
  }
  current_buf_->DidConsume(result);
  // Schedule next write.
  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &BufferedSocketWriter::DoWrite));
}

int BufferedSocketWriter::GetBufferSize() {
  AutoLock auto_lock(lock_);
  return buffer_size_;
}

int BufferedSocketWriter::GetBufferChunks() {
  AutoLock auto_lock(lock_);
  return queue_.size();
}

void BufferedSocketWriter::Close() {
  AutoLock auto_lock(lock_);
  closed_ = true;
}

}  // namespace remoting
