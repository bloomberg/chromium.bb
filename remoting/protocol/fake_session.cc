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

FakeStreamChannelFactory* FakeTransport::GetStreamChannelFactory() {
  return &channel_factory_;
}

FakeStreamChannelFactory* FakeTransport::GetMultiplexedChannelFactory() {
  return &channel_factory_;
}

FakeSession::FakeSession()
    : config_(SessionConfig::ForTest()), jid_(kTestJid), weak_factory_(this) {}

FakeSession::~FakeSession() {}

void FakeSession::SimulateConnection(FakeSession* peer) {
  peer_ = peer->weak_factory_.GetWeakPtr();
  peer->peer_ = weak_factory_.GetWeakPtr();

  transport_.GetStreamChannelFactory()->PairWith(
      peer->transport_.GetStreamChannelFactory());
  transport_.GetMultiplexedChannelFactory()->PairWith(
      peer->transport_.GetMultiplexedChannelFactory());

  event_handler_->OnSessionStateChange(CONNECTING);
  peer->event_handler_->OnSessionStateChange(ACCEPTING);
  peer->event_handler_->OnSessionStateChange(ACCEPTED);
  event_handler_->OnSessionStateChange(ACCEPTED);
  event_handler_->OnSessionStateChange(AUTHENTICATING);
  peer->event_handler_->OnSessionStateChange(AUTHENTICATING);
  event_handler_->OnSessionStateChange(AUTHENTICATED);
  peer->event_handler_->OnSessionStateChange(AUTHENTICATED);
  event_handler_->OnSessionStateChange(CONNECTED);
  peer->event_handler_->OnSessionStateChange(CONNECTED);
}

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

void FakeSession::Close(ErrorCode error) {
  closed_ = true;
  error_ = error;
  event_handler_->OnSessionStateChange(CLOSED);

  FakeSession* peer = peer_.get();
  if (peer) {
    peer->peer_.reset();
    peer_.reset();
    peer->Close(error);
  }
}

}  // namespace protocol
}  // namespace remoting
