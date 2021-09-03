// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_devtools_support.h"

#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

absl::optional<PermissionsPolicyBlockLocator> TracePermissionsPolicyBlockSource(
    Frame* frame,
    mojom::PermissionsPolicyFeature feature) {
  const PermissionsPolicy* current_policy =
      frame->GetSecurityContext()->GetPermissionsPolicy();
  DCHECK(current_policy);
  if (current_policy->IsFeatureEnabled(feature))
    return absl::nullopt;

  Frame* current_frame = frame;
  Frame* child_frame = nullptr;

  // Trace up the frame tree until feature is not disabled by inherited policy
  // in |current_frame|.
  // After the trace up, the only 2 possibilities for a feature to be disabled
  // become
  // - The HTTP header of |current_frame|.
  // - The iframe attribute on |child_frame|'s html frame owner element.
  while (true) {
    DCHECK(current_frame);
    current_policy =
        current_frame->GetSecurityContext()->GetPermissionsPolicy();
    DCHECK(current_policy);

    if (current_policy->IsFeatureEnabledByInheritedPolicy(feature))
      break;

    child_frame = current_frame;
    current_frame = current_frame->Tree().Parent();
  }

  const PermissionsPolicy::Allowlist allowlist =
      current_policy->GetAllowlistForDevTools(feature);

  bool allowed_by_current_frame = allowlist.Contains(
      current_frame->GetSecurityContext()->GetSecurityOrigin()->ToUrlOrigin());
  bool allowed_by_child_frame =
      child_frame ? allowlist.Contains(child_frame->GetSecurityContext()
                                           ->GetSecurityOrigin()
                                           ->ToUrlOrigin())
                  : true;

  if (!allowed_by_current_frame || !allowed_by_child_frame) {
    // Feature disabled by allowlist, i.e. value in HTTP header.
    return PermissionsPolicyBlockLocator{
        IdentifiersFactory::FrameId(current_frame),
        PermissionsPolicyBlockReason::kHeader,
    };
  } else {
    // Otherwise, feature must be disabled by iframe attribute.

    // |child_frame| is nullptr iff
    // - feature is disabled in the starting frame (1)
    // - feature is enabled by inherited policy in the starting frame (2)
    // Container policy (iframe attribute) is part of inherited policy.
    // Along with (2), we can conclude feature is enabled by container policy
    // (iframe attribute) which contradicts with the else branch condition.
    DCHECK(child_frame);
    return PermissionsPolicyBlockLocator{
        IdentifiersFactory::FrameId(child_frame),
        PermissionsPolicyBlockReason::kIframeAttribute,
    };
  }
}
}  // namespace blink
