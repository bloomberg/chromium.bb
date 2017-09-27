// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CustomCompositorAnimationManager.h"

#include "core/dom/DOMNodeIds.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

CustomCompositorAnimationManager::CustomCompositorAnimationManager() {}

CustomCompositorAnimationManager::~CustomCompositorAnimationManager() {}

void CustomCompositorAnimationManager::ApplyMutations(
    CompositorMutations* mutations) {
  TRACE_EVENT0("cc", "CustomCompositorAnimationManager::applyMutations");
  for (const auto& entry : mutations->map) {
    int element_id = entry.key;
    const CompositorMutation& mutation = *entry.value;
    Node* node = DOMNodeIds::NodeForId(element_id);
    if (!node || !node->IsElementNode())
      continue;
    ToElement(node)->UpdateFromCompositorMutation(mutation);
  }
}

}  // namespace blink
