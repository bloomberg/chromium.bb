// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/initiator_csp_context.h"

namespace content {

InitiatorCSPContext::InitiatorCSPContext(
    std::vector<network::mojom::ContentSecurityPolicyPtr> policies,
    network::mojom::CSPSourcePtr self_source,
    mojo::PendingRemote<blink::mojom::NavigationInitiator> navigation_initiator)
    : reporting_render_frame_host_impl_(nullptr),
      initiator(std::move(navigation_initiator)) {
  for (auto& policy : policies)
    AddContentSecurityPolicy(std::move(policy));

  SetSelf(std::move(self_source));
}

InitiatorCSPContext::~InitiatorCSPContext() = default;

void InitiatorCSPContext::SetReportingRenderFrameHost(
    RenderFrameHostImpl* rfh) {
  reporting_render_frame_host_impl_ = rfh;
}

void InitiatorCSPContext::ReportContentSecurityPolicyViolation(
    network::mojom::CSPViolationPtr violation) {
  if (initiator)
    initiator->SendViolationReport(std::move(violation));
}

bool InitiatorCSPContext::SchemeShouldBypassCSP(
    const base::StringPiece& scheme) {
  // TODO(andypaicu): RenderFrameHostImpl::SchemeShouldBypassCSP could be
  // static except for the fact that it's virtual. It's weird to use
  // the reporting RFH to do this check but overall harmless.
  if (reporting_render_frame_host_impl_)
    return reporting_render_frame_host_impl_->SchemeShouldBypassCSP(scheme);

  return false;
}

void InitiatorCSPContext::SanitizeDataForUseInCspViolation(
    bool is_redirect,
    network::mojom::CSPDirectiveName directive,
    GURL* blocked_url,
    network::mojom::SourceLocation* source_location) const {
  if (reporting_render_frame_host_impl_) {
    reporting_render_frame_host_impl_->SanitizeDataForUseInCspViolation(
        is_redirect, directive, blocked_url, source_location);
  }
}

}  // namespace content
