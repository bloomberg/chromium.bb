// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorElementId.h"

namespace blink {

CompositorElementId CreateCompositorElementId(
    DOMNodeId dom_node_id,
    CompositorSubElementId sub_element_id) {
  DCHECK(dom_node_id > 0 &&
         dom_node_id < std::numeric_limits<DOMNodeId>::max() /
                           static_cast<unsigned>(
                               CompositorSubElementId::kNumSubElementTypes));
  cc::ElementIdType id =
      dom_node_id << 2;  // Shift to make room for sub_element_id enum bits.
  id += static_cast<uint64_t>(sub_element_id);
  return CompositorElementId(id);
}

DOMNodeId DomNodeIdFromCompositorElementId(CompositorElementId element_id) {
  return element_id.id_ >> 2;
}

CompositorSubElementId SubElementIdFromCompositorElementId(
    CompositorElementId element_id) {
  return static_cast<CompositorSubElementId>(
      element_id.id_ %
      static_cast<unsigned>(CompositorSubElementId::kNumSubElementTypes));
}

}  // namespace blink
