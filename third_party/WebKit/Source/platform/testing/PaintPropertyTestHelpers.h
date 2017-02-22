// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"

namespace blink {
namespace testing {

static inline PassRefPtr<EffectPaintPropertyNode> createOpacityOnlyEffect(
    PassRefPtr<const EffectPaintPropertyNode> parent,
    float opacity) {
  RefPtr<TransformPaintPropertyNode> localTransformSpace =
      const_cast<TransformPaintPropertyNode*>(parent->localTransformSpace());
  RefPtr<ClipPaintPropertyNode> outputClip =
      const_cast<ClipPaintPropertyNode*>(parent->outputClip());
  return EffectPaintPropertyNode::create(
      std::move(parent), std::move(localTransformSpace), std::move(outputClip),
      ColorFilterNone, CompositorFilterOperations(), opacity,
      SkBlendMode::kSrcOver);
}

static inline PaintChunkProperties defaultPaintChunkProperties() {
  PropertyTreeState propertyTreeState(TransformPaintPropertyNode::root(),
                                      ClipPaintPropertyNode::root(),
                                      EffectPaintPropertyNode::root());
  PaintChunkProperties defaultProperties(propertyTreeState);

  return defaultProperties;
}

}  // namespace testing
}  // namespace blink
