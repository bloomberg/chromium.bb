/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#ifndef ShadowRootRareDataV0_h
#define ShadowRootRareDataV0_h

#include "core/dom/V0InsertionPoint.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ShadowRootRareDataV0 : public GarbageCollected<ShadowRootRareDataV0> {
 public:
  ShadowRootRareDataV0()
      : descendant_shadow_element_count_(0),
        descendant_content_element_count_(0) {}

  HTMLShadowElement* ShadowInsertionPointOfYoungerShadowRoot() const {
    return shadow_insertion_point_of_younger_shadow_root_.Get();
  }
  void SetShadowInsertionPointOfYoungerShadowRoot(
      HTMLShadowElement* shadow_insertion_point) {
    shadow_insertion_point_of_younger_shadow_root_ = shadow_insertion_point;
  }

  void DidAddInsertionPoint(V0InsertionPoint*);
  void DidRemoveInsertionPoint(V0InsertionPoint*);

  bool ContainsShadowElements() const {
    return descendant_shadow_element_count_;
  }
  bool ContainsContentElements() const {
    return descendant_content_element_count_;
  }

  unsigned DescendantShadowElementCount() const {
    return descendant_shadow_element_count_;
  }

  const HeapVector<Member<V0InsertionPoint>>& DescendantInsertionPoints() {
    return descendant_insertion_points_;
  }
  void SetDescendantInsertionPoints(
      HeapVector<Member<V0InsertionPoint>>& list) {
    descendant_insertion_points_.swap(list);
  }
  void ClearDescendantInsertionPoints() {
    descendant_insertion_points_.clear();
  }

  void SetYoungerShadowRoot(ShadowRoot& younger_shadow_root) {
    younger_shadow_root_ = &younger_shadow_root;
  }
  void SetOlderShadowRoot(ShadowRoot& older_shadow_root) {
    older_shadow_root_ = &older_shadow_root;
  }

  ShadowRoot* YoungerShadowRoot() const { return younger_shadow_root_; }
  ShadowRoot* OlderShadowRoot() const { return older_shadow_root_; }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(younger_shadow_root_);
    visitor->Trace(older_shadow_root_);
    visitor->Trace(shadow_insertion_point_of_younger_shadow_root_);
    visitor->Trace(descendant_insertion_points_);
  }

 private:
  Member<ShadowRoot> younger_shadow_root_;
  Member<ShadowRoot> older_shadow_root_;
  Member<HTMLShadowElement> shadow_insertion_point_of_younger_shadow_root_;
  unsigned descendant_shadow_element_count_;
  unsigned descendant_content_element_count_;
  HeapVector<Member<V0InsertionPoint>> descendant_insertion_points_;
};

inline void ShadowRootRareDataV0::DidAddInsertionPoint(
    V0InsertionPoint* point) {
  DCHECK(point);
  if (IsHTMLShadowElement(*point))
    ++descendant_shadow_element_count_;
  else if (IsHTMLContentElement(*point))
    ++descendant_content_element_count_;
  else
    NOTREACHED();
}

inline void ShadowRootRareDataV0::DidRemoveInsertionPoint(
    V0InsertionPoint* point) {
  DCHECK(point);
  if (IsHTMLShadowElement(*point)) {
    DCHECK_GT(descendant_shadow_element_count_, 0u);
    --descendant_shadow_element_count_;
  } else if (IsHTMLContentElement(*point)) {
    DCHECK_GT(descendant_content_element_count_, 0u);
    --descendant_content_element_count_;
  } else {
    NOTREACHED();
  }
}

}  // namespace blink

#endif
