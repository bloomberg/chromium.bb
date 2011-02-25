// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment.h"

#include "remoting/host/capturer.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/user_authenticator.h"
#include "remoting/proto/auth.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/input_stub.h"

using remoting::protocol::InputStub;

namespace remoting {

DesktopEnvironment::DesktopEnvironment(Capturer* capturer,
                                       InputStub* input_stub)
    : event_handler_(NULL),
      capturer_(capturer),
      input_stub_(input_stub) {
}

DesktopEnvironment::~DesktopEnvironment() {
}

void DesktopEnvironment::SuggestResolution(
    const protocol::SuggestResolutionRequest* msg, Task* done) {
  done->Run();
  delete done;
}

void DesktopEnvironment::BeginSessionRequest(
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
    event_handler_->LocalLoginSucceeded();
  } else {
    LOG(WARNING) << "Login failed for user " << credentials->username();
  }

  done->Run();
  delete done;
}

}  // namespace remoting
