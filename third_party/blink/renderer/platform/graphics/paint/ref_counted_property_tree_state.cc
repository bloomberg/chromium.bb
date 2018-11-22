// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/ref_counted_property_tree_state.h"

#include <memory>

namespace blink {

const RefCountedPropertyTreeState& RefCountedPropertyTreeState::Root() {
  DEFINE_STATIC_LOCAL(
      std::unique_ptr<RefCountedPropertyTreeState>, root,
      (std::make_unique<RefCountedPropertyTreeState>(
          &TransformPaintPropertyNode::Root(), &ClipPaintPropertyNode::Root(),
          &EffectPaintPropertyNode::Root())));
  return *root;
}

}  // namespace blink
