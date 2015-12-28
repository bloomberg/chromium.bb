// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_session.h"

#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/protocol/fake_authenticator.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

const char kTestJid[] = "host1@gmail.com/chromoting123";

FakeSession::FakeSession()
    : config_(SessionConfig::ForTest()), jid_(kTestJid), weak_factory_(this) {}
FakeSession::~FakeSession() {}

void FakeSession::SimulateConnection(FakeSession* peer) {
  peer_ = peer->weak_factory_.GetWeakPtr();
  peer->peer_ = weak_factory_.GetWeakPtr();

  event_handler_->OnSessionStateChange(CONNECTING);
  peer->event_handler_->OnSessionStateChange(ACCEPTING);
  peer->event_handler_->OnSessionStateChange(ACCEPTED);
  event_handler_->OnSessionStateChange(ACCEPTED);
  event_handler_->OnSessionStateChange(AUTHENTICATING);
  peer->event_handler_->OnSessionStateChange(AUTHENTICATING);

  // Initialize transport and authenticator on the client.
  authenticator_.reset(new FakeAuthenticator(FakeAuthenticator::CLIENT, 0,
                                             FakeAuthenticator::ACCEPT, false));
  transport_->Start(authenticator_.get(),
                    base::Bind(&FakeSession::SendTransportInfo,
                               weak_factory_.GetWeakPtr()));

  // Initialize transport and authenticator on the host.
  peer->authenticator_.reset(new FakeAuthenticator(
      FakeAuthenticator::HOST, 0, FakeAuthenticator::ACCEPT, false));
  peer->transport_->Start(peer->authenticator_.get(),
                          base::Bind(&FakeSession::SendTransportInfo, peer_));

  peer->event_handler_->OnSessionStateChange(AUTHENTICATED);
  event_handler_->OnSessionStateChange(AUTHENTICATED);
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

void FakeSession::SetTransport(Transport* transport) {
  transport_ = transport;
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

void FakeSession::SendTransportInfo(
    scoped_ptr<buzz::XmlElement> transport_info) {
  if (!peer_)
    return;
  peer_->transport_->ProcessTransportInfo(transport_info.get());
}

}  // namespace protocol
}  // namespace remoting
