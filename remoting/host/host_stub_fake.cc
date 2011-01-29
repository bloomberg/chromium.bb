// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/host_stub_fake.h"
#include "remoting/proto/auth.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/connection_to_client.h"

namespace remoting {

HostStubFake::HostStubFake(ChromotingHost* host)
    : host_(host) {
}

void HostStubFake::SuggestResolution(
    const protocol::SuggestResolutionRequest* msg, Task* done) {
  done->Run();
  delete done;
}

void HostStubFake::BeginSessionRequest(
    const protocol::LocalLoginCredentials* credentials, Task* done) {
  done->Run();
  delete done;

  protocol::LocalLoginStatus* status = new protocol::LocalLoginStatus();
  status->set_success(true);
  host_->connection_to_client()->client_stub()->BeginSessionResponse(
      status, new DeleteTask<protocol::LocalLoginStatus>(status));
  host_->LocalLoginSucceeded();
}

}  // namespace remoting
