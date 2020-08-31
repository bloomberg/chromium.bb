// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CONTENT_SECURITY_POLICY_UTIL_H_
#define CONTENT_RENDERER_CONTENT_SECURITY_POLICY_UTIL_H_

#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"

namespace content {

// Convert a WebContentSecurityPolicy into a ContentSecurityPolicy. These two
// classes represent the exact same thing, but one is in content, the other is
// in blink.
// TODO(arthursonzogni): Remove this when BeginNavigation IPC will be called
// directly from blink.
network::mojom::ContentSecurityPolicyPtr BuildContentSecurityPolicy(
    const blink::WebContentSecurityPolicy&);

// TODO(arthursonzogni): Remove this when BeginNavigation IPC will be called
// directly from blink.
network::mojom::CSPSourcePtr BuildCSPSource(
    const blink::WebContentSecurityPolicySourceExpression&);

}  // namespace content

#endif /* CONTENT_RENDERER_CONTENT_SECURITY_POLICY_UTIL_H_ */
