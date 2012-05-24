// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/buffered_socket_writer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "net/base/net_errors.h"

namespace remoting {
namespace protocol {

class BufferedSocketWriterBase::PendingPacket {
 public:
  PendingPacket(scoped_refptr<net::IOBufferWithSize> data,
                const base::Closure& done_task)
      : data_(data),
        done_task_(done_task) {
  }
  ~PendingPacket() {
    if (!done_task_.is_null())
      done_task_.Run();
  }

  net::IOBufferWithSize* data() {
    return data_;
  }

 private:
  scoped_refptr<net::IOBufferWithSize> data_;
  base::Closure done_task_;

  DISALLOW_COPY_AND_ASSIGN(PendingPacket);
};

BufferedSocketWriterBase::BufferedSocketWriterBase()
    : buffer_size_(0),
      socket_(NULL),
      write_pending_(false),
      closed_(false) {
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

  // Don't write after Close().
  if (closed_)
    return false;

  queue_.push_back(new PendingPacket(data, done_task));
  buffer_size_ += data->size();

  DoWrite();
  return true;
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
    if (result >= 0) {
      AdvanceBufferPosition(result);
    } else {
      if (result == net::ERR_IO_PENDING) {
        write_pending_ = true;
      } else {
        HandleError(result);
        if (!write_failed_callback_.is_null())
          write_failed_callback_.Run(result);
      }
      return;
    }
  }
}

void BufferedSocketWriterBase::OnWritten(int result) {
  DCHECK(CalledOnValidThread());
  write_pending_ = false;

  if (result < 0) {
    HandleError(result);
    if (!write_failed_callback_.is_null())
      write_failed_callback_.Run(result);
    return;
  }

  AdvanceBufferPosition(result);

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

BufferedSocketWriterBase::~BufferedSocketWriterBase() {}

void BufferedSocketWriterBase::PopQueue() {
  // This also calls |done_task|.
  delete queue_.front();
  queue_.pop_front();
}

BufferedSocketWriter::BufferedSocketWriter() {
}

void BufferedSocketWriter::GetNextPacket(
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

void BufferedSocketWriter::AdvanceBufferPosition(int written) {
  buffer_size_ -= written;
  current_buf_->DidConsume(written);

  if (current_buf_->BytesRemaining() == 0) {
    PopQueue();
    current_buf_ = NULL;
  }
}

void BufferedSocketWriter::OnError(int result) {
  current_buf_ = NULL;
}

BufferedSocketWriter::~BufferedSocketWriter() {
  STLDeleteElements(&queue_);
}

BufferedDatagramWriter::BufferedDatagramWriter() {
}

void BufferedDatagramWriter::GetNextPacket(
    net::IOBuffer** buffer, int* size) {
  if (queue_.empty()) {
    *buffer = NULL;
    return;  // Nothing to write.
  }
  *buffer = queue_.front()->data();
  *size = queue_.front()->data()->size();
}

void BufferedDatagramWriter::AdvanceBufferPosition(int written) {
  DCHECK_EQ(written, queue_.front()->data()->size());
  buffer_size_ -= queue_.front()->data()->size();
  PopQueue();
}

void BufferedDatagramWriter::OnError(int result) {
  // Nothing to do here.
}

BufferedDatagramWriter::~BufferedDatagramWriter() {}

}  // namespace protocol
}  // namespace remoting
