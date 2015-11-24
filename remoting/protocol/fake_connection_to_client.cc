// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_connection_to_client.h"

#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

FakeConnectionToClient::FakeConnectionToClient(scoped_ptr<Session> session)
    : session_(session.Pass()) {}

FakeConnectionToClient::~FakeConnectionToClient() {}

void FakeConnectionToClient::SetEventHandler(EventHandler* event_handler) {
  event_handler_ = event_handler;
}

VideoStub* FakeConnectionToClient::video_stub() {
  return video_stub_;
}

AudioStub* FakeConnectionToClient::audio_stub() {
  return audio_stub_;
}

ClientStub* FakeConnectionToClient::client_stub() {
  return client_stub_;
}

void FakeConnectionToClient::Disconnect(ErrorCode disconnect_error) {
  CHECK(is_connected_);

  is_connected_ = false;
  disconnect_error_ = disconnect_error;
  if (event_handler_)
    event_handler_->OnConnectionClosed(this, disconnect_error_);
}

Session* FakeConnectionToClient::session() {
  return session_.get();
}

void FakeConnectionToClient::OnInputEventReceived(int64_t timestamp) {}

void FakeConnectionToClient::set_clipboard_stub(ClipboardStub* clipboard_stub) {
  clipboard_stub_ = clipboard_stub;
}

void FakeConnectionToClient::set_host_stub(HostStub* host_stub) {
  host_stub_ = host_stub;
}

void FakeConnectionToClient::set_input_stub(InputStub* input_stub) {
  input_stub_ = input_stub;
}

void FakeConnectionToClient::set_video_feedback_stub(
    VideoFeedbackStub* video_feedback_stub) {
  video_feedback_stub_ = video_feedback_stub;
}

}  // namespace protocol
}  // namespace remoting
