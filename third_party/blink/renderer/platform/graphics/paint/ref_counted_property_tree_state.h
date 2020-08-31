// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REF_COUNTED_PROPERTY_TREE_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REF_COUNTED_PROPERTY_TREE_STATE_H_

#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

namespace blink {

// A complete set of paint properties including those that are inherited from
// other objects.  RefPtrs are used to guard against use-after-free bugs.
class PLATFORM_EXPORT RefCountedPropertyTreeState {
  USING_FAST_MALLOC(RefCountedPropertyTreeState);

 public:
  RefCountedPropertyTreeState(const PropertyTreeState& property_tree_state)
      : transform_(&property_tree_state.Transform()),
        clip_(&property_tree_state.Clip()),
        effect_(&property_tree_state.Effect()) {}

  bool HasDirectCompositingReasons() const;

  const TransformPaintPropertyNode& Transform() const { return *transform_; }
  const ClipPaintPropertyNode& Clip() const { return *clip_; }
  const EffectPaintPropertyNode& Effect() const { return *effect_; }

  PropertyTreeState GetPropertyTreeState() const {
    return PropertyTreeState(Transform(), Clip(), Effect());
  }

  void ClearChangedToRoot() const {
    Transform().ClearChangedToRoot();
    Clip().ClearChangedToRoot();
    Effect().ClearChangedToRoot();
  }

  String ToString() const { return GetPropertyTreeState().ToString(); }
#if DCHECK_IS_ON()
  // Dumps the tree from this state up to the root as a string.
  String ToTreeString() const { return GetPropertyTreeState().ToTreeString(); }
#endif

 private:
  scoped_refptr<const TransformPaintPropertyNode> transform_;
  scoped_refptr<const ClipPaintPropertyNode> clip_;
  scoped_refptr<const EffectPaintPropertyNode> effect_;
};

inline bool operator==(const RefCountedPropertyTreeState& a,
                       const RefCountedPropertyTreeState& b) {
  return &a.Transform() == &b.Transform() && &a.Clip() == &b.Clip() &&
         &a.Effect() == &b.Effect();
}

inline bool operator!=(const RefCountedPropertyTreeState& a,
                       const RefCountedPropertyTreeState& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_REF_COUNTED_PROPERTY_TREE_STATE_H_
