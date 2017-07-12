// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_base.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

void AXPlatformNodeBase::Init(AXPlatformNodeDelegate* delegate) {
  delegate_ = delegate;
}

const AXNodeData& AXPlatformNodeBase::GetData() const {
  CHECK(delegate_);
  return delegate_->GetData();
}

gfx::Rect AXPlatformNodeBase::GetBoundsInScreen() const {
  CHECK(delegate_);
  return delegate_->GetScreenBoundsRect();
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetParent() {
  CHECK(delegate_);
  return delegate_->GetParent();
}

int AXPlatformNodeBase::GetChildCount() {
  CHECK(delegate_);
  return delegate_->GetChildCount();
}

gfx::NativeViewAccessible AXPlatformNodeBase::ChildAtIndex(int index) {
  CHECK(delegate_);
  return delegate_->ChildAtIndex(index);
}

// AXPlatformNode overrides.

void AXPlatformNodeBase::Destroy() {
  AXPlatformNode::Destroy();
  delegate_ = nullptr;
  Dispose();
}

void AXPlatformNodeBase::Dispose() {
  delete this;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetNativeViewAccessible() {
  return nullptr;
}

AXPlatformNodeDelegate* AXPlatformNodeBase::GetDelegate() const {
  return delegate_;
}

// Helpers.

AXPlatformNodeBase* AXPlatformNodeBase::GetPreviousSibling() {
  CHECK(delegate_);
  gfx::NativeViewAccessible parent_accessible = GetParent();
  AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);
  if (!parent)
    return nullptr;

  int previous_index = GetIndexInParent() - 1;
  if (previous_index >= 0 &&
      previous_index < parent->GetChildCount()) {
    return FromNativeViewAccessible(parent->ChildAtIndex(previous_index));
  }
  return nullptr;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetNextSibling() {
  CHECK(delegate_);
  gfx::NativeViewAccessible parent_accessible = GetParent();
  AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);
  if (!parent)
    return nullptr;

  int next_index = GetIndexInParent() + 1;
  if (next_index >= 0 && next_index < parent->GetChildCount())
    return FromNativeViewAccessible(parent->ChildAtIndex(next_index));
  return nullptr;
}

bool AXPlatformNodeBase::IsDescendant(AXPlatformNodeBase* node) {
  CHECK(delegate_);
  if (!node)
    return false;
  if (node == this)
    return true;
  gfx::NativeViewAccessible native_parent = node->GetParent();
  if (!native_parent)
    return false;
  AXPlatformNodeBase* parent = FromNativeViewAccessible(native_parent);
  return IsDescendant(parent);
}

bool AXPlatformNodeBase::HasBoolAttribute(
    ui::AXBoolAttribute attribute) const {
  CHECK(delegate_);
  return GetData().HasBoolAttribute(attribute);
}

bool AXPlatformNodeBase::GetBoolAttribute(
    ui::AXBoolAttribute attribute) const {
  CHECK(delegate_);
  return GetData().GetBoolAttribute(attribute);
}

bool AXPlatformNodeBase::GetBoolAttribute(
    ui::AXBoolAttribute attribute, bool* value) const {
  CHECK(delegate_);
  return GetData().GetBoolAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasFloatAttribute(
    ui::AXFloatAttribute attribute) const {
  CHECK(delegate_);
  return GetData().HasFloatAttribute(attribute);
}

float AXPlatformNodeBase::GetFloatAttribute(
    ui::AXFloatAttribute attribute) const {
  CHECK(delegate_);
  return GetData().GetFloatAttribute(attribute);
}

bool AXPlatformNodeBase::GetFloatAttribute(
    ui::AXFloatAttribute attribute, float* value) const {
  CHECK(delegate_);
  return GetData().GetFloatAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasIntAttribute(
    ui::AXIntAttribute attribute) const {
  CHECK(delegate_);
  return GetData().HasIntAttribute(attribute);
}

int AXPlatformNodeBase::GetIntAttribute(
    ui::AXIntAttribute attribute) const {
  CHECK(delegate_);
  return GetData().GetIntAttribute(attribute);
}

bool AXPlatformNodeBase::GetIntAttribute(
    ui::AXIntAttribute attribute, int* value) const {
  CHECK(delegate_);
  return GetData().GetIntAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasStringAttribute(
    ui::AXStringAttribute attribute) const {
  CHECK(delegate_);
  return GetData().HasStringAttribute(attribute);
}

const std::string& AXPlatformNodeBase::GetStringAttribute(
    ui::AXStringAttribute attribute) const {
  CHECK(delegate_);
  return GetData().GetStringAttribute(attribute);
}

bool AXPlatformNodeBase::GetStringAttribute(
    ui::AXStringAttribute attribute, std::string* value) const {
  CHECK(delegate_);
  return GetData().GetStringAttribute(attribute, value);
}

base::string16 AXPlatformNodeBase::GetString16Attribute(
    ui::AXStringAttribute attribute) const {
  CHECK(delegate_);
  return GetData().GetString16Attribute(attribute);
}

bool AXPlatformNodeBase::GetString16Attribute(
    ui::AXStringAttribute attribute,
    base::string16* value) const {
  CHECK(delegate_);
  return GetData().GetString16Attribute(attribute, value);
}

bool AXPlatformNodeBase::HasIntListAttribute(
    ui::AXIntListAttribute attribute) const {
  return GetData().HasIntListAttribute(attribute);
}

const std::vector<int32_t>& AXPlatformNodeBase::GetIntListAttribute(
    ui::AXIntListAttribute attribute) const {
  return GetData().GetIntListAttribute(attribute);
}

bool AXPlatformNodeBase::GetIntListAttribute(
    ui::AXIntListAttribute attribute,
    std::vector<int32_t>* value) const {
  return GetData().GetIntListAttribute(attribute, value);
}

AXPlatformNodeBase::AXPlatformNodeBase() {
}

AXPlatformNodeBase::~AXPlatformNodeBase() {
  CHECK(!delegate_);
}

// static
AXPlatformNodeBase* AXPlatformNodeBase::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(accessible));
}

bool AXPlatformNodeBase::SetTextSelection(int start_offset, int end_offset) {
  ui::AXActionData action_data;
  action_data.action = ui::AX_ACTION_SET_SELECTION;
  action_data.anchor_node_id = action_data.focus_node_id = GetData().id;
  action_data.anchor_offset = start_offset;
  action_data.focus_offset = end_offset;
  DCHECK(delegate_);
  return delegate_->AccessibilityPerformAction(action_data);
}

bool AXPlatformNodeBase::IsTextOnlyObject() const {
  return GetData().role == ui::AX_ROLE_STATIC_TEXT ||
         GetData().role == ui::AX_ROLE_LINE_BREAK ||
         GetData().role == ui::AX_ROLE_INLINE_TEXT_BOX;
}

bool AXPlatformNodeBase::IsNativeTextControl() const {
  const std::string& html_tag = GetStringAttribute(ui::AX_ATTR_HTML_TAG);
  if (html_tag == "input") {
    std::string input_type;
    if (!GetData().GetHtmlAttribute("type", &input_type))
      return true;
    return input_type.empty() || input_type == "email" ||
           input_type == "password" || input_type == "search" ||
           input_type == "tel" || input_type == "text" || input_type == "url" ||
           input_type == "number";
  }
  return html_tag == "textarea";
}

bool AXPlatformNodeBase::IsSimpleTextControl() const {
  // Time fields, color wells and spinner buttons might also use text fields as
  // constituent parts, but they are not considered text fields as a whole.
  switch (GetData().role) {
    case ui::AX_ROLE_COMBO_BOX:
    case ui::AX_ROLE_SEARCH_BOX:
      return true;
    case ui::AX_ROLE_TEXT_FIELD:
      return !GetData().HasState(ui::AX_STATE_RICHLY_EDITABLE);
    default:
      return false;
  }
}

// Indicates if this object is at the root of a rich edit text control.
bool AXPlatformNodeBase::IsRichTextControl() {
  gfx::NativeViewAccessible parent_accessible = GetParent();
  AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);
  if (!parent)
    return false;

  return GetData().HasState(ui::AX_STATE_RICHLY_EDITABLE) &&
         (!parent || !parent->GetData().HasState(ui::AX_STATE_RICHLY_EDITABLE));
}

base::string16 AXPlatformNodeBase::GetInnerText() {
  if (IsTextOnlyObject())
    return GetString16Attribute(ui::AX_ATTR_NAME);

  base::string16 text;
  for (int i = 0; i < GetChildCount(); ++i) {
    gfx::NativeViewAccessible child_accessible = ChildAtIndex(i);
    AXPlatformNodeBase* child = FromNativeViewAccessible(child_accessible);
    if (!child)
      continue;

    text += child->GetInnerText();
  }
  return text;
}

bool AXPlatformNodeBase::IsRangeValueSupported() const {
  switch (GetData().role) {
    case ui::AX_ROLE_PROGRESS_INDICATOR:
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_SPIN_BUTTON:
    case ui::AX_ROLE_SCROLL_BAR:
      return true;
    case ui::AX_ROLE_SPLITTER:
      return GetData().HasState(ui::AX_STATE_FOCUSABLE);
    default:
      return false;
  }
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTable() const {
  AXPlatformNodeBase* table = const_cast<AXPlatformNodeBase*>(this);
  while (table && !ui::IsTableLikeRole(table->GetData().role)) {
    gfx::NativeViewAccessible parent_accessible = table->GetParent();
    AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);

    table = parent;
  }
  return table;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCell(int index) const {
  DCHECK(delegate_);

  if (!ui::IsTableLikeRole(GetData().role) &&
      !ui::IsCellOrTableHeaderRole(GetData().role))
    return nullptr;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return nullptr;
  const std::vector<int32_t>& unique_cell_ids =
      table->GetIntListAttribute(ui::AX_ATTR_UNIQUE_CELL_IDS);
  if (index < 0 || index >= static_cast<int>(unique_cell_ids.size()))
    return nullptr;

  return static_cast<AXPlatformNodeBase*>(
      table->delegate_->GetFromNodeID(unique_cell_ids[index]));
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCell(int row,
                                                     int column) const {
  if (!ui::IsTableLikeRole(GetData().role) &&
      !ui::IsCellOrTableHeaderRole(GetData().role))
    return nullptr;

  if (row < 0 || row >= GetTableRowCount() || column < 0 ||
      column >= GetTableColumnCount()) {
    return nullptr;
  }

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return nullptr;

  // In contrast to unique cell IDs, these are duplicated whenever a cell spans
  // multiple columns or rows.
  const std::vector<int32_t>& cell_ids =
      table->GetIntListAttribute(ui::AX_ATTR_CELL_IDS);
  DCHECK_EQ(GetTableRowCount() * GetTableColumnCount(),
            static_cast<int>(cell_ids.size()));
  int position = row * GetTableColumnCount() + column;
  if (position < 0 || position >= static_cast<int>(cell_ids.size()))
    return nullptr;

  return static_cast<AXPlatformNodeBase*>(
      table->delegate_->GetFromNodeID(cell_ids[position]));
}

int AXPlatformNodeBase::GetTableCellIndex() const {
  if (!ui::IsCellOrTableHeaderRole(GetData().role))
    return -1;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return -1;

  const std::vector<int32_t>& unique_cell_ids =
      table->GetIntListAttribute(ui::AX_ATTR_UNIQUE_CELL_IDS);
  auto iter =
      std::find(unique_cell_ids.begin(), unique_cell_ids.end(), GetData().id);
  if (iter == unique_cell_ids.end())
    return -1;

  return std::distance(unique_cell_ids.begin(), iter);
}

int AXPlatformNodeBase::GetTableColumn() const {
  return GetIntAttribute(ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX);
}

int AXPlatformNodeBase::GetTableColumnCount() const {
  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return 0;

  return table->GetIntAttribute(ui::AX_ATTR_TABLE_COLUMN_COUNT);
}

int AXPlatformNodeBase::GetTableColumnSpan() const {
  if (!ui::IsCellOrTableHeaderRole(GetData().role))
    return 0;

  int column_span;
  if (GetIntAttribute(ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN, &column_span))
    return column_span;
  return 1;
}

int AXPlatformNodeBase::GetTableRow() const {
  return GetIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_INDEX);
}

int AXPlatformNodeBase::GetTableRowCount() const {
  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return 0;

  return table->GetIntAttribute(ui::AX_ATTR_TABLE_ROW_COUNT);
}

int AXPlatformNodeBase::GetTableRowSpan() const {
  if (!ui::IsCellOrTableHeaderRole(GetData().role))
    return 0;

  int row_span;
  if (GetIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_SPAN, &row_span))
    return row_span;
  return 1;
}

}  // namespace ui
