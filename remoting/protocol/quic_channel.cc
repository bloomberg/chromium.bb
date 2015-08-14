// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/quic_channel.h"

#include "base/callback_helpers.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace remoting {
namespace protocol {

static const size_t kNamePrefixLength = 1;
static const size_t kMaxNameLength  = 255;

QuicChannel::QuicChannel(net::QuicP2PStream* stream,
                         const base::Closure& on_destroyed_callback)
    : stream_(stream), on_destroyed_callback_(on_destroyed_callback) {
  DCHECK(stream_);
  stream_->SetDelegate(this);
}

QuicChannel::~QuicChannel() {
  // Don't call the read callback when destroying the stream.
  read_callback_.Reset();

  on_destroyed_callback_.Run();

  // The callback must destroy the stream which must result in OnClose().
  DCHECK(!stream_);
}

int QuicChannel::Read(const scoped_refptr<net::IOBuffer>& buffer,
                      int buffer_len,
                      const net::CompletionCallback& callback) {
  DCHECK(read_callback_.is_null());

  if (error_ != net::OK)
    return error_;

  if (data_received_.total_bytes() == 0) {
    read_buffer_ = buffer;
    read_buffer_size_ = buffer_len;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  int result = std::min(buffer_len, data_received_.total_bytes());
  data_received_.CopyTo(buffer->data(), result);
  data_received_.CropFront(result);
  return result;
}

int QuicChannel::Write(const scoped_refptr<net::IOBuffer>& buffer,
                       int buffer_len,
                       const net::CompletionCallback& callback) {
  if (error_ != net::OK)
    return error_;

  return stream_->Write(base::StringPiece(buffer->data(), buffer_len),
                        callback);
}

void QuicChannel::SetName(const std::string& name) {
  DCHECK(name_.empty());

  name_ = name;
}

void QuicChannel::OnDataReceived(const char* data, int length) {
  if (read_callback_.is_null()) {
    data_received_.AppendCopyOf(data, length);
    return;
  }

  DCHECK_EQ(data_received_.total_bytes(), 0);
  int bytes_to_read = std::min(length, read_buffer_size_);
  memcpy(read_buffer_->data(), data, bytes_to_read);
  read_buffer_ = nullptr;

  // Copy leftover data to |data_received_|.
  if (length > bytes_to_read)
    data_received_.AppendCopyOf(data + bytes_to_read, length - bytes_to_read);

  base::ResetAndReturn(&read_callback_).Run(bytes_to_read);
}

void QuicChannel::OnClose(net::QuicErrorCode error) {
  error_ = (error == net::QUIC_NO_ERROR) ? net::ERR_CONNECTION_CLOSED
                                         : net::ERR_QUIC_PROTOCOL_ERROR;
  stream_ = nullptr;
  if (!read_callback_.is_null()) {
    base::ResetAndReturn(&read_callback_).Run(error_);
  }
}

QuicClientChannel::QuicClientChannel(net::QuicP2PStream* stream,
                                     const base::Closure& on_destroyed_callback,
                                     const std::string& name)
    : QuicChannel(stream, on_destroyed_callback) {
  CHECK_LE(name.size(), kMaxNameLength);

  SetName(name);

  // Send the name to the host.
  stream_->WriteHeader(
      std::string(kNamePrefixLength, static_cast<char>(name.size())) + name);
}

QuicClientChannel::~QuicClientChannel() {}

QuicServerChannel::QuicServerChannel(
    net::QuicP2PStream* stream,
    const base::Closure& on_destroyed_callback)
    : QuicChannel(stream, on_destroyed_callback) {}

void QuicServerChannel::ReceiveName(
    const base::Closure& name_received_callback) {
  name_received_callback_ = name_received_callback;

  // First read 1 byte containing name length.
  name_read_buffer_ = new net::DrainableIOBuffer(
      new net::IOBuffer(kNamePrefixLength), kNamePrefixLength);
  int result = Read(name_read_buffer_, kNamePrefixLength,
                    base::Bind(&QuicServerChannel::OnNameSizeReadResult,
                               base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnNameSizeReadResult(result);
}

QuicServerChannel::~QuicServerChannel() {}

void QuicServerChannel::OnNameSizeReadResult(int result) {
  if (result < 0) {
    base::ResetAndReturn(&name_received_callback_).Run();
    return;
  }

  DCHECK_EQ(result, static_cast<int>(kNamePrefixLength));
  name_length_ = *reinterpret_cast<uint8_t*>(name_read_buffer_->data());
  name_read_buffer_ =
      new net::DrainableIOBuffer(new net::IOBuffer(name_length_), name_length_);
  ReadNameLoop(0);
}

void QuicServerChannel::ReadNameLoop(int result) {
  while (result >= 0 && name_read_buffer_->BytesRemaining() > 0) {
    result = Read(name_read_buffer_, name_read_buffer_->BytesRemaining(),
                  base::Bind(&QuicServerChannel::OnNameReadResult,
                             base::Unretained(this)));
    if (result >= 0) {
      name_read_buffer_->DidConsume(result);
    }
  }

  if (result < 0 && result != net::ERR_IO_PENDING) {
    // Failed to read name for the stream.
    base::ResetAndReturn(&name_received_callback_).Run();
    return;
  }

  if (name_read_buffer_->BytesRemaining() == 0) {
    name_read_buffer_->SetOffset(0);
    SetName(std::string(name_read_buffer_->data(),
                        name_read_buffer_->data() + name_length_));
    base::ResetAndReturn(&name_received_callback_).Run();
  }
}

void QuicServerChannel::OnNameReadResult(int result) {
  if (result > 0)
    name_read_buffer_->DidConsume(result);

  ReadNameLoop(result);
}

}  // namespace protocol
}  // namespace remoting
