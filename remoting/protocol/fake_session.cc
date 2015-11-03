// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_session.h"

namespace remoting {
namespace protocol {

const char kTestJid[] = "host1@gmail.com/chromoting123";

FakeTransport::FakeTransport() {}
FakeTransport::~FakeTransport() {}

void FakeTransport::Start(EventHandler* event_handler,
                                 Authenticator* authenticator) {
  NOTREACHED();
}

bool FakeTransport::ProcessTransportInfo(
    buzz::XmlElement* transport_info) {
  NOTREACHED();
  return true;
}

DatagramChannelFactory* FakeTransport::GetDatagramChannelFactory() {
  NOTIMPLEMENTED();
  return nullptr;
}

FakeStreamChannelFactory* FakeTransport::GetStreamChannelFactory() {
  return &channel_factory_;
}

FakeStreamChannelFactory* FakeTransport::GetMultiplexedChannelFactory() {
  return &channel_factory_;
}

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

FakeTransport* FakeSession::GetTransport() {
  return &transport_;
}

FakeStreamChannelFactory* FakeSession::GetQuicChannelFactory() {
  return transport_.GetStreamChannelFactory();
}

void FakeSession::Close(ErrorCode error) {
  closed_ = true;
  error_ = error;
}

}  // namespace protocol
}  // namespace remoting
