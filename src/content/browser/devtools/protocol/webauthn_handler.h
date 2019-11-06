// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/web_authn.h"

namespace content {
namespace protocol {

class WebAuthnHandler : public DevToolsDomainHandler, public WebAuthn::Backend {
 public:
  WebAuthnHandler();
  ~WebAuthnHandler() override;

  // DevToolsDomainHandler:
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  void Wire(UberDispatcher* dispatcher) override;

  // WebAuthn::Backend
  Response Enable() override;
  Response Disable() override;

 private:
  RenderFrameHostImpl* frame_host_;
  DISALLOW_COPY_AND_ASSIGN(WebAuthnHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_
