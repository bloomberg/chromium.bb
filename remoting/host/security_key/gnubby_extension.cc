// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/gnubby_extension.h"

#include "base/memory/ptr_util.h"
#include "remoting/host/security_key/gnubby_extension_session.h"

namespace {
// TODO(joedow): Update this once clients support sending a gnubby capabililty.
//               Tracked via: crbug.com/587485
const char kCapability[] = "";
}

namespace remoting {

GnubbyExtension::GnubbyExtension() {}

GnubbyExtension::~GnubbyExtension() {}

std::string GnubbyExtension::capability() const {
  return kCapability;
}

std::unique_ptr<HostExtensionSession> GnubbyExtension::CreateExtensionSession(
    ClientSessionDetails* details,
    protocol::ClientStub* client_stub) {
  // TODO(joedow): Update this mechanism to allow for multiple sessions.  The
  //               extension will only send messages through the initial
  //               |client_stub| and |details| with the current design.
  return base::WrapUnique(new GnubbyExtensionSession(details, client_stub));
}

}  // namespace remoting
