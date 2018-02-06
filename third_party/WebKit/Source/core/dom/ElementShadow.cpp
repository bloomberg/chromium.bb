/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/ElementShadow.h"

#include "core/css/StyleChangeReason.h"
#include "core/css/StyleSheetList.h"
#include "core/css/resolver/ScopedStyleResolver.h"
#include "core/dom/ElementShadowV0.h"
#include "core/dom/NodeTraversal.h"
#include "core/frame/Deprecation.h"
#include "core/probe/CoreProbes.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/bindings/ScriptForbiddenScope.h"

namespace blink {

ElementShadow* ElementShadow::Create() {
  return new ElementShadow();
}

ElementShadow::ElementShadow() : needs_distribution_recalc_(false) {}

ShadowRoot& ElementShadow::AddShadowRoot(Element& shadow_host,
                                         ShadowRootType type) {
  EventDispatchForbiddenScope assert_no_event_dispatch;
  ScriptForbiddenScope forbid_script;

  DCHECK(!shadow_root_);
  if (type == ShadowRootType::V0) {
    DCHECK(!element_shadow_v0_);
    element_shadow_v0_ = ElementShadowV0::Create(*this);
  }

  shadow_root_ = ShadowRoot::Create(shadow_host.GetDocument(), type);
  shadow_root_->SetParentOrShadowHostNode(&shadow_host);
  shadow_root_->SetParentTreeScope(shadow_host.GetTreeScope());
  if (type == ShadowRootType::V0) {
    SetNeedsDistributionRecalc();
  } else {
    for (Node& child : NodeTraversal::ChildrenOf(shadow_host))
      child.LazyReattachIfAttached();
  }

  shadow_root_->InsertedInto(&shadow_host);
  shadow_host.SetChildNeedsStyleRecalc();
  shadow_host.SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(StyleChangeReason::kShadow));

  probe::didPushShadowRoot(&shadow_host, shadow_root_);

  return *shadow_root_;
}

void ElementShadow::Attach(const Node::AttachContext& context) {
  Node::AttachContext children_context(context);
  ShadowRoot& root = GetShadowRoot();
  if (root.NeedsAttach())
    root.AttachLayoutTree(children_context);
}

void ElementShadow::Detach(const Node::AttachContext& context) {
  Node::AttachContext children_context(context);
  children_context.clear_invalidation = true;
  GetShadowRoot().DetachLayoutTree(children_context);
}

void ElementShadow::SetNeedsDistributionRecalcWillBeSetNeedsAssignmentRecalc() {
  if (RuntimeEnabledFeatures::IncrementalShadowDOMEnabled() && IsV1())
    GetShadowRoot().SetNeedsAssignmentRecalc();
  else
    SetNeedsDistributionRecalc();
}

void ElementShadow::SetNeedsDistributionRecalc() {
  DCHECK(!(RuntimeEnabledFeatures::IncrementalShadowDOMEnabled() && IsV1()));
  if (needs_distribution_recalc_)
    return;
  needs_distribution_recalc_ = true;
  Host().MarkAncestorsWithChildNeedsDistributionRecalc();
  if (!IsV1())
    V0().ClearDistribution();
}

void ElementShadow::Distribute() {
  if (IsV1())
    GetShadowRoot().DistributeV1();
  else
    V0().Distribute();
}

void ElementShadow::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_shadow_v0_);
  visitor->Trace(shadow_root_);
}

void ElementShadow::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(element_shadow_v0_);
  visitor->TraceWrappers(shadow_root_);
}

}  // namespace blink
