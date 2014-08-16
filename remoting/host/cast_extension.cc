// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/cast_extension.h"

#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/cast_extension_session.h"
#include "remoting/protocol/network_settings.h"

namespace remoting {

const char kCapability[] = "casting";

CastExtension::CastExtension(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    const protocol::NetworkSettings& network_settings)
    : network_task_runner_(network_task_runner),
      url_request_context_getter_(url_request_context_getter),
      network_settings_(network_settings) {
}

CastExtension::~CastExtension() {
}

std::string CastExtension::capability() const {
  return kCapability;
}

scoped_ptr<HostExtensionSession> CastExtension::CreateExtensionSession(
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub) {
  return CastExtensionSession::Create(network_task_runner_,
                                      url_request_context_getter_,
                                      network_settings_,
                                      client_session_control,
                                      client_stub)
      .PassAs<HostExtensionSession>();
}

}  // namespace remoting

