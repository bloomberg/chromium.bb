// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_connection_to_host.h"

#include "remoting/protocol/authenticator.h"

namespace remoting {
namespace test {

FakeConnectionToHost::FakeConnectionToHost()
    : state_(INITIALIZING),
      session_config_(protocol::SessionConfig::ForTest()) {
}

FakeConnectionToHost::~FakeConnectionToHost() {
}

void FakeConnectionToHost::set_candidate_config(
    scoped_ptr<protocol::CandidateSessionConfig> config) {
}

void FakeConnectionToHost::set_client_stub(protocol::ClientStub* client_stub) {
}

void FakeConnectionToHost::set_clipboard_stub(
    protocol::ClipboardStub* clipboard_stub) {
}

void FakeConnectionToHost::set_video_stub(protocol::VideoStub* video_stub) {
}

void FakeConnectionToHost::set_audio_stub(protocol::AudioStub* audio_stub) {
}

void FakeConnectionToHost::Connect(
    SignalStrategy* signal_strategy,
    scoped_ptr<protocol::TransportFactory> transport_factory,
    scoped_ptr<protocol::Authenticator> authenticator,
    const std::string& host_jid,
    HostEventCallback* event_callback) {
  DCHECK(event_callback);

  event_callback_ = event_callback;

  SetState(CONNECTING, protocol::OK);
}

void FakeConnectionToHost::SignalStateChange(protocol::Session::State state,
                                             protocol::ErrorCode error) {
  DCHECK(event_callback_);

  switch (state) {
    case protocol::Session::INITIALIZING:
    case protocol::Session::CONNECTING:
    case protocol::Session::ACCEPTING:
    case protocol::Session::AUTHENTICATING:
      // No updates for these events.
      break;

    case protocol::Session::CONNECTED:
      SetState(CONNECTED, error);
      break;

    case protocol::Session::AUTHENTICATED:
      SetState(AUTHENTICATED, error);
      break;

    case protocol::Session::CLOSED:
      SetState(CLOSED, error);
      break;

    case protocol::Session::FAILED:
      DCHECK(error != protocol::ErrorCode::OK);
      SetState(FAILED, error);
      break;
  }
}

void FakeConnectionToHost::SignalConnectionReady(bool ready) {
  DCHECK(event_callback_);

  event_callback_->OnConnectionReady(ready);
}

const protocol::SessionConfig& FakeConnectionToHost::config() {
  return session_config_;
}

protocol::ClipboardStub* FakeConnectionToHost::clipboard_forwarder() {
  return &mock_clipboard_stub_;
}

protocol::HostStub* FakeConnectionToHost::host_stub() {
  return &mock_host_stub_;
}

protocol::InputStub* FakeConnectionToHost::input_stub() {
  return &mock_input_stub_;
}

protocol::ConnectionToHost::State FakeConnectionToHost::state() const {
  return state_;
}

void FakeConnectionToHost::SetState(State state, protocol::ErrorCode error) {
  // |error| should be specified only when |state| is set to FAILED.
  DCHECK(state == FAILED || error == protocol::OK);

  if (state != state_) {
    state_ = state;
    event_callback_->OnConnectionState(state_, error);
  }
}

}  // namespace test
}  // namespace remoting
