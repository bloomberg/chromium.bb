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

const TransformPaintPropertyNode&
TransformPaintPropertyNode::NearestScrollTranslationNode() const {
  const auto* transform = this;
  while (!transform->ScrollNode()) {
    transform = transform->Parent();
    // The transform should never be null because the root transform has an
    // associated scroll node (see: TransformPaintPropertyNode::Root()).
    DCHECK(transform);
  }
  return *transform;
}

String TransformPaintPropertyNode::ToString() const {
  auto transform = String::Format(
      "parent=%p transform=%s origin=%s flattensInheritedTransform=%s "
      "renderingContextId=%x directCompositingReasons=%s "
      "compositorElementId=%s",
      Parent(), matrix_.ToString().Ascii().data(),
      origin_.ToString().Ascii().data(),
      flattens_inherited_transform_ ? "yes" : "no", rendering_context_id_,
      CompositingReasonsAsString(direct_compositing_reasons_).Ascii().data(),
      compositor_element_id_.ToString().c_str());
  if (scroll_) {
    return String::Format("%s scroll=%p", transform.Utf8().data(),
                          scroll_.Get());
  }
  return transform;
}

#if DCHECK_IS_ON()

String TransformPaintPropertyNode::ToTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::TransformPaintPropertyNode>()
      .PathAsString(this);
}

#endif

}  // namespace blink
