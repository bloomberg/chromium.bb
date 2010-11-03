// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_session.h"

#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace remoting {

namespace protocol {

const char kTestJid[] = "host1@gmail.com/chromoting123";

FakeSocket::FakeSocket()
    : read_pending_(false),
      input_pos_(0) {
}

FakeSocket::~FakeSocket() {
}

void FakeSocket::AppendInputData(char* data, int data_size) {
  input_data_.insert(input_data_.end(), data, data + data_size);
  // Complete pending read if any.
  if (read_pending_) {
    read_pending_ = false;
    int result = std::min(read_buffer_size_,
                          static_cast<int>(input_data_.size() - input_pos_));
    CHECK(result > 0);
    memcpy(read_buffer_->data(),
           &(*input_data_.begin()) + input_pos_, result);
    input_pos_ += result;
    read_callback_->Run(result);
    read_buffer_ = NULL;
  }
}

int FakeSocket::Read(net::IOBuffer* buf, int buf_len,
                     net::CompletionCallback* callback) {
  if (input_pos_ < static_cast<int>(input_data_.size())) {
    int result = std::min(buf_len,
                          static_cast<int>(input_data_.size()) - input_pos_);
    memcpy(buf->data(), &(*input_data_.begin()) + input_pos_, result);
    input_pos_ += result;
    return result;
  } else {
    read_pending_ = true;
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }
}

int FakeSocket::Write(net::IOBuffer* buf, int buf_len,
                      net::CompletionCallback* callback) {
  written_data_.insert(written_data_.end(),
                       buf->data(), buf->data() + buf_len);
  return buf_len;
}

bool FakeSocket::SetReceiveBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}
bool FakeSocket::SetSendBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}

FakeSession::FakeSession()
    : candidate_config_(CandidateChromotocolConfig::CreateDefault()),
      config_(ChromotocolConfig::CreateDefault()),
      message_loop_(NULL),
      jid_(kTestJid) {
}

FakeSession::~FakeSession() { }

void FakeSession::SetStateChangeCallback(
    StateChangeCallback* callback) {
  callback_.reset(callback);
}

FakeSocket* FakeSession::control_channel() {
  return &control_channel_;
}

FakeSocket* FakeSession::event_channel() {
  return &event_channel_;
}

FakeSocket* FakeSession::video_channel() {
  return &video_channel_;
}

FakeSocket* FakeSession::video_rtp_channel() {
  return &video_rtp_channel_;
}

FakeSocket* FakeSession::video_rtcp_channel() {
  return &video_rtcp_channel_;
}

const std::string& FakeSession::jid() {
  return jid_;
}

MessageLoop* FakeSession::message_loop() {
  return message_loop_;
}

const CandidateChromotocolConfig*
FakeSession::candidate_config() {
  return candidate_config_.get();
}

const ChromotocolConfig* FakeSession::config() {
  CHECK(config_.get());
  return config_.get();
}

void FakeSession::set_config(const ChromotocolConfig* config) {
  config_.reset(config);
}

void FakeSession::Close(Task* closed_task) {
  closed_ = true;
  closed_task->Run();
  delete closed_task;
}

}  // namespace protocol

}  // namespace remoting
