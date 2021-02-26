// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_IFRAME_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_IFRAME_POLICY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/feature_policy/dom_feature_policy.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// IFramePolicy inherits Policy. It represents the feature policy of an iframe
// contained in a document, as seen from that document (not including any
// information private to that frame). It is synthesized from the parent
// document's policy and the iframe's container policy.
class IFramePolicy final : public DOMFeaturePolicy {
 public:
  ~IFramePolicy() override = default;

  // Create a new IFramePolicy, which is synthetic for a frame contained within
  // a document.
  IFramePolicy(ExecutionContext* parent_context,
               const ParsedFeaturePolicy& container_policy,
               scoped_refptr<const SecurityOrigin> src_origin)
      : DOMFeaturePolicy(parent_context) {
    DCHECK(src_origin);
    UpdateContainerPolicy(container_policy, src_origin);
  }

  void UpdateContainerPolicy(
      const ParsedFeaturePolicy& container_policy,
      scoped_refptr<const SecurityOrigin> src_origin) override {
    policy_ = FeaturePolicy::CreateFromParentPolicy(
        context_->GetSecurityContext().GetFeaturePolicy(), container_policy,
        src_origin->ToUrlOrigin());
  }

 protected:
  const FeaturePolicy* GetPolicy() const override { return policy_.get(); }

 private:
  std::unique_ptr<FeaturePolicy> policy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_IFRAME_POLICY_H_
