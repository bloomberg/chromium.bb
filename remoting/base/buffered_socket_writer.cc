// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/buffered_socket_writer.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"

namespace remoting {

namespace {

int WriteNetSocket(net::Socket* socket,
                   const scoped_refptr<net::IOBuffer>& buf,
                   int buf_len,
                   const net::CompletionCallback& callback) {
  return socket->Write(buf.get(), buf_len, callback);
}

}  // namespace

struct BufferedSocketWriter::PendingPacket {
  PendingPacket(scoped_refptr<net::DrainableIOBuffer> data,
                const base::Closure& done_task)
      : data(data),
        done_task(done_task) {
  }

  scoped_refptr<net::DrainableIOBuffer> data;
  base::Closure done_task;
};

// static
scoped_ptr<BufferedSocketWriter> BufferedSocketWriter::CreateForSocket(
    net::Socket* socket,
    const WriteFailedCallback& write_failed_callback) {
  scoped_ptr<BufferedSocketWriter> result(new BufferedSocketWriter());
  result->Init(base::Bind(&WriteNetSocket, socket), write_failed_callback);
  return result.Pass();
}

BufferedSocketWriter::BufferedSocketWriter() : weak_factory_(this) {
}

BufferedSocketWriter::~BufferedSocketWriter() {
  STLDeleteElements(&queue_);
}

void BufferedSocketWriter::Init(
    const WriteCallback& write_callback,
    const WriteFailedCallback& write_failed_callback) {
  write_callback_ = write_callback;
  write_failed_callback_ = write_failed_callback;
}

void BufferedSocketWriter::Write(
    const scoped_refptr<net::IOBufferWithSize>& data,
    const base::Closure& done_task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data.get());

  // Don't write after error.
  if (is_closed())
    return;

  queue_.push_back(new PendingPacket(
      new net::DrainableIOBuffer(data.get(), data->size()), done_task));

  DoWrite();
}

bool BufferedSocketWriter::is_closed() {
  return write_callback_.is_null();
}

void BufferedSocketWriter::DoWrite() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::WeakPtr<BufferedSocketWriter> self = weak_factory_.GetWeakPtr();
  while (self && !write_pending_ && !is_closed() && !queue_.empty()) {
    int result = write_callback_.Run(
        queue_.front()->data.get(), queue_.front()->data->BytesRemaining(),
        base::Bind(&BufferedSocketWriter::OnWritten,
                   weak_factory_.GetWeakPtr()));
    HandleWriteResult(result);
  }
}

void BufferedSocketWriter::HandleWriteResult(int result) {
  if (result < 0) {
    if (result == net::ERR_IO_PENDING) {
      write_pending_ = true;
    } else {
      write_callback_.Reset();
      if (!write_failed_callback_.is_null()) {
        WriteFailedCallback callback = write_failed_callback_;
        callback.Run(result);
      }
    }
    return;
  }

  DCHECK(!queue_.empty());

  queue_.front()->data->DidConsume(result);

  if (queue_.front()->data->BytesRemaining() == 0) {
    base::Closure done_task = queue_.front()->done_task;
    delete queue_.front();
    queue_.pop_front();

    if (!done_task.is_null())
      done_task.Run();
  }
}

void BufferedSocketWriter::OnWritten(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(write_pending_);
  write_pending_ = false;

  base::WeakPtr<BufferedSocketWriter> self = weak_factory_.GetWeakPtr();
  HandleWriteResult(result);
  if (self)
    DoWrite();
}

}  // namespace remoting
