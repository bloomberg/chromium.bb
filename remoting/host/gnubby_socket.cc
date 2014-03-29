// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gnubby_socket.h"

#include "base/macros.h"
#include "base/timer/timer.h"
#include "net/socket/stream_listen_socket.h"

namespace remoting {

namespace {

const size_t kRequestSizeBytes = 4;
const size_t kMaxRequestLength = 16384;
const unsigned int kRequestTimeoutSeconds = 60;

// SSH Failure Code
const char kSshError[] = {0x05};

}  // namespace

GnubbySocket::GnubbySocket(scoped_ptr<net::StreamListenSocket> socket,
                           const base::Closure& timeout_callback)
    : socket_(socket.Pass()) {
  timer_.reset(new base::Timer(false, false));
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromSeconds(kRequestTimeoutSeconds),
                timeout_callback);
}

GnubbySocket::~GnubbySocket() {}

void GnubbySocket::AddRequestData(const char* data, int data_len) {
  DCHECK(CalledOnValidThread());

  request_data_.insert(request_data_.end(), data, data + data_len);
  ResetTimer();
}

void GnubbySocket::GetAndClearRequestData(std::string* data_out) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsRequestComplete() && !IsRequestTooLarge());

  // The request size is not part of the data; don't send it.
  data_out->assign(request_data_.begin() + kRequestSizeBytes,
                   request_data_.end());
  request_data_.clear();
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

void GnubbySocket::SendResponse(const std::string& response_data) {
  DCHECK(CalledOnValidThread());

  socket_->Send(GetResponseLengthAsBytes(response_data));
  socket_->Send(response_data);
  ResetTimer();
}

void GnubbySocket::SendSshError() {
  DCHECK(CalledOnValidThread());

  SendResponse(std::string(kSshError, arraysize(kSshError)));
}

bool GnubbySocket::IsSocket(net::StreamListenSocket* socket) const {
  return socket == socket_.get();
}

void GnubbySocket::SetTimerForTesting(scoped_ptr<base::Timer> timer) {
  timer->Start(FROM_HERE, timer_->GetCurrentDelay(), timer_->user_task());
  timer_ = timer.Pass();
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
