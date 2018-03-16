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

#ifndef ElementShadow_h
#define ElementShadow_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/dom/ShadowRoot.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class ElementShadowV0;

class CORE_EXPORT ElementShadow final : public GarbageCollected<ElementShadow>,
                                        public TraceWrapperBase {
 public:
  static ElementShadow* Create();

  Element& Host() const {
    DCHECK(shadow_root_);
    return shadow_root_->host();
  }

  ShadowRoot& GetShadowRoot() const {
    DCHECK(shadow_root_);
    return *shadow_root_;
  }
  ElementShadow* ContainingShadow() const;

  ShadowRoot& AddShadowRoot(Element& shadow_host, ShadowRootType);

  void Attach(const Node::AttachContext&);
  void Detach(const Node::AttachContext&);

  void DistributeIfNeeded();

  void SetNeedsDistributionRecalcWillBeSetNeedsAssignmentRecalc();
  void SetNeedsDistributionRecalc();
  bool NeedsDistributionRecalc() const { return needs_distribution_recalc_; }

  bool IsV1() const { return GetShadowRoot().IsV1(); }
  bool IsOpenOrV0() const { return GetShadowRoot().IsOpenOrV0(); }

  ElementShadowV0& V0() const {
    DCHECK(element_shadow_v0_);
    return *element_shadow_v0_;
  }

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const override;
  const char* NameInHeapSnapshot() const override { return "ElementShadow"; }

 private:
  ElementShadow();

  void Distribute();

  TraceWrapperMember<ElementShadowV0> element_shadow_v0_;
  TraceWrapperMember<ShadowRoot> shadow_root_;
  bool needs_distribution_recalc_;
  DISALLOW_COPY_AND_ASSIGN(ElementShadow);
};

inline ShadowRoot* Node::GetShadowRoot() const {
  if (!IsElementNode())
    return nullptr;
  return ToElement(this)->GetShadowRoot();
}

inline ShadowRoot* Element::GetShadowRoot() const {
  if (ElementShadow* shadow = Shadow())
    return &shadow->GetShadowRoot();
  return nullptr;
}

inline ShadowRoot* Element::ShadowRootIfV1() const {
  ShadowRoot* root = GetShadowRoot();
  if (root && root->IsV1())
    return root;
  return nullptr;
}

inline ElementShadow* ElementShadow::ContainingShadow() const {
  if (ShadowRoot* parent_root = Host().ContainingShadowRoot())
    return parent_root->Owner();
  return nullptr;
}

inline void ElementShadow::DistributeIfNeeded() {
  if (needs_distribution_recalc_)
    Distribute();
  needs_distribution_recalc_ = false;
}

}  // namespace blink

#endif
