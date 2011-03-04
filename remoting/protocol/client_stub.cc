// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface of a client that receives commands from a Chromoting host.
//
// This interface is responsible for a subset of control messages sent to
// the Chromoting client.

#include "remoting/protocol/client_stub.h"

namespace remoting {
namespace protocol {

ClientStub::ClientStub() : authenticated_(false) {
}

ClientStub::~ClientStub() {
}

void ClientStub::OnAuthenticated() {
  authenticated_ = true;
}

bool ClientStub::authenticated() {
  return authenticated_;
}

}  // namespace protocol
}  // namespace remoting
