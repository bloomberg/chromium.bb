// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyTreeState_h
#define PropertyTreeState_h

#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/wtf/HashFunctions.h"
#include "platform/wtf/HashTraits.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

// A complete set of paint properties including those that are inherited from
// other objects.
class PLATFORM_EXPORT PropertyTreeState {
  USING_FAST_MALLOC(PropertyTreeState);

 public:
  PropertyTreeState(const TransformPaintPropertyNode* transform,
                    const ClipPaintPropertyNode* clip,
                    const EffectPaintPropertyNode* effect)
      : transform_(transform), clip_(clip), effect_(effect) {}

  bool HasDirectCompositingReasons() const;

  const TransformPaintPropertyNode* Transform() const { return transform_; }
  void SetTransform(const TransformPaintPropertyNode* node) {
    transform_ = node;
  }

  const ClipPaintPropertyNode* Clip() const { return clip_; }
  void SetClip(const ClipPaintPropertyNode* node) { clip_ = node; }

  const EffectPaintPropertyNode* Effect() const { return effect_; }
  void SetEffect(const EffectPaintPropertyNode* node) { effect_ = node; }

  static const PropertyTreeState& Root();

  // Returns the compositor element id, if any, for this property state. If
  // neither the effect nor transform nodes have a compositor element id then a
  // default instance is returned.
  const CompositorElementId GetCompositorElementId(
      const CompositorElementIdSet& element_ids) const;

  void ClearChangedToRoot() const {
    Transform()->ClearChangedToRoot();
    Clip()->ClearChangedToRoot();
    Effect()->ClearChangedToRoot();
  }

  String ToString() const;
#if DCHECK_IS_ON()
  // Dumps the tree from this state up to the root as a string.
  String ToTreeString() const;
#endif

 private:
  const TransformPaintPropertyNode* transform_;
  const ClipPaintPropertyNode* clip_;
  const EffectPaintPropertyNode* effect_;
};

inline bool operator==(const PropertyTreeState& a, const PropertyTreeState& b) {
  return a.Transform() == b.Transform() && a.Clip() == b.Clip() &&
         a.Effect() == b.Effect();
}

inline bool operator!=(const PropertyTreeState& a, const PropertyTreeState& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // PropertyTreeState_h
