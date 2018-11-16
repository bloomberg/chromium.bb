// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

#include <memory>

namespace blink {

const PropertyTreeState& PropertyTreeState::Root() {
  DEFINE_STATIC_LOCAL(
      PropertyTreeState, root,
      (&TransformPaintPropertyNode::Root(), &ClipPaintPropertyNode::Root(),
       &EffectPaintPropertyNode::Root()));
  return root;
}

PropertyTreeState PropertyTreeState::Unalias() const {
  return PropertyTreeState(transform_ ? transform_->Unalias() : nullptr,
                           clip_ ? clip_->Unalias() : nullptr,
                           effect_ ? effect_->Unalias() : nullptr);
}

String PropertyTreeState::ToString() const {
  return String::Format("t:%p c:%p e:%p", Transform(), Clip(), Effect());
}

#if DCHECK_IS_ON()

String PropertyTreeState::ToTreeString() const {
  return "transform:\n" + (Transform() ? Transform()->ToTreeString() : "null") +
         "\nclip:\n" + (Clip() ? Clip()->ToTreeString() : "null") +
         "\neffect:\n" + (Effect() ? Effect()->ToTreeString() : "null");
}

#endif

size_t PropertyTreeState::CacheMemoryUsageInBytes() const {
  return Clip()->CacheMemoryUsageInBytes() +
         Transform()->CacheMemoryUsageInBytes();
}

std::ostream& operator<<(std::ostream& os, const PropertyTreeState& state) {
  return os << state.ToString().Utf8().data();
}

}  // namespace blink
