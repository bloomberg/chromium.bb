// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/gnubby_socket.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace remoting {

namespace {

const size_t kRequestSizeBytes = 4;
const size_t kMaxRequestLength = 16384;
const size_t kRequestReadBufferLength = kRequestSizeBytes + kMaxRequestLength;

// SSH Failure Code
const char kSshError[] = {0x05};

}  // namespace

GnubbySocket::GnubbySocket(scoped_ptr<net::StreamSocket> socket,
                           const base::TimeDelta& timeout,
                           const base::Closure& timeout_callback)
    : socket_(std::move(socket)),
      read_completed_(false),
      read_buffer_(new net::IOBufferWithSize(kRequestReadBufferLength)) {
  timer_.reset(new base::Timer(false, false));
  timer_->Start(FROM_HERE, timeout, timeout_callback);
}

GnubbySocket::~GnubbySocket() {}

bool GnubbySocket::GetAndClearRequestData(std::string* data_out) {
  DCHECK(CalledOnValidThread());
  DCHECK(read_completed_);

  if (!read_completed_)
    return false;
  if (!IsRequestComplete() || IsRequestTooLarge())
    return false;
  // The request size is not part of the data; don't send it.
  data_out->assign(request_data_.begin() + kRequestSizeBytes,
                   request_data_.end());
  request_data_.clear();
  return true;
}

void GnubbySocket::SendResponse(const std::string& response_data) {
  DCHECK(CalledOnValidThread());
  DCHECK(!write_buffer_);

  std::string response_length_string = GetResponseLengthAsBytes(response_data);
  int response_len = response_length_string.size() + response_data.size();
  scoped_ptr<std::string> response(
      new std::string(response_length_string + response_data));
  write_buffer_ = new net::DrainableIOBuffer(
      new net::StringIOBuffer(std::move(response)), response_len);
  DoWrite();
}

void GnubbySocket::SendSshError() {
  DCHECK(CalledOnValidThread());

  SendResponse(std::string(kSshError, arraysize(kSshError)));
}

void GnubbySocket::StartReadingRequest(
    const base::Closure& request_received_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(request_received_callback_.is_null());

  request_received_callback_ = request_received_callback;
  DoRead();
}

void GnubbySocket::OnDataWritten(int result) {
  DCHECK(CalledOnValidThread());
  DCHECK(write_buffer_);

  if (result < 0) {
    LOG(ERROR) << "Error sending response: " << result;
    return;
  }
  ResetTimer();
  write_buffer_->DidConsume(result);
  DoWrite();
}

void GnubbySocket::DoWrite() {
  DCHECK(CalledOnValidThread());
  DCHECK(write_buffer_);

  if (!write_buffer_->BytesRemaining()) {
    write_buffer_ = nullptr;
    return;
  }
  int result = socket_->Write(
      write_buffer_.get(), write_buffer_->BytesRemaining(),
      base::Bind(&GnubbySocket::OnDataWritten, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnDataWritten(result);
}

void GnubbySocket::OnDataRead(int result) {
  DCHECK(CalledOnValidThread());

  if (result <= 0) {
    if (result < 0)
      LOG(ERROR) << "Error reading request: " << result;
    read_completed_ = true;
    base::ResetAndReturn(&request_received_callback_).Run();
    return;
  }

  ResetTimer();
  request_data_.insert(request_data_.end(), read_buffer_->data(),
                       read_buffer_->data() + result);
  if (IsRequestComplete()) {
    read_completed_ = true;
    base::ResetAndReturn(&request_received_callback_).Run();
    return;
  }

  DoRead();
}

void GnubbySocket::DoRead() {
  DCHECK(CalledOnValidThread());

  int result = socket_->Read(
      read_buffer_.get(), kRequestReadBufferLength,
      base::Bind(&GnubbySocket::OnDataRead, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnDataRead(result);
}

bool GnubbySocket::IsRequestComplete() const {
  DCHECK(CalledOnValidThread());

  if (request_data_.size() < kRequestSizeBytes)
    return false;
  return GetRequestLength() <= request_data_.size();
}

bool GnubbySocket::IsRequestTooLarge() const {
  DCHECK(CalledOnValidThread());

  if (request_data_.size() < kRequestSizeBytes)
    return false;
  return GetRequestLength() > kMaxRequestLength;
}

size_t GnubbySocket::GetRequestLength() const {
  DCHECK(request_data_.size() >= kRequestSizeBytes);

  return ((request_data_[0] & 255) << 24) + ((request_data_[1] & 255) << 16) +
         ((request_data_[2] & 255) << 8) + (request_data_[3] & 255) +
         kRequestSizeBytes;
}

std::string GnubbySocket::GetResponseLengthAsBytes(
    const std::string& response) const {
  std::string response_len;
  response_len.reserve(kRequestSizeBytes);
  int len = response.size();

  response_len.push_back((len >> 24) & 255);
  response_len.push_back((len >> 16) & 255);
  response_len.push_back((len >> 8) & 255);
  response_len.push_back(len & 255);

  return response_len;
}

void GnubbySocket::ResetTimer() {
  if (timer_->IsRunning())
    timer_->Reset();
}

}  // namespace remoting
