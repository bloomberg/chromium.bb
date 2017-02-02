// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/TransformPaintPropertyNode.h"

#include "platform/graphics/paint/PropertyTreeState.h"

namespace blink {

// The root of the transform tree. The root transform node references the root
// scroll node.
TransformPaintPropertyNode* TransformPaintPropertyNode::root() {
  DEFINE_STATIC_REF(TransformPaintPropertyNode, root,
                    adoptRef(new TransformPaintPropertyNode(
                        nullptr, TransformationMatrix(), FloatPoint3D(), false,
                        0, CompositingReasonNone, CompositorElementId(),
                        ScrollPaintPropertyNode::root())));
  return root;
}

const ScrollPaintPropertyNode*
TransformPaintPropertyNode::findEnclosingScrollNode() const {
  if (m_scroll)
    return m_scroll.get();

  for (const auto* ancestor = parent(); ancestor;
       ancestor = ancestor->parent()) {
    if (const auto* scrollNode = ancestor->scrollNode())
      return scrollNode;
  }
  // The root transform node references the root scroll node so a scroll node
  // should always exist.
  NOTREACHED();
  return nullptr;
}

String TransformPaintPropertyNode::toString() const {
  auto transform = String::format(
      "parent=%p transform=%s origin=%s flattensInheritedTransform=%s "
      "renderingContextId=%x directCompositingReasons=%s "
      "compositorElementId=(%d, %d)",
      m_parent.get(), m_matrix.toString().ascii().data(),
      m_origin.toString().ascii().data(),
      m_flattensInheritedTransform ? "yes" : "no", m_renderingContextId,
      compositingReasonsAsString(m_directCompositingReasons).ascii().data(),
      m_compositorElementId.primaryId, m_compositorElementId.secondaryId);
  if (m_scroll)
    return transform + " scroll=" + m_scroll->toString();
  return transform;
}

#if DCHECK_IS_ON()

String TransformPaintPropertyNode::toTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::TransformPaintPropertyNode>()
      .pathAsString(this);
}

#endif

}  // namespace blink
