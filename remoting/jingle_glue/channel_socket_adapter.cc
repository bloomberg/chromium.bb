// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/channel_socket_adapter.h"

#include <limits>

#include "base/logging.h"
#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "remoting/jingle_glue/utils.h"
#include "third_party/libjingle/source/talk/p2p/base/transportchannel.h"

namespace remoting {

TransportChannelSocketAdapter::TransportChannelSocketAdapter(
    cricket::TransportChannel* channel)
    : channel_(channel),
      read_pending_(false),
      write_pending_(false),
      closed_error_code_(net::OK) {
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());
  DCHECK(channel_);

  channel_->SignalReadPacket.connect(
      this, &TransportChannelSocketAdapter::OnNewPacket);
  channel_->SignalWritableState.connect(
      this, &TransportChannelSocketAdapter::OnWritableState);
  channel_->SignalDestroyed.connect(
      this, &TransportChannelSocketAdapter::OnChannelDestroyed);
}

TransportChannelSocketAdapter::~TransportChannelSocketAdapter() {
}

int TransportChannelSocketAdapter::Read(
    net::IOBuffer* buf, int buffer_size, net::CompletionCallback* callback) {
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());
  DCHECK(buf);
  CHECK(!read_pending_);

  if (!channel_) {
    DCHECK(closed_error_code_ != net::OK);
    return closed_error_code_;
  }

  read_callback_ = callback;
  read_buffer_ = buf;
  read_buffer_size_ = buffer_size;
  read_pending_ = true;

  return net::ERR_IO_PENDING;
}

int TransportChannelSocketAdapter::Write(
    net::IOBuffer* buffer, int buffer_size, net::CompletionCallback* callback) {
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());
  DCHECK(buffer);
  CHECK(!write_pending_);

  if (!channel_) {
    DCHECK(closed_error_code_ != net::OK);
    return closed_error_code_;
  }

  int result = channel_->SendPacket(buffer->data(), buffer_size);
  if (result < 0) {
    result = MapPosixToChromeError(channel_->GetError());
    if (result == net::ERR_IO_PENDING) {
      write_pending_ = true;
      write_callback_ = callback;
      write_buffer_ = buffer;
      write_buffer_size_ = buffer_size;
    }
  }
  return result;
}

bool TransportChannelSocketAdapter::SetReceiveBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}

bool TransportChannelSocketAdapter::SetSendBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}

void TransportChannelSocketAdapter::Close(int error_code) {
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

  if (!channel_)  // Already closed.
    return;

  DCHECK(error_code != net::OK);
  closed_error_code_ = error_code;
  channel_->SignalReadPacket.disconnect(this);
  channel_->SignalDestroyed.disconnect(this);
  channel_ = NULL;

  if (read_pending_) {
    net::CompletionCallback* callback = read_callback_;
    read_pending_ = false;
    read_buffer_ = NULL;
    callback->Run(error_code);
  }

  if (write_pending_) {
    net::CompletionCallback* callback = write_callback_;
    write_pending_ = false;
    write_buffer_ = NULL;
    callback->Run(error_code);
  }
}

void TransportChannelSocketAdapter::OnNewPacket(
    cricket::TransportChannel* channel, const char* data, size_t data_size) {
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());
  DCHECK_EQ(channel, channel_);
  if (read_pending_) {
    DCHECK(read_buffer_);
    CHECK_LT(data_size, static_cast<size_t>(std::numeric_limits<int>::max()));

    if (read_buffer_size_ < static_cast<int>(data_size)) {
      LOG(WARNING) << "Data buffer is smaller than the received packet. "
                   << "Dropping the data that doesn't fit.";
      data_size = read_buffer_size_;
    }

    memcpy(read_buffer_->data(), data, data_size);

    net::CompletionCallback* callback = read_callback_;
    read_pending_ = false;
    read_buffer_ = NULL;

    callback->Run(data_size);
  } else {
    LOG(WARNING)
        << "Data was received without a callback. Dropping the packet.";
  }
}

void TransportChannelSocketAdapter::OnWritableState(
    cricket::TransportChannel* channel) {
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());
  // Try to send the packet if there is a pending write.
  if (write_pending_) {
    int result = channel_->SendPacket(write_buffer_->data(),
                                      write_buffer_size_);
    if (result < 0)
      result = MapPosixToChromeError(channel_->GetError());

    if (result != net::ERR_IO_PENDING) {
      net::CompletionCallback* callback = write_callback_;
      write_pending_ = false;
      write_buffer_ = NULL;
      callback->Run(result);
    }
  }
}

void TransportChannelSocketAdapter::OnChannelDestroyed(
    cricket::TransportChannel* channel) {
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());
  DCHECK_EQ(channel, channel_);
  Close(net::ERR_CONNECTION_ABORTED);
}

}  // namespace remoting
