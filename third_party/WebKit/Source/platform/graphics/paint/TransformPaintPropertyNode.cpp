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
                    base::AdoptRef(new TransformPaintPropertyNode(
                        nullptr, TransformationMatrix(), FloatPoint3D(), false,
                        0, CompositingReason::kNone, CompositorElementId(),
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

std::unique_ptr<JSONObject> TransformPaintPropertyNode::ToJSON() const {
  auto json = JSONObject::Create();
  if (Parent())
    json->SetString("parent", String::Format("%p", Parent()));
  if (!matrix_.IsIdentity())
    json->SetString("matrix", matrix_.ToString());
  if (!matrix_.IsIdentityOrTranslation())
    json->SetString("origin", origin_.ToString());
  if (!flattens_inherited_transform_)
    json->SetBoolean("flattensInheritedTransform", false);
  if (rendering_context_id_) {
    json->SetString("renderingContextId",
                    String::Format("%x", rendering_context_id_));
  }
  if (direct_compositing_reasons_ != CompositingReason::kNone) {
    json->SetString("directCompositingReasons",
                    CompositingReason::ToString(direct_compositing_reasons_));
  }
  if (compositor_element_id_) {
    json->SetString("compositorElementId",
                    compositor_element_id_.ToString().c_str());
  }
  if (scroll_)
    json->SetString("scroll", String::Format("%p", scroll_.get()));
  return json;
}

#if DCHECK_IS_ON()

String TransformPaintPropertyNode::ToTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::TransformPaintPropertyNode>()
      .PathAsString(this);
}

#endif

}  // namespace blink
