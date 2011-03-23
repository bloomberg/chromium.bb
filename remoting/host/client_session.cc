// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "remoting/host/user_authenticator.h"
#include "remoting/proto/auth.pb.h"

namespace remoting {

ClientSession::ClientSession(
    EventHandler* event_handler,
    scoped_refptr<protocol::ConnectionToClient> connection)
    : event_handler_(event_handler),
      connection_(connection) {
}

ClientSession::~ClientSession() {
}

void ClientSession::SuggestResolution(
    const protocol::SuggestResolutionRequest* msg, Task* done) {
  done->Run();
  delete done;
}

void ClientSession::BeginSessionRequest(
    const protocol::LocalLoginCredentials* credentials, Task* done) {
  DCHECK(event_handler_);

  bool success = false;
  scoped_ptr<UserAuthenticator> authenticator(UserAuthenticator::Create());
  switch (credentials->type()) {
    case protocol::PASSWORD:
      success = authenticator->Authenticate(credentials->username(),
                                            credentials->credential());
      break;

    default:
      LOG(ERROR) << "Invalid credentials type " << credentials->type();
      break;
  }

  if (success) {
    event_handler_->LocalLoginSucceeded(connection_.get());
  } else {
    LOG(WARNING) << "Login failed for user " << credentials->username();
    event_handler_->LocalLoginFailed(connection_.get());
  }

  done->Run();
  delete done;
}

void ClientSession::Disconnect() {
  connection_->Disconnect();
}

protocol::ConnectionToClient* ClientSession::connection() const {
  return connection_.get();
}

}  // namespace remoting
