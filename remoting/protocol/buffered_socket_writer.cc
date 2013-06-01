// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/buffered_socket_writer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/net_errors.h"

namespace remoting {
namespace protocol {

struct BufferedSocketWriterBase::PendingPacket {
  PendingPacket(scoped_refptr<net::IOBufferWithSize> data,
                const base::Closure& done_task)
      : data(data),
        done_task(done_task) {
  }

  scoped_refptr<net::IOBufferWithSize> data;
  base::Closure done_task;
};

BufferedSocketWriterBase::BufferedSocketWriterBase()
    : buffer_size_(0),
      socket_(NULL),
      write_pending_(false),
      closed_(false),
      destroyed_flag_(NULL) {
}

void BufferedSocketWriterBase::Init(net::Socket* socket,
                                    const WriteFailedCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(socket);
  socket_ = socket;
  write_failed_callback_ = callback;
}

bool BufferedSocketWriterBase::Write(
    scoped_refptr<net::IOBufferWithSize> data, const base::Closure& done_task) {
  DCHECK(CalledOnValidThread());
  DCHECK(socket_);
  DCHECK(data.get());

  // Don't write after Close().
  if (closed_)
    return false;

  queue_.push_back(new PendingPacket(data, done_task));
  buffer_size_ += data->size();

  DoWrite();

  // DoWrite() may trigger OnWriteError() to be called.
  return !closed_;
}

void BufferedSocketWriterBase::DoWrite() {
  DCHECK(CalledOnValidThread());
  DCHECK(socket_);

  // Don't try to write if there is another write pending.
  if (write_pending_)
    return;

  // Don't write after Close().
  if (closed_)
    return;

  while (true) {
    net::IOBuffer* current_packet;
    int current_packet_size;
    GetNextPacket(&current_packet, &current_packet_size);

    // Return if the queue is empty.
    if (!current_packet)
      return;

    int result = socket_->Write(
        current_packet, current_packet_size,
        base::Bind(&BufferedSocketWriterBase::OnWritten,
                   base::Unretained(this)));
    bool write_again = false;
    HandleWriteResult(result, &write_again);
    if (!write_again)
      return;
  }
}

void BufferedSocketWriterBase::HandleWriteResult(int result,
                                                 bool* write_again) {
  *write_again = false;
  if (result < 0) {
    if (result == net::ERR_IO_PENDING) {
      write_pending_ = true;
    } else {
      HandleError(result);
      if (!write_failed_callback_.is_null())
        write_failed_callback_.Run(result);
    }
    return;
  }

  base::Closure done_task = AdvanceBufferPosition(result);
  if (!done_task.is_null()) {
    bool destroyed = false;
    destroyed_flag_ = &destroyed;
    done_task.Run();
    if (destroyed) {
      // Stop doing anything if we've been destroyed by the callback.
      return;
    }
    destroyed_flag_ = NULL;
  }

  *write_again = true;
}

void BufferedSocketWriterBase::OnWritten(int result) {
  DCHECK(CalledOnValidThread());
  DCHECK(write_pending_);
  write_pending_ = false;

  bool write_again;
  HandleWriteResult(result, &write_again);
  if (write_again)
    DoWrite();
}

void BufferedSocketWriterBase::HandleError(int result) {
  DCHECK(CalledOnValidThread());

  closed_ = true;

  STLDeleteElements(&queue_);

  // Notify subclass that an error is received.
  OnError(result);
}

int BufferedSocketWriterBase::GetBufferSize() {
  return buffer_size_;
}

int BufferedSocketWriterBase::GetBufferChunks() {
  return queue_.size();
}

void BufferedSocketWriterBase::Close() {
  DCHECK(CalledOnValidThread());
  closed_ = true;
}

BufferedSocketWriterBase::~BufferedSocketWriterBase() {
  if (destroyed_flag_)
    *destroyed_flag_ = true;

  STLDeleteElements(&queue_);
}

base::Closure BufferedSocketWriterBase::PopQueue() {
  base::Closure result = queue_.front()->done_task;
  delete queue_.front();
  queue_.pop_front();
  return result;
}

BufferedSocketWriter::BufferedSocketWriter() {
}

void BufferedSocketWriter::GetNextPacket(
    net::IOBuffer** buffer, int* size) {
  if (!current_buf_.get()) {
    if (queue_.empty()) {
      *buffer = NULL;
      return;  // Nothing to write.
    }
    current_buf_ = new net::DrainableIOBuffer(queue_.front()->data.get(),
                                              queue_.front()->data->size());
  }

  *buffer = current_buf_.get();
  *size = current_buf_->BytesRemaining();
}

base::Closure BufferedSocketWriter::AdvanceBufferPosition(int written) {
  buffer_size_ -= written;
  current_buf_->DidConsume(written);

  if (current_buf_->BytesRemaining() == 0) {
    current_buf_ = NULL;
    return PopQueue();
  }
  return base::Closure();
}

void BufferedSocketWriter::OnError(int result) {
  current_buf_ = NULL;
}

BufferedSocketWriter::~BufferedSocketWriter() {
}

BufferedDatagramWriter::BufferedDatagramWriter() {
}

void BufferedDatagramWriter::GetNextPacket(
    net::IOBuffer** buffer, int* size) {
  if (queue_.empty()) {
    *buffer = NULL;
    return;  // Nothing to write.
  }
  *buffer = queue_.front()->data.get();
  *size = queue_.front()->data->size();
}

base::Closure BufferedDatagramWriter::AdvanceBufferPosition(int written) {
  DCHECK_EQ(written, queue_.front()->data->size());
  buffer_size_ -= queue_.front()->data->size();
  return PopQueue();
}

void BufferedDatagramWriter::OnError(int result) {
  // Nothing to do here.
}

BufferedDatagramWriter::~BufferedDatagramWriter() {
}

}  // namespace protocol
}  // namespace remoting
