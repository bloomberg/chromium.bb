// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_base.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

void AXPlatformNodeBase::Init(AXPlatformNodeDelegate* delegate) {
  delegate_ = delegate;
}

const AXNodeData& AXPlatformNodeBase::GetData() const {
  CR_DEFINE_STATIC_LOCAL(AXNodeData, empty_data, ());
  if (delegate_)
    return delegate_->GetData();
  return empty_data;
}

gfx::Rect AXPlatformNodeBase::GetBoundsInScreen() const {
  if (delegate_)
    return delegate_->GetScreenBoundsRect();
  return gfx::Rect();
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetParent() {
  if (delegate_)
    return delegate_->GetParent();
  return nullptr;
}

int AXPlatformNodeBase::GetChildCount() {
  if (delegate_)
    return delegate_->GetChildCount();
  return 0;
}

gfx::NativeViewAccessible AXPlatformNodeBase::ChildAtIndex(int index) {
  if (delegate_)
    return delegate_->ChildAtIndex(index);
  return nullptr;
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
  if (!delegate_)
    return nullptr;
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
  if (!delegate_)
    return nullptr;
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
  if (!delegate_)
    return false;
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

bool AXPlatformNodeBase::HasBoolAttribute(AXBoolAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().HasBoolAttribute(attribute);
}

bool AXPlatformNodeBase::GetBoolAttribute(AXBoolAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().GetBoolAttribute(attribute);
}

bool AXPlatformNodeBase::GetBoolAttribute(AXBoolAttribute attribute,
                                          bool* value) const {
  if (!delegate_)
    return false;
  return GetData().GetBoolAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasFloatAttribute(AXFloatAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().HasFloatAttribute(attribute);
}

float AXPlatformNodeBase::GetFloatAttribute(AXFloatAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().GetFloatAttribute(attribute);
}

bool AXPlatformNodeBase::GetFloatAttribute(AXFloatAttribute attribute,
                                           float* value) const {
  if (!delegate_)
    return false;
  return GetData().GetFloatAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasIntAttribute(AXIntAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().HasIntAttribute(attribute);
}

int AXPlatformNodeBase::GetIntAttribute(AXIntAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().GetIntAttribute(attribute);
}

bool AXPlatformNodeBase::GetIntAttribute(AXIntAttribute attribute,
                                         int* value) const {
  if (!delegate_)
    return false;
  return GetData().GetIntAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasStringAttribute(AXStringAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().HasStringAttribute(attribute);
}

const std::string& AXPlatformNodeBase::GetStringAttribute(
    AXStringAttribute attribute) const {
  CR_DEFINE_STATIC_LOCAL(std::string, empty_data, ());
  if (!delegate_)
    return empty_data;
  return GetData().GetStringAttribute(attribute);
}

bool AXPlatformNodeBase::GetStringAttribute(AXStringAttribute attribute,
                                            std::string* value) const {
  if (!delegate_)
    return false;
  return GetData().GetStringAttribute(attribute, value);
}

base::string16 AXPlatformNodeBase::GetString16Attribute(
    AXStringAttribute attribute) const {
  if (!delegate_)
    return base::string16();
  return GetData().GetString16Attribute(attribute);
}

bool AXPlatformNodeBase::GetString16Attribute(AXStringAttribute attribute,
                                              base::string16* value) const {
  if (!delegate_)
    return false;
  return GetData().GetString16Attribute(attribute, value);
}

bool AXPlatformNodeBase::HasIntListAttribute(
    AXIntListAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().HasIntListAttribute(attribute);
}

const std::vector<int32_t>& AXPlatformNodeBase::GetIntListAttribute(
    AXIntListAttribute attribute) const {
  CR_DEFINE_STATIC_LOCAL(std::vector<int32_t>, empty_data, ());
  if (!delegate_)
    return empty_data;
  return GetData().GetIntListAttribute(attribute);
}

bool AXPlatformNodeBase::GetIntListAttribute(
    AXIntListAttribute attribute,
    std::vector<int32_t>* value) const {
  if (!delegate_)
    return false;
  return GetData().GetIntListAttribute(attribute, value);
}

AXPlatformNodeBase::AXPlatformNodeBase() {
}

AXPlatformNodeBase::~AXPlatformNodeBase() {
}

// static
AXPlatformNodeBase* AXPlatformNodeBase::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(accessible));
}

bool AXPlatformNodeBase::SetTextSelection(int start_offset, int end_offset) {
  AXActionData action_data;
  action_data.action = AX_ACTION_SET_SELECTION;
  action_data.anchor_node_id = action_data.focus_node_id = GetData().id;
  action_data.anchor_offset = start_offset;
  action_data.focus_offset = end_offset;
  if (!delegate_)
    return false;

  return delegate_->AccessibilityPerformAction(action_data);
}

bool AXPlatformNodeBase::IsTextOnlyObject() const {
  return GetData().role == AX_ROLE_STATIC_TEXT ||
         GetData().role == AX_ROLE_LINE_BREAK ||
         GetData().role == AX_ROLE_INLINE_TEXT_BOX;
}

bool AXPlatformNodeBase::IsNativeTextControl() const {
  const std::string& html_tag = GetStringAttribute(AX_ATTR_HTML_TAG);
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
    case AX_ROLE_COMBO_BOX:
    case AX_ROLE_SEARCH_BOX:
      return true;
    case AX_ROLE_TEXT_FIELD:
      return !GetData().HasState(AX_STATE_RICHLY_EDITABLE);
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

  return GetData().HasState(AX_STATE_RICHLY_EDITABLE) &&
         (!parent || !parent->GetData().HasState(AX_STATE_RICHLY_EDITABLE));
}

base::string16 AXPlatformNodeBase::GetInnerText() {
  if (IsTextOnlyObject())
    return GetString16Attribute(AX_ATTR_NAME);

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
    case AX_ROLE_METER:
    case AX_ROLE_PROGRESS_INDICATOR:
    case AX_ROLE_SLIDER:
    case AX_ROLE_SPIN_BUTTON:
    case AX_ROLE_SCROLL_BAR:
      return true;
    case AX_ROLE_SPLITTER:
      return GetData().HasState(AX_STATE_FOCUSABLE);
    default:
      return false;
  }
}

base::string16 AXPlatformNodeBase::GetRangeValueText() {
  float fval;
  base::string16 value = GetString16Attribute(AX_ATTR_VALUE);

  if (value.empty() && GetFloatAttribute(AX_ATTR_VALUE_FOR_RANGE, &fval)) {
    value = base::UTF8ToUTF16(base::DoubleToString(fval));
  }
  return value;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTable() const {
  if (!delegate_)
    return nullptr;
  AXPlatformNodeBase* table = const_cast<AXPlatformNodeBase*>(this);
  while (table && !IsTableLikeRole(table->GetData().role)) {
    gfx::NativeViewAccessible parent_accessible = table->GetParent();
    AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);

    table = parent;
  }
  return table;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCell(int index) const {
  if (!delegate_)
    return nullptr;
  if (!IsTableLikeRole(GetData().role) &&
      !IsCellOrTableHeaderRole(GetData().role))
    return nullptr;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return nullptr;
  const std::vector<int32_t>& unique_cell_ids =
      table->GetIntListAttribute(AX_ATTR_UNIQUE_CELL_IDS);
  if (index < 0 || index >= static_cast<int>(unique_cell_ids.size()))
    return nullptr;

  return static_cast<AXPlatformNodeBase*>(
      table->delegate_->GetFromNodeID(unique_cell_ids[index]));
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCell(int row,
                                                     int column) const {
  if (!IsTableLikeRole(GetData().role) &&
      !IsCellOrTableHeaderRole(GetData().role))
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
      table->GetIntListAttribute(AX_ATTR_CELL_IDS);
  DCHECK_EQ(GetTableRowCount() * GetTableColumnCount(),
            static_cast<int>(cell_ids.size()));
  int position = row * GetTableColumnCount() + column;
  if (position < 0 || position >= static_cast<int>(cell_ids.size()))
    return nullptr;

  return static_cast<AXPlatformNodeBase*>(
      table->delegate_->GetFromNodeID(cell_ids[position]));
}

int AXPlatformNodeBase::GetTableCellIndex() const {
  if (!IsCellOrTableHeaderRole(GetData().role))
    return -1;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return -1;

  const std::vector<int32_t>& unique_cell_ids =
      table->GetIntListAttribute(AX_ATTR_UNIQUE_CELL_IDS);
  auto iter =
      std::find(unique_cell_ids.begin(), unique_cell_ids.end(), GetData().id);
  if (iter == unique_cell_ids.end())
    return -1;

  return std::distance(unique_cell_ids.begin(), iter);
}

int AXPlatformNodeBase::GetTableColumn() const {
  return GetIntAttribute(AX_ATTR_TABLE_CELL_COLUMN_INDEX);
}

int AXPlatformNodeBase::GetTableColumnCount() const {
  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return 0;

  return table->GetIntAttribute(AX_ATTR_TABLE_COLUMN_COUNT);
}

int AXPlatformNodeBase::GetTableColumnSpan() const {
  if (!IsCellOrTableHeaderRole(GetData().role))
    return 0;

  int column_span;
  if (GetIntAttribute(AX_ATTR_TABLE_CELL_COLUMN_SPAN, &column_span))
    return column_span;
  return 1;
}

int AXPlatformNodeBase::GetTableRow() const {
  return GetIntAttribute(AX_ATTR_TABLE_CELL_ROW_INDEX);
}

int AXPlatformNodeBase::GetTableRowCount() const {
  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return 0;

  return table->GetIntAttribute(AX_ATTR_TABLE_ROW_COUNT);
}

int AXPlatformNodeBase::GetTableRowSpan() const {
  if (!IsCellOrTableHeaderRole(GetData().role))
    return 0;

  int row_span;
  if (GetIntAttribute(AX_ATTR_TABLE_CELL_ROW_SPAN, &row_span))
    return row_span;
  return 1;
}

bool AXPlatformNodeBase::HasCaret() {
  if (IsSimpleTextControl() && HasIntAttribute(ui::AX_ATTR_TEXT_SEL_START) &&
      HasIntAttribute(ui::AX_ATTR_TEXT_SEL_END)) {
    return true;
  }

  // The caret is always at the focus of the selection.
  int32_t focus_id = delegate_->GetTreeData().sel_focus_object_id;
  AXPlatformNodeBase* focus_object =
      static_cast<AXPlatformNodeBase*>(delegate_->GetFromNodeID(focus_id));

  if (!focus_object)
    return false;

  return focus_object->IsDescendantOf(this);
}

bool AXPlatformNodeBase::IsDescendantOf(AXPlatformNodeBase* ancestor) {
  if (!ancestor)
    return false;

  if (this == ancestor)
    return true;

  AXPlatformNodeBase* parent = FromNativeViewAccessible(GetParent());
  if (!parent)
    return false;

  return parent->IsDescendantOf(ancestor);
}

bool AXPlatformNodeBase::IsLeaf() {
  if (GetChildCount() == 0)
    return true;

  // These types of objects may have children that we use as internal
  // implementation details, but we want to expose them as leaves to platform
  // accessibility APIs because screen readers might be confused if they find
  // any children.
  // Note that if a combo box, search box or text field are not native, they
  // might present a menu of choices using aria-owns which should not be hidden
  // from tree.
  if (IsNativeTextControl() || IsTextOnlyObject())
    return true;

  // Roles whose children are only presentational according to the ARIA and
  // HTML5 Specs should be hidden from screen readers.
  // (Note that whilst ARIA buttons can have only presentational children, HTML5
  // buttons are allowed to have content.)
  switch (GetData().role) {
    case ui::AX_ROLE_IMAGE:
    case ui::AX_ROLE_METER:
    case ui::AX_ROLE_SCROLL_BAR:
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_SPLITTER:
    case ui::AX_ROLE_PROGRESS_INDICATOR:
      return true;
    default:
      return false;
  }
}

bool AXPlatformNodeBase::IsChildOfLeaf() {
  AXPlatformNodeBase* ancestor = FromNativeViewAccessible(GetParent());

  while (ancestor) {
    if (ancestor->IsLeaf())
      return true;
    ancestor = FromNativeViewAccessible(ancestor->GetParent());
  }

  return false;
}

base::string16 AXPlatformNodeBase::GetText() {
  return GetInnerText();
}

base::string16 AXPlatformNodeBase::GetValue() {
  // Expose slider value.
  if (IsRangeValueSupported()) {
    return GetRangeValueText();
  } else if (ui::IsDocument(GetData().role)) {
    // On Windows, the value of a document should be its URL.
    return base::UTF8ToUTF16(delegate_->GetTreeData().url);
  }
  base::string16 value = GetString16Attribute(ui::AX_ATTR_VALUE);

  // Some screen readers like Jaws and VoiceOver require a
  // value to be set in text fields with rich content, even though the same
  // information is available on the children.
  if (value.empty() && (IsSimpleTextControl() || IsRichTextControl()) &&
      !IsNativeTextControl())
    return GetInnerText();

  return value;
}

}  // namespace ui
