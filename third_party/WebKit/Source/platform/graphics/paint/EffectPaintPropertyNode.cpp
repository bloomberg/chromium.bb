// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/EffectPaintPropertyNode.h"

#include "platform/graphics/paint/PropertyTreeState.h"

namespace blink {

EffectPaintPropertyNode* EffectPaintPropertyNode::root() {
  DEFINE_STATIC_REF(
      EffectPaintPropertyNode, root,
      (EffectPaintPropertyNode::create(
          nullptr, TransformPaintPropertyNode::root(),
          ClipPaintPropertyNode::root(), ColorFilterNone,
          CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver)));
  return root;
}

FloatRect EffectPaintPropertyNode::mapRect(const FloatRect& inputRect) const {
  FloatRect rect = inputRect;
  rect.moveBy(-m_paintOffset);
  FloatRect result = m_filter.mapRect(rect);
  result.moveBy(m_paintOffset);
  return result;
}

cc::Layer* EffectPaintPropertyNode::ensureDummyLayer() const {
  if (m_dummyLayer)
    return m_dummyLayer.get();
  m_dummyLayer = cc::Layer::Create();
  return m_dummyLayer.get();
}

String EffectPaintPropertyNode::toString() const {
  return String::format(
      "parent=%p localTransformSpace=%p outputClip=%p opacity=%f filter=%s "
      "blendMode=%s directCompositingReasons=%s compositorElementId=(%d, %d) "
      "paintOffset=%s",
      m_parent.get(), m_localTransformSpace.get(), m_outputClip.get(),
      m_opacity, m_filter.toString().ascii().data(),
      SkBlendMode_Name(m_blendMode),
      compositingReasonsAsString(m_directCompositingReasons).ascii().data(),
      m_compositorElementId.primaryId, m_compositorElementId.secondaryId,
      m_paintOffset.toString().ascii().data());
}

#if DCHECK_IS_ON()

String EffectPaintPropertyNode::toTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::EffectPaintPropertyNode>()
      .pathAsString(this);
}

#endif

}  // namespace blink
