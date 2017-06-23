// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ClipPaintPropertyNode.h"

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/PropertyTreeState.h"

namespace blink {

ClipPaintPropertyNode* ClipPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(ClipPaintPropertyNode, root,
                    (ClipPaintPropertyNode::Create(
                        nullptr, TransformPaintPropertyNode::Root(),
                        FloatRoundedRect(LayoutRect::InfiniteIntRect()))));
  return root;
}

String ClipPaintPropertyNode::ToString() const {
  return String::Format(
      "parent=%p localTransformSpace=%p rect=%s directCompositingReasons=%s",
      Parent(), local_transform_space_.Get(),
      clip_rect_.ToString().Ascii().data(),
      CompositingReasonsAsString(direct_compositing_reasons_).Ascii().data());
}

#if DCHECK_IS_ON()

String ClipPaintPropertyNode::ToTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::ClipPaintPropertyNode>()
      .PathAsString(this);
}

#endif

}  // namespace blink
