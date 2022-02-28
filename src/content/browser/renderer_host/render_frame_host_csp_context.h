// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_CSP_CONTEXT_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_CSP_CONTEXT_H_

#include "services/network/public/cpp/content_security_policy/csp_context.h"

class GURL;

namespace content {

class RenderFrameHostImpl;

// RenderFrameHostCSPContext is a network::CSPContext that reports Content
// Security Policy violations through the mojo connection between a
// RenderFrameHostImpl and its corresponding LocalFrame.
class RenderFrameHostCSPContext : public network::CSPContext {
 public:
  // Construct a new RenderFrameHostCSPContext reporting CSP violations through
  // `render_frame_host`. The parameter `render_frame_host` can be null, in
  // which case this won't report any violations.
  explicit RenderFrameHostCSPContext(RenderFrameHostImpl* render_frame_host);

  // network::CSPContext
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation_params) override;
  void SanitizeDataForUseInCspViolation(
      bool is_redirect,
      network::mojom::CSPDirectiveName directive,
      GURL* blocked_url,
      network::mojom::SourceLocation* source_location) const override;

 private:
  RenderFrameHostImpl* render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_CSP_CONTEXT_H_
