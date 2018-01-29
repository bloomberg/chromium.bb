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

#ifndef ElementShadowV0_h
#define ElementShadowV0_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/css/SelectRuleFeatureSet.h"
#include "core/dom/V0InsertionPoint.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT ElementShadowV0 final
    : public GarbageCollectedFinalized<ElementShadowV0>,
      public TraceWrapperBase {
 public:
  static ElementShadowV0* Create(ElementShadow&);
  ~ElementShadowV0();

  void WillAffectSelector();
  const SelectRuleFeatureSet& EnsureSelectFeatureSet();

  const V0InsertionPoint* FinalDestinationInsertionPointFor(const Node*) const;
  const DestinationInsertionPoints* DestinationInsertionPointsFor(
      const Node*) const;

  void Distribute();
  void DidDistributeNode(const Node*, V0InsertionPoint*);
  void ClearDistribution();

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  explicit ElementShadowV0(ElementShadow&);

  ShadowRoot& YoungestShadowRoot() const;
  ShadowRoot& OldestShadowRoot() const;

  void DistributeNodeChildrenTo(V0InsertionPoint*, ContainerNode*);

  void CollectSelectFeatureSetFrom(const ShadowRoot&);
  bool NeedsSelectFeatureSet() const { return needs_select_feature_set_; }
  void SetNeedsSelectFeatureSet() { needs_select_feature_set_ = true; }

  Member<ElementShadow> element_shadow_;
  using NodeToDestinationInsertionPoints =
      HeapHashMap<Member<const Node>, Member<DestinationInsertionPoints>>;
  NodeToDestinationInsertionPoints node_to_insertion_points_;
  SelectRuleFeatureSet select_features_;
  bool needs_select_feature_set_;
  DISALLOW_COPY_AND_ASSIGN(ElementShadowV0);
};

}  // namespace blink

#endif
