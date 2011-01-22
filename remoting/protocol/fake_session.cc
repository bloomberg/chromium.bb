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

void FakeSocket::AppendInputData(const char* data, int data_size) {
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

FakeUdpSocket::FakeUdpSocket()
    : read_pending_(false),
      input_pos_(0) {
}

FakeUdpSocket::~FakeUdpSocket() {
}

void FakeUdpSocket::AppendInputPacket(const char* data, int data_size) {
  input_packets_.push_back(std::string());
  input_packets_.back().assign(data, data + data_size);

  // Complete pending read if any.
  if (read_pending_) {
    read_pending_ = false;
    int result = std::min(data_size, read_buffer_size_);
    memcpy(read_buffer_->data(), data, result);
    input_pos_ = input_packets_.size();
    read_callback_->Run(result);
    read_buffer_ = NULL;
  }
}

int FakeUdpSocket::Read(net::IOBuffer* buf, int buf_len,
                        net::CompletionCallback* callback) {
  if (input_pos_ < static_cast<int>(input_packets_.size())) {
    int result = std::min(
        buf_len, static_cast<int>(input_packets_[input_pos_].size()));
    memcpy(buf->data(), &(*input_packets_[input_pos_].begin()), result);
    ++input_pos_;
    return result;
  } else {
    read_pending_ = true;
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }
}

int FakeUdpSocket::Write(net::IOBuffer* buf, int buf_len,
                         net::CompletionCallback* callback) {
  written_packets_.push_back(std::string());
  written_packets_.back().assign(buf->data(), buf->data() + buf_len);
  return buf_len;
}

bool FakeUdpSocket::SetReceiveBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}
bool FakeUdpSocket::SetSendBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}

FakeSession::FakeSession()
    : candidate_config_(CandidateSessionConfig::CreateDefault()),
      config_(SessionConfig::CreateDefault()),
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

FakeUdpSocket* FakeSession::video_rtp_channel() {
  return &video_rtp_channel_;
}

FakeUdpSocket* FakeSession::video_rtcp_channel() {
  return &video_rtcp_channel_;
}

const std::string& FakeSession::jid() {
  return jid_;
}

MessageLoop* FakeSession::message_loop() {
  return message_loop_;
}

const CandidateSessionConfig* FakeSession::candidate_config() {
  return candidate_config_.get();
}

const SessionConfig* FakeSession::config() {
  CHECK(config_.get());
  return config_.get();
}

void FakeSession::set_config(const SessionConfig* config) {
  config_.reset(config);
}

const std::string& FakeSession::initiator_token() {
  return initiator_token_;
}

void FakeSession::set_initiator_token(const std::string& initiator_token) {
  initiator_token_ = initiator_token;
}

const std::string& FakeSession::receiver_token() {
  return receiver_token_;
}

void FakeSession::set_receiver_token(const std::string& receiver_token) {
  receiver_token_ = receiver_token;
}

void FakeSession::Close(Task* closed_task) {
  closed_ = true;
  closed_task->Run();
  delete closed_task;
}

}  // namespace protocol
}  // namespace remoting
