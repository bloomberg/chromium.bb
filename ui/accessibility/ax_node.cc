// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"

#include <algorithm>

#include "base/strings/string16.h"
#include "ui/gfx/transform.h"

namespace ui {

AXNode::AXNode(AXNode* parent, int32_t id, int32_t index_in_parent)
    : index_in_parent_(index_in_parent), parent_(parent) {
  data_.id = id;
}

AXNode::~AXNode() {
}

void AXNode::SetData(const AXNodeData& src) {
  data_ = src;
}

void AXNode::SetLocation(int offset_container_id,
                         const gfx::RectF& location,
                         gfx::Transform* transform) {
  data_.offset_container_id = offset_container_id;
  data_.location = location;
  if (transform)
    data_.transform.reset(new gfx::Transform(*transform));
  else
    data_.transform.reset(nullptr);
}

void AXNode::SetIndexInParent(int index_in_parent) {
  index_in_parent_ = index_in_parent;
}

void AXNode::SwapChildren(std::vector<AXNode*>& children) {
  children.swap(children_);
}

void AXNode::Destroy() {
  delete this;
}

bool AXNode::IsDescendantOf(AXNode* ancestor) {
  if (this == ancestor)
    return true;
  else if (parent())
    return parent()->IsDescendantOf(ancestor);

  return false;
}

std::vector<int> AXNode::GetOrComputeLineStartOffsets() {
  std::vector<int> line_offsets;
  if (data().GetIntListAttribute(AX_ATTR_CACHED_LINE_STARTS, &line_offsets))
    return line_offsets;

  int end_offset = 0;
  ComputeLineStartOffsets(&line_offsets, &end_offset);
  data_.AddIntListAttribute(AX_ATTR_CACHED_LINE_STARTS, line_offsets);
  return line_offsets;
}

void AXNode::ComputeLineStartOffsets(std::vector<int>* line_offsets,
                                     int* end_offset) const {
  DCHECK(line_offsets);
  DCHECK(end_offset);
  for (const AXNode* child : children()) {
    DCHECK(child);
    if (child->child_count()) {
      child->ComputeLineStartOffsets(line_offsets, end_offset);
      continue;
    }

    base::string16 text = child->data().GetString16Attribute(ui::AX_ATTR_NAME);
    *end_offset += static_cast<int>(text.length());
    if (!child->data().HasIntAttribute(ui::AX_ATTR_NEXT_ON_LINE_ID))
      line_offsets->push_back(*end_offset);
  }
}

}  // namespace ui
