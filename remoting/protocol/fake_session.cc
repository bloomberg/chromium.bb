// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_session.h"

namespace remoting {
namespace protocol {

const char kTestJid[] = "host1@gmail.com/chromoting123";

FakeSession::FakeSession()
    : event_handler_(nullptr),
      config_(SessionConfig::ForTest()),
      jid_(kTestJid),
      error_(OK),
      closed_(false) {}

FakeSession::~FakeSession() {}

void FakeSession::SetEventHandler(EventHandler* event_handler) {
  event_handler_ = event_handler;
}

ErrorCode FakeSession::error() {
  return error_;
}

const std::string& FakeSession::jid() {
  return jid_;
}

const SessionConfig& FakeSession::config() {
  return *config_;
}

StreamChannelFactory* FakeSession::GetTransportChannelFactory() {
  return &channel_factory_;
}

StreamChannelFactory* FakeSession::GetMultiplexedChannelFactory() {
  return &channel_factory_;
}

StreamChannelFactory* FakeSession::GetQuicChannelFactory() {
  return &channel_factory_;
}

void FakeSession::Close() {
  closed_ = true;
}

}  // namespace protocol
}  // namespace remoting
