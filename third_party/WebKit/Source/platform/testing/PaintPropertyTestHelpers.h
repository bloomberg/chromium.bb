// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTestHelpers_h
#define PaintPropertyTestHelpers_h

#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"

namespace blink {
namespace testing {

static inline PassRefPtr<EffectPaintPropertyNode> CreateOpacityOnlyEffect(
    PassRefPtr<const EffectPaintPropertyNode> parent,
    float opacity) {
  RefPtr<TransformPaintPropertyNode> local_transform_space =
      const_cast<TransformPaintPropertyNode*>(parent->LocalTransformSpace());
  RefPtr<ClipPaintPropertyNode> output_clip =
      const_cast<ClipPaintPropertyNode*>(parent->OutputClip());
  return EffectPaintPropertyNode::Create(
      std::move(parent), std::move(local_transform_space),
      std::move(output_clip), kColorFilterNone, CompositorFilterOperations(),
      opacity, SkBlendMode::kSrcOver);
}

static inline PaintChunkProperties DefaultPaintChunkProperties() {
  PaintChunkProperties default_properties(PropertyTreeState::Root());

  return default_properties;
}

}  // namespace testing
}  // namespace blink

#endif  // PaintPropertyTestHelpers_h
