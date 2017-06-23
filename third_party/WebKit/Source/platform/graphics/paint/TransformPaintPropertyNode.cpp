// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/TransformPaintPropertyNode.h"

#include "platform/graphics/paint/PropertyTreeState.h"

namespace blink {

// The root of the transform tree. The root transform node references the root
// scroll node.
TransformPaintPropertyNode* TransformPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(TransformPaintPropertyNode, root,
                    AdoptRef(new TransformPaintPropertyNode(
                        nullptr, TransformationMatrix(), FloatPoint3D(), false,
                        0, kCompositingReasonNone, CompositorElementId(),
                        ScrollPaintPropertyNode::Root())));
  return root;
}

const ScrollPaintPropertyNode*
TransformPaintPropertyNode::FindEnclosingScrollNode() const {
  if (scroll_)
    return scroll_.Get();

  for (const auto* ancestor = Parent(); ancestor;
       ancestor = ancestor->Parent()) {
    if (const auto* scroll_node = ancestor->ScrollNode())
      return scroll_node;
  }
  // The root transform node references the root scroll node so a scroll node
  // should always exist.
  NOTREACHED();
  return nullptr;
}

String TransformPaintPropertyNode::ToString() const {
  auto transform = String::Format(
      "parent=%p transform=%s origin=%s flattensInheritedTransform=%s "
      "renderingContextId=%x directCompositingReasons=%s "
      "compositorElementId=%lu",
      Parent(), matrix_.ToString().Ascii().data(),
      origin_.ToString().Ascii().data(),
      flattens_inherited_transform_ ? "yes" : "no", rendering_context_id_,
      CompositingReasonsAsString(direct_compositing_reasons_).Ascii().data(),
      static_cast<unsigned long>(compositor_element_id_.id_));
  if (scroll_)
    return transform + " scroll=" + scroll_->ToString();
  return transform;
}

#if DCHECK_IS_ON()

String TransformPaintPropertyNode::ToTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::TransformPaintPropertyNode>()
      .PathAsString(this);
}

#endif

}  // namespace blink
