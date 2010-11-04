// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/buffered_socket_writer.h"

#include "base/message_loop.h"
#include "net/base/net_errors.h"

namespace remoting {
namespace protocol {

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
  AutoLock auto_lock(lock_);
  message_loop_ = MessageLoop::current();
  socket_ = socket;
}

bool BufferedSocketWriterBase::Write(
    scoped_refptr<net::IOBufferWithSize> data) {
  AutoLock auto_lock(lock_);
  if (!socket_)
    return false;
  queue_.push(data);
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
    AutoLock auto_lock(lock_);
    if (closed_)
      return;
  }

  while (true) {
    net::IOBuffer* current_packet;
    int current_packet_size;
    {
      AutoLock auto_lock(lock_);
      GetNextPacket_Locked(&current_packet, &current_packet_size);
    }

    // Return if the queue is empty.
    if (!current_packet)
      return;

    int result = socket_->Write(current_packet, current_packet_size,
                                &written_callback_);
    if (result >= 0) {
      AutoLock auto_lock(lock_);
      AdvanceBufferPosition_Locked(result);
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

void BufferedSocketWriterBase::OnWritten(int result) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  write_pending_ = false;

  if (result < 0) {
    if (write_failed_callback_.get())
      write_failed_callback_->Run(result);
    return;
  }

  {
    AutoLock auto_lock(lock_);
    AdvanceBufferPosition_Locked(result);
  }

  // Schedule next write.
  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &BufferedSocketWriterBase::DoWrite));
}

int BufferedSocketWriterBase::GetBufferSize() {
  AutoLock auto_lock(lock_);
  return buffer_size_;
}

int BufferedSocketWriterBase::GetBufferChunks() {
  AutoLock auto_lock(lock_);
  return queue_.size();
}

void BufferedSocketWriterBase::Close() {
  AutoLock auto_lock(lock_);
  closed_ = true;
}

BufferedSocketWriter::BufferedSocketWriter() { }
BufferedSocketWriter::~BufferedSocketWriter() { }

void BufferedSocketWriter::GetNextPacket_Locked(
    net::IOBuffer** buffer, int* size) {
  while (!current_buf_ || current_buf_->BytesRemaining() == 0) {
    if (queue_.empty()) {
      *buffer = NULL;
      return;  // Nothing to write.
    }
    current_buf_ =
        new net::DrainableIOBuffer(queue_.front(), queue_.front()->size());
    queue_.pop();
  }

  *buffer = current_buf_;
  *size = current_buf_->BytesRemaining();
}

void BufferedSocketWriter::AdvanceBufferPosition_Locked(int written) {
  buffer_size_ -= written;
  current_buf_->DidConsume(written);
}

BufferedDatagramWriter::BufferedDatagramWriter() { }
BufferedDatagramWriter::~BufferedDatagramWriter() { }

void BufferedDatagramWriter::GetNextPacket_Locked(
    net::IOBuffer** buffer, int* size) {
  if (queue_.empty()) {
    *buffer = NULL;
    return;  // Nothing to write.
  }
  *buffer = queue_.front();
  *size = queue_.front()->size();
}

void BufferedDatagramWriter::AdvanceBufferPosition_Locked(int written) {
  DCHECK_EQ(written, queue_.front()->size());
  buffer_size_ -= queue_.front()->size();
  queue_.pop();
}


}  // namespace protocol
}  // namespace remoting
