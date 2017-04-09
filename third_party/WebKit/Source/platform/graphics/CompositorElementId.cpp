// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorElementId.h"

namespace blink {

CompositorElementId CreateCompositorElementId(
    int dom_node_id,
    CompositorSubElementId sub_element_id) {
  return CompositorElementId(dom_node_id, static_cast<int>(sub_element_id));
}

}  // namespace blink
