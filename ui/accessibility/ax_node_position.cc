// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_position.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.h"

namespace ui {

AXTree* AXNodePosition::tree_ = nullptr;

AXNodePosition::AXNodePosition() {}

AXNodePosition::~AXNodePosition() {}

AXNodePosition::AXPositionInstance AXNodePosition::Clone() const {
  return AXPositionInstance(new AXNodePosition(*this));
}

base::string16 AXNodePosition::GetInnerText() const {
  if (IsNullPosition())
    return base::string16();

  DCHECK(GetAnchor());
  base::string16 value =
      GetAnchor()->data().GetString16Attribute(AX_ATTR_VALUE);
  if (!value.empty())
    return value;
  return GetAnchor()->data().GetString16Attribute(AX_ATTR_NAME);
}

void AXNodePosition::AnchorChild(int child_index,
                                 int* tree_id,
                                 int32_t* child_id) const {
  DCHECK(tree_id);
  DCHECK(child_id);

  if (!GetAnchor() || child_index < 0 || child_index >= AnchorChildCount()) {
    *tree_id = INVALID_TREE_ID;
    *child_id = INVALID_ANCHOR_ID;
    return;
  }

  AXNode* child = GetAnchor()->ChildAtIndex(child_index);
  DCHECK(child);
  *tree_id = this->tree_id();
  *child_id = child->id();
}

int AXNodePosition::AnchorChildCount() const {
  return GetAnchor() ? GetAnchor()->child_count() : 0;
}

int AXNodePosition::AnchorIndexInParent() const {
  return GetAnchor() ? GetAnchor()->index_in_parent() : INVALID_INDEX;
}

void AXNodePosition::AnchorParent(int* tree_id, int32_t* parent_id) const {
  DCHECK(tree_id);
  DCHECK(parent_id);

  if (!GetAnchor() || !GetAnchor()->parent()) {
    *tree_id = INVALID_TREE_ID;
    *parent_id = INVALID_ANCHOR_ID;
    return;
  }

  AXNode* parent = GetAnchor()->parent();
  *tree_id = this->tree_id();
  *parent_id = parent->id();
}

AXNode* AXNodePosition::GetNodeInTree(int tree_id, int32_t node_id) const {
  if (!tree_ || node_id == INVALID_ANCHOR_ID)
    return nullptr;
  return AXNodePosition::tree_->GetFromId(node_id);
}

int AXNodePosition::MaxTextOffset() const {
  if (IsNullPosition())
    return INVALID_INDEX;
  return static_cast<int>(GetInnerText().length());
}

// TODO(nektar): There might be other newline characters than '\n'.
bool AXNodePosition::IsInLineBreak() const {
  switch (kind()) {
    case AXPositionKind::NULL_POSITION:
      return false;
    case AXPositionKind::TREE_POSITION:
    case AXPositionKind::TEXT_POSITION:
      return GetInnerText() == base::UTF8ToUTF16("\n");
  }
  NOTREACHED();
  return false;
}

std::vector<int32_t> AXNodePosition::GetWordStartOffsets() const {
  if (IsNullPosition())
    return std::vector<int32_t>();
  DCHECK(GetAnchor());
  return GetAnchor()->data().GetIntListAttribute(ui::AX_ATTR_WORD_STARTS);
}

std::vector<int32_t> AXNodePosition::GetWordEndOffsets() const {
  if (IsNullPosition())
    return std::vector<int32_t>();
  DCHECK(GetAnchor());
  return GetAnchor()->data().GetIntListAttribute(ui::AX_ATTR_WORD_ENDS);
}

int32_t AXNodePosition::GetNextOnLineID(int32_t node_id) const {
  if (IsNullPosition())
    return INVALID_ANCHOR_ID;
  AXNode* node = GetNodeInTree(tree_id(), node_id);
  int next_on_line_id;
  if (!node ||
      !node->data().GetIntAttribute(AX_ATTR_NEXT_ON_LINE_ID,
                                    &next_on_line_id)) {
    return INVALID_ANCHOR_ID;
  }
  return static_cast<int32_t>(next_on_line_id);
}

int32_t AXNodePosition::GetPreviousOnLineID(int32_t node_id) const {
  if (IsNullPosition())
    return INVALID_ANCHOR_ID;
  AXNode* node = GetNodeInTree(tree_id(), node_id);
  int previous_on_line_id;
  if (!node ||
      !node->data().GetIntAttribute(AX_ATTR_PREVIOUS_ON_LINE_ID,
                                    &previous_on_line_id)) {
    return INVALID_ANCHOR_ID;
  }
  return static_cast<int32_t>(previous_on_line_id);
}

}  // namespace ui
