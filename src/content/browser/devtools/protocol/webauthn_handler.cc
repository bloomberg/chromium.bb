// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/webauthn_handler.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment_impl.h"

namespace content {
namespace protocol {

WebAuthnHandler::WebAuthnHandler()
    : DevToolsDomainHandler(WebAuthn::Metainfo::domainName) {}

WebAuthnHandler::~WebAuthnHandler() = default;

void WebAuthnHandler::SetRenderer(int process_host_id,
                                  RenderFrameHostImpl* frame_host) {
  frame_host_ = frame_host;
}

void WebAuthnHandler::Wire(UberDispatcher* dispatcher) {
  WebAuthn::Dispatcher::wire(dispatcher, this);
}

Response WebAuthnHandler::Enable() {
  AuthenticatorEnvironmentImpl::GetInstance()->EnableVirtualAuthenticatorFor(
      frame_host_);
  return Response::OK();
}

Response WebAuthnHandler::Disable() {
  AuthenticatorEnvironmentImpl::GetInstance()->DisableVirtualAuthenticatorFor(
      frame_host_);
  return Response::OK();
}

}  // namespace protocol
}  // namespace content
