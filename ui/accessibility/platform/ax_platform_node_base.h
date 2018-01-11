// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_

#include "base/macros.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

struct AXNodeData;
class AXPlatformNodeDelegate;

class AX_EXPORT AXPlatformNodeBase : public AXPlatformNode {
 public:
  virtual void Init(AXPlatformNodeDelegate* delegate);

  // These are simple wrappers to our delegate.
  const AXNodeData& GetData() const;
  gfx::Rect GetBoundsInScreen() const;
  gfx::NativeViewAccessible GetParent();
  int GetChildCount();
  gfx::NativeViewAccessible ChildAtIndex(int index);

  // This needs to be implemented for each platform.
  virtual int GetIndexInParent() = 0;

  // AXPlatformNode.
  void Destroy() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  AXPlatformNodeDelegate* GetDelegate() const override;

  // Helpers.
  AXPlatformNodeBase* GetPreviousSibling();
  AXPlatformNodeBase* GetNextSibling();
  bool IsDescendant(AXPlatformNodeBase* descendant);

  bool HasBoolAttribute(AXBoolAttribute attr) const;
  bool GetBoolAttribute(AXBoolAttribute attr) const;
  bool GetBoolAttribute(AXBoolAttribute attr, bool* value) const;

  bool HasFloatAttribute(AXFloatAttribute attr) const;
  float GetFloatAttribute(AXFloatAttribute attr) const;
  bool GetFloatAttribute(AXFloatAttribute attr, float* value) const;

  bool HasIntAttribute(AXIntAttribute attribute) const;
  int GetIntAttribute(AXIntAttribute attribute) const;
  bool GetIntAttribute(AXIntAttribute attribute, int* value) const;

  bool HasStringAttribute(AXStringAttribute attribute) const;
  const std::string& GetStringAttribute(AXStringAttribute attribute) const;
  bool GetStringAttribute(AXStringAttribute attribute,
                          std::string* value) const;
  bool GetString16Attribute(AXStringAttribute attribute,
                            base::string16* value) const;
  base::string16 GetString16Attribute(AXStringAttribute attribute) const;

  bool HasIntListAttribute(AXIntListAttribute attribute) const;
  const std::vector<int32_t>& GetIntListAttribute(
      AXIntListAttribute attribute) const;

  bool GetIntListAttribute(AXIntListAttribute attribute,
                           std::vector<int32_t>* value) const;

  // Returns the table or ARIA grid if inside one.
  AXPlatformNodeBase* GetTable() const;

  // If inside a table or ARIA grid, returns the cell found at the given index.
  // Indices are in row major order and each cell is counted once regardless of
  // its span.
  AXPlatformNodeBase* GetTableCell(int index) const;

  // If inside a table or ARIA grid, returns the cell at the given row and
  // column (0-based). Works correctly with cells that span multiple rows or
  // columns.
  AXPlatformNodeBase* GetTableCell(int row, int column) const;

  // If inside a table or ARIA grid, returns the zero-based index of the cell.
  // Indices are in row major order and each cell is counted once regardless of
  // its span. Returns -1 if the cell is not found or if not inside a table.
  int GetTableCellIndex() const;

  // If inside a table or ARIA grid, returns the physical column number for the
  // current cell. In contrast to logical columns, physical columns always start
  // from 0 and have no gaps in their numbering. Logical columns can be set
  // using aria-colindex.
  int GetTableColumn() const;

  // If inside a table or ARIA grid, returns the number of physical columns,
  // otherwise returns 0.
  int GetTableColumnCount() const;

  // If inside a table or ARIA grid, returns the number of physical columns that
  // this cell spans. If not a cell, returns 0.
  int GetTableColumnSpan() const;

  // If inside a table or ARIA grid, returns the physical row number for the
  // current cell. In contrast to logical rows, physical rows always start from
  // 0 and have no gaps in their numbering. Logical rows can be set using
  // aria-rowindex.
  int GetTableRow() const;

  // If inside a table or ARIA grid, returns the number of physical rows,
  // otherwise returns 0.
  int GetTableRowCount() const;

  // If inside a table or ARIA grid, returns the number of physical rows that
  // this cell spans. If not a cell, returns 0.
  int GetTableRowSpan() const;

  // Returns true if either a descendant has selection (sel_focus_object_id) or
  // if this node is a simple text element and has text selection attributes.
  bool HasCaret();

  // Return true if this object is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(AXPlatformNodeBase* ancestor);

  // Returns true if an ancestor of this node (not including itself) is a
  // leaf node, meaning that this node is not actually exposed to the
  // platform.
  bool IsChildOfLeaf();

  // Returns true if this is a leaf node on this platform, meaning any
  // children should not be exposed to this platform's native accessibility
  // layer. Each platform subclass should implement this itself.
  // The definition of a leaf may vary depending on the platform,
  // but a leaf node should never have children that are focusable or
  // that might send notifications.
  bool IsLeaf();

  virtual base::string16 GetText();

  virtual base::string16 GetValue();

  //
  // Delegate.  This is a weak reference which owns |this|.
  //
  AXPlatformNodeDelegate* delegate_;

 protected:
  AXPlatformNodeBase();
  ~AXPlatformNodeBase() override;

  bool IsTextOnlyObject() const;
  bool IsPlainTextField() const;
  bool IsRichTextField() const;
  bool IsRangeValueSupported() const;

  // Get the range value text, which might come from aria-valuetext or
  // a floating-point value. This is different from the value string
  // attribute used in input controls such as text boxes and combo boxes.
  base::string16 GetRangeValueText();

  // |GetInnerText| recursively includes all the text from descendants such as
  // text found in any embedded object.
  base::string16 GetInnerText();

  // Cast a gfx::NativeViewAccessible to an AXPlatformNodeBase if it is one,
  // or return NULL if it's not an instance of this class.
  static AXPlatformNodeBase* FromNativeViewAccessible(
      gfx::NativeViewAccessible accessible);

  virtual void Dispose();

  // Sets the text selection in this object if possible.
  bool SetTextSelection(int start_offset, int end_offset);

 private:
  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeBase);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
