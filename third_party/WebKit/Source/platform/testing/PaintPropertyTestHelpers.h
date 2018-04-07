// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_PROPERTY_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_PROPERTY_TEST_HELPERS_H_

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

namespace blink {
namespace test {

static inline scoped_refptr<EffectPaintPropertyNode> CreateOpacityOnlyEffect(
    scoped_refptr<const EffectPaintPropertyNode> parent,
    float opacity) {
  scoped_refptr<TransformPaintPropertyNode> local_transform_space =
      const_cast<TransformPaintPropertyNode*>(parent->LocalTransformSpace());
  scoped_refptr<ClipPaintPropertyNode> output_clip =
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

}  // namespace test
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_PROPERTY_TEST_HELPERS_H_
