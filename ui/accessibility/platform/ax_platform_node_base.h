// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(USE_ATK)
#include <atk/atk.h>
#endif

namespace ui {

struct AXNodeData;
class AXPlatformNodeDelegate;

struct AX_EXPORT AXHypertext {
  AXHypertext();
  AXHypertext(const AXHypertext& other);
  ~AXHypertext();

  // Maps an embedded character offset in |hypertext| to an index in
  // |hyperlinks|.
  std::map<int32_t, int32_t> hyperlink_offset_to_index;

  // The unique id of a AXPlatformNodes for each hyperlink.
  // TODO(nektar): Replace object IDs with child indices if we decide that
  // we are not implementing IA2 hyperlinks for anything other than IA2
  // Hypertext.
  std::vector<int32_t> hyperlinks;

  base::string16 hypertext;
};

class AX_EXPORT AXPlatformNodeBase : public AXPlatformNode {
 public:
  virtual void Init(AXPlatformNodeDelegate* delegate);

  // These are simple wrappers to our delegate.
  const AXNodeData& GetData() const;
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

  bool HasBoolAttribute(ax::mojom::BoolAttribute attr) const;
  bool GetBoolAttribute(ax::mojom::BoolAttribute attr) const;
  bool GetBoolAttribute(ax::mojom::BoolAttribute attr, bool* value) const;

  bool HasFloatAttribute(ax::mojom::FloatAttribute attr) const;
  float GetFloatAttribute(ax::mojom::FloatAttribute attr) const;
  bool GetFloatAttribute(ax::mojom::FloatAttribute attr, float* value) const;

  bool HasIntAttribute(ax::mojom::IntAttribute attribute) const;
  int GetIntAttribute(ax::mojom::IntAttribute attribute) const;
  bool GetIntAttribute(ax::mojom::IntAttribute attribute, int* value) const;

  bool HasStringAttribute(ax::mojom::StringAttribute attribute) const;
  const std::string& GetStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  bool GetStringAttribute(ax::mojom::StringAttribute attribute,
                          std::string* value) const;
  bool GetString16Attribute(ax::mojom::StringAttribute attribute,
                            base::string16* value) const;
  base::string16 GetString16Attribute(
      ax::mojom::StringAttribute attribute) const;

  bool HasIntListAttribute(ax::mojom::IntListAttribute attribute) const;
  const std::vector<int32_t>& GetIntListAttribute(
      ax::mojom::IntListAttribute attribute) const;

  bool GetIntListAttribute(ax::mojom::IntListAttribute attribute,
                           std::vector<int32_t>* value) const;

  // Returns the selection container if inside one.
  AXPlatformNodeBase* GetSelectionContainer() const;

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

  // Returns true if this node can be scrolled either in the horizontal or the
  // vertical direction.
  bool IsScrollable() const;

  // Returns true if this node can be scrolled in the horizontal direction.
  bool IsHorizontallyScrollable() const;

  // Returns true if this node can be scrolled in the vertical direction.
  bool IsVerticallyScrollable() const;

  bool HasFocus();

  virtual std::string GetText();

  virtual base::string16 GetValue();

  // Represents a non-static text node in IAccessibleHypertext (and ATK in the
  // future). This character is embedded in the response to
  // IAccessibleText::get_text, indicating the position where a non-static text
  // child object appears.
  static const base::char16 kEmbeddedCharacter;

  //
  // Delegate.  This is a weak reference which owns |this|.
  //
  AXPlatformNodeDelegate* delegate_;

 protected:
  AXPlatformNodeBase();
  ~AXPlatformNodeBase() override;

  bool IsTextOnlyObject() const;
  bool IsPlainTextField() const;
  // Is in a focused textfield with a related suggestion popup available,
  // such as for the Autofill feature. The suggestion popup can be either hidden
  // and available or already visible. This indicates next down arrow key will
  // navigate within the suggestion popup.
  bool IsFocusedInputWithSuggestions();
  bool IsRichTextField() const;
  bool IsRangeValueSupported() const;

  // Get the range value text, which might come from aria-valuetext or
  // a floating-point value. This is different from the value string
  // attribute used in input controls such as text boxes and combo boxes.
  base::string16 GetRangeValueText();

  // |GetInnerText| recursively includes all the text from descendants such as
  // text found in any embedded object.
  std::string GetInnerText();

  // Cast a gfx::NativeViewAccessible to an AXPlatformNodeBase if it is one,
  // or return NULL if it's not an instance of this class.
  static AXPlatformNodeBase* FromNativeViewAccessible(
      gfx::NativeViewAccessible accessible);

  virtual void Dispose();

  // Sets the text selection in this object if possible.
  bool SetTextSelection(int start_offset, int end_offset);

#if BUILDFLAG(USE_ATK)
  using PlatformAttributeList = AtkAttributeSet*;
#else
  using PlatformAttributeList = std::vector<base::string16>;
#endif

  // Compute the attributes exposed via platform accessibility objects and put
  // them into an attribute list, |attributes|. Currently only used by
  // IAccessible2 on Windows and ATK on Aura Linux.
  void ComputeAttributes(PlatformAttributeList* attributes);

  // If the string attribute |attribute| is present, add its value as an
  // IAccessible2 attribute with the name |name|.
  void AddAttributeToList(const ax::mojom::StringAttribute attribute,
                          const char* name,
                          PlatformAttributeList* attributes);

  // If the bool attribute |attribute| is present, add its value as an
  // IAccessible2 attribute with the name |name|.
  void AddAttributeToList(const ax::mojom::BoolAttribute attribute,
                          const char* name,
                          PlatformAttributeList* attributes);

  // If the int attribute |attribute| is present, add its value as an
  // IAccessible2 attribute with the name |name|.
  void AddAttributeToList(const ax::mojom::IntAttribute attribute,
                          const char* name,
                          PlatformAttributeList* attributes);

  // A helper to add the given string value to |attributes|.
  virtual void AddAttributeToList(const char* name,
                                  const std::string& value,
                                  PlatformAttributeList* attributes);

  // A pure virtual method that subclasses use to actually add the attribute to
  // |attributes|.
  virtual void AddAttributeToList(const char* name,
                                  const char* value,
                                  PlatformAttributeList* attributes) = 0;

  // Escapes characters in string attributes as required by the IA2 Spec
  // and AT-SPI2. It's okay for input to be the same as output.
  static void SanitizeStringAttribute(const std::string& input,
                                      std::string* output);

  // Compute the hypertext for this node to be exposed via IA2 and ATK This
  // method is responsible for properly embedding children using the special
  // embedded element character.
  AXHypertext ComputeHypertext();

 private:
  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeBase);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
