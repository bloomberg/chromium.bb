// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/legacy_layout_tree_walking.h"

#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// We still have the legacy layout tree structure, which means that a multicol
// container LayoutBlockFlow will consist of a LayoutFlowThread child, followed
// by zero or more siblings of type LayoutMultiColumnSet and/or
// LayoutMultiColumnSpannerPlaceholder. NG needs to skip these special
// objects. The actual content is inside the flow thread.

LayoutObject* GetLayoutObjectForFirstChildNode(LayoutNGBlockFlow* parent) {
  LayoutObject* child = parent->FirstChild();
  if (!child)
    return nullptr;
  if (child->IsLayoutFlowThread())
    return ToLayoutBlockFlow(child)->FirstChild();
  return child;
}

LayoutObject* GetLayoutObjectForParentNode(LayoutObject* object) {
  // First check that we're not walking where we shouldn't be walking.
  DCHECK(!object->IsLayoutFlowThread());
  DCHECK(!object->IsLayoutMultiColumnSet());
  DCHECK(!object->IsLayoutMultiColumnSpannerPlaceholder());

  LayoutObject* parent = object->Parent();
  if (!parent)
    return nullptr;
  if (parent->IsLayoutFlowThread())
    return parent->Parent();
  return parent;
}

bool AreNGBlockFlowChildrenInline(const LayoutNGBlockFlow* block) {
  if (block->ChildrenInline())
    return true;
  if (const auto* first_child = block->FirstChild()) {
    if (first_child->IsLayoutFlowThread())
      return first_child->ChildrenInline();
  }
  return false;
}

}  // namespace blink
