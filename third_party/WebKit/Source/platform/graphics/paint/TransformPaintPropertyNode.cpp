// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/TransformPaintPropertyNode.h"

namespace blink {

TransformPaintPropertyNode* TransformPaintPropertyNode::root() {
  DEFINE_STATIC_REF(TransformPaintPropertyNode, root,
                    (TransformPaintPropertyNode::create(
                        nullptr, TransformationMatrix(), FloatPoint3D())));
  return root;
}

String TransformPaintPropertyNode::toString() const {
  return String::format(
      "parent=%p transform=%s origin=%s flattensInheritedTransform=%s "
      "renderContextID=%x directCompositingReasons=%s",
      m_parent.get(), m_matrix.toString().ascii().data(),
      m_origin.toString().ascii().data(),
      m_flattensInheritedTransform ? "yes" : "no", m_renderingContextId,
      compositingReasonsAsString(m_directCompositingReasons).ascii().data());
}

}  // namespace blink
