// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include "base/task.h"
#include "media/base/callback.h"
#include "remoting/host/user_authenticator.h"
#include "remoting/proto/auth.pb.h"

namespace remoting {

ClientSession::ClientSession(
    EventHandler* event_handler,
    UserAuthenticator* user_authenticator,
    scoped_refptr<protocol::ConnectionToClient> connection,
    protocol::InputStub* input_stub)
    : event_handler_(event_handler),
      user_authenticator_(user_authenticator),
      connection_(connection),
      input_stub_(input_stub),
      authenticated_(false) {
}

ClientSession::~ClientSession() {
}

void ClientSession::SuggestResolution(
    const protocol::SuggestResolutionRequest* msg, Task* done) {
  media::AutoTaskRunner done_runner(done);

  if (!authenticated_) {
    LOG(WARNING) << "Invalid control message received "
                 << "(client not authenticated).";
    return;
  }
}

void ClientSession::BeginSessionRequest(
    const protocol::LocalLoginCredentials* credentials, Task* done) {
  DCHECK(event_handler_);

  media::AutoTaskRunner done_runner(done);

  bool success = false;
  switch (credentials->type()) {
    case protocol::PASSWORD:
      success = user_authenticator_->Authenticate(credentials->username(),
                                                  credentials->credential());
      break;

    default:
      LOG(ERROR) << "Invalid credentials type " << credentials->type();
      break;
  }

  OnAuthorizationComplete(success);
}

void ClientSession::OnAuthorizationComplete(bool success) {
  if (success) {
    authenticated_ = true;
    event_handler_->LocalLoginSucceeded(connection_.get());
  } else {
    LOG(WARNING) << "Login failed";
    event_handler_->LocalLoginFailed(connection_.get());
  }
}

void ClientSession::InjectKeyEvent(const protocol::KeyEvent* event,
                                   Task* done) {
  media::AutoTaskRunner done_runner(done);
  if (authenticated_) {
    done_runner.release();
    input_stub_->InjectKeyEvent(event, done);
  }
}

void ClientSession::InjectMouseEvent(const protocol::MouseEvent* event,
                                     Task* done) {
  media::AutoTaskRunner done_runner(done);
  if (authenticated_) {
    done_runner.release();
    input_stub_->InjectMouseEvent(event, done);
  }
}

void ClientSession::Disconnect() {
  connection_->Disconnect();
  authenticated_ = false;
}

}  // namespace remoting
