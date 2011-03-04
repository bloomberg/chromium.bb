// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface of a host that receives commands from a Chromoting client.
//
// This interface handles control messages defined in contro.proto.

#include "remoting/protocol/host_stub.h"

namespace remoting {
namespace protocol {

HostStub::HostStub() : authenticated_(false) {
}

HostStub::~HostStub() {
}

void HostStub::OnAuthenticated() {
  authenticated_ = true;
}

bool HostStub::authenticated() {
  return authenticated_;
}

}  // namespace protocol
}  // namespace remoting
