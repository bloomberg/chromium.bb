// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/cast_extension.h"

#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/cast_extension_session.h"
#include "remoting/protocol/transport_context.h"

namespace remoting {

const char kCapability[] = "casting";

CastExtension::CastExtension(
    scoped_refptr<protocol::TransportContext> transport_context)
    : transport_context_(transport_context) {}

CastExtension::~CastExtension() {}

std::string CastExtension::capability() const {
  return kCapability;
}

scoped_ptr<HostExtensionSession> CastExtension::CreateExtensionSession(
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub) {
  return CastExtensionSession::Create(transport_context_,
                                      client_session_control, client_stub);
}

}  // namespace remoting

