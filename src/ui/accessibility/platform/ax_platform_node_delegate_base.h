// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_BASE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/optional.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

// Base implementation of AXPlatformNodeDelegate where all functions
// return a default value. Useful for classes that want to implement
// AXPlatformNodeDelegate but don't need to override much of its
// behavior.
class AX_EXPORT AXPlatformNodeDelegateBase : public AXPlatformNodeDelegate {
 public:
  AXPlatformNodeDelegateBase();
  ~AXPlatformNodeDelegateBase() override;

  // Get the accessibility data that should be exposed for this node.
  // Virtually all of the information is obtained from this structure
  // (role, state, name, cursor position, etc.) - the rest of this interface
  // is mostly to implement support for walking the accessibility tree.
  const AXNodeData& GetData() const override;

  // Get the accessibility tree data for this node.
  const AXTreeData& GetTreeData() const override;

  // Get the unignored selection from the tree
  const AXTree::Selection GetUnignoredSelection() const override;

  // Creates a text position rooted at this object.
  AXNodePosition::AXPositionInstance CreateTextPositionAt(
      int offset) const override;

  // See comments in AXPlatformNodeDelegate.
  gfx::NativeViewAccessible GetNSWindow() override;

  // Get the node for this delegate, which may be an AXPlatformNode or it may
  // be a native accessible object implemented by another class.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  // Get the parent of the node, which may be an AXPlatformNode or it may
  // be a native accessible object implemented by another class.
  gfx::NativeViewAccessible GetParent() override;

  // Get the index in parent. Typically this is the AXNode's index_in_parent_.
  int GetIndexInParent() override;

  // Get the number of children of this node.
  int GetChildCount() const override;

  // Get the child of a node given a 0-based index.
  gfx::NativeViewAccessible ChildAtIndex(int index) override;

  gfx::NativeViewAccessible GetFirstChild() override;
  gfx::NativeViewAccessible GetLastChild() override;
  gfx::NativeViewAccessible GetNextSibling() override;
  gfx::NativeViewAccessible GetPreviousSibling() override;

  // Returns true if an ancestor of this node (not including itself) is a
  // leaf node, meaning that this node is not actually exposed to the
  // platform.
  bool IsChildOfLeaf() const override;

  // If this object is exposed to the platform, returns this object. Otherwise,
  // returns the platform leaf under which this object is found.
  gfx::NativeViewAccessible GetClosestPlatformObject() const override;

  class ChildIteratorBase : public ChildIterator {
   public:
    ChildIteratorBase(AXPlatformNodeDelegateBase* parent, int index);
    ChildIteratorBase(const ChildIteratorBase& it);
    ~ChildIteratorBase() override = default;
    bool operator==(const ChildIterator& rhs) const override;
    bool operator!=(const ChildIterator& rhs) const override;
    void operator++() override;
    void operator++(int) override;
    void operator--() override;
    void operator--(int) override;
    gfx::NativeViewAccessible GetNativeViewAccessible() const override;
    int GetIndexInParent() const override;
    AXPlatformNodeDelegate& operator*() const override;
    AXPlatformNodeDelegate* operator->() const override;

   private:
    int index_;
    AXPlatformNodeDelegateBase* parent_;
  };
  std::unique_ptr<AXPlatformNodeDelegate::ChildIterator> ChildrenBegin()
      override;
  std::unique_ptr<AXPlatformNodeDelegate::ChildIterator> ChildrenEnd() override;

  std::string GetName() const override;
  base::string16 GetHypertext() const override;
  bool SetHypertextSelection(int start_offset, int end_offset) override;
  TextAttributeMap ComputeTextAttributeMap(
      const TextAttributeList& default_attributes) const override;
  std::string GetInheritedFontFamilyName() const override;

  base::string16 GetInnerText() const override;

  gfx::Rect GetBoundsRect(const AXCoordinateSystem coordinate_system,
                          const AXClippingBehavior clipping_behavior,
                          AXOffscreenResult* offscreen_result) const override;

  gfx::Rect GetHypertextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result) const override;

  gfx::Rect GetInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result) const override;

  // Derivative utils for AXPlatformNodeDelegate::GetBoundsRect
  gfx::Rect GetClippedScreenBoundsRect(
      AXOffscreenResult* offscreen_result = nullptr) const override;
  gfx::Rect GetUnclippedScreenBoundsRect(
      AXOffscreenResult* offscreen_result = nullptr) const;

  // Do a *synchronous* hit test of the given location in global screen physical
  // pixel coordinates, and the node within this node's subtree (inclusive)
  // that's hit, if any.
  //
  // If the result is anything other than this object or NULL, it will be
  // hit tested again recursively - that allows hit testing to work across
  // implementation classes. It's okay to take advantage of this and return
  // only an immediate child and not the deepest descendant.
  //
  // This function is mainly used by accessibility debugging software.
  // Platforms with touch accessibility use a different asynchronous interface.
  gfx::NativeViewAccessible HitTestSync(
      int screen_physical_pixel_x,
      int screen_physical_pixel_y) const override;

  // Return the node within this node's subtree (inclusive) that currently
  // has focus.
  gfx::NativeViewAccessible GetFocus() override;

  // Get whether this node is offscreen.
  bool IsOffscreen() const override;

  // Get whether this node is a minimized window.
  bool IsMinimized() const override;

  // Get whether this node is in web content.
  bool IsWebContent() const override;

  // Returns true if the caret or selection is visible on this object.
  bool HasVisibleCaretOrSelection() const override;

  // Get another node from this same tree.
  AXPlatformNode* GetFromNodeID(int32_t id) override;

  // Get a node from a different tree using a tree ID and node ID.
  // Note that this is only guaranteed to work if the other tree is of the
  // same type, i.e. it won't work between web and views or vice-versa.
  AXPlatformNode* GetFromTreeIDAndNodeID(const ui::AXTreeID& ax_tree_id,
                                         int32_t id) override;

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true), return
  // a target nodes for which this delegate's node has that relationship
  // attribute or NULL if there is no such relationship.
  AXPlatformNode* GetTargetNodeForRelation(
      ax::mojom::IntAttribute attr) override;

  // Given a node ID attribute (one where IsNodeIdIntListAttribute is true),
  // return a vector of all target nodes for which this delegate's node has that
  // relationship attribute.
  std::vector<AXPlatformNode*> GetTargetNodesForRelation(
      ax::mojom::IntListAttribute attr) override;

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true), return
  // a set of all source nodes that have that relationship attribute between
  // them and this delegate's node.
  std::set<AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntAttribute attr) override;

  // Given a node ID list attribute (one where IsNodeIdIntListAttribute is
  // true) return a set of all source nodes that have that relationship
  // attribute between them and this delegate's node.
  std::set<AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntListAttribute attr) override;

  base::string16 GetAuthorUniqueId() const override;

  const AXUniqueId& GetUniqueId() const override;

  base::Optional<int> FindTextBoundary(
      ax::mojom::TextBoundary boundary,
      int offset,
      ax::mojom::MoveDirection direction,
      ax::mojom::TextAffinity affinity) const override;

  const std::vector<gfx::NativeViewAccessible> GetUIADescendants()
      const override;

  std::string GetLanguage() const override;

  //
  // Tables. All of these should be called on a node that's a table-like
  // role, otherwise they return nullopt.
  //
  bool IsTable() const override;
  base::Optional<int> GetTableColCount() const override;
  base::Optional<int> GetTableRowCount() const override;
  base::Optional<int> GetTableAriaColCount() const override;
  base::Optional<int> GetTableAriaRowCount() const override;
  base::Optional<int> GetTableCellCount() const override;
  base::Optional<bool> GetTableHasColumnOrRowHeaderNode() const override;
  std::vector<int32_t> GetColHeaderNodeIds() const override;
  std::vector<int32_t> GetColHeaderNodeIds(int col_index) const override;
  std::vector<int32_t> GetRowHeaderNodeIds() const override;
  std::vector<int32_t> GetRowHeaderNodeIds(int row_index) const override;
  AXPlatformNode* GetTableCaption() const override;

  // Table row-like nodes.
  bool IsTableRow() const override;
  base::Optional<int> GetTableRowRowIndex() const override;

  // Table cell-like nodes.
  bool IsTableCellOrHeader() const override;
  base::Optional<int> GetTableCellIndex() const override;
  base::Optional<int> GetTableCellColIndex() const override;
  base::Optional<int> GetTableCellRowIndex() const override;
  base::Optional<int> GetTableCellColSpan() const override;
  base::Optional<int> GetTableCellRowSpan() const override;
  base::Optional<int> GetTableCellAriaColIndex() const override;
  base::Optional<int> GetTableCellAriaRowIndex() const override;
  base::Optional<int32_t> GetCellId(int row_index,
                                    int col_index) const override;
  base::Optional<int32_t> CellIndexToId(int cell_index) const override;

  // Helper methods to check if a cell is an ARIA-1.1+ 'cell' or 'gridcell'
  bool IsCellOrHeaderOfARIATable() const override;
  bool IsCellOrHeaderOfARIAGrid() const override;

  // Ordered-set-like and item-like nodes.
  bool IsOrderedSetItem() const override;
  bool IsOrderedSet() const override;
  base::Optional<int> GetPosInSet() const override;
  base::Optional<int> GetSetSize() const override;

  //
  // Events.
  //

  // Return the platform-native GUI object that should be used as a target
  // for accessibility events.
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;

  //
  // Actions.
  //

  // Perform an accessibility action, switching on the ax::mojom::Action
  // provided in |data|.
  bool AccessibilityPerformAction(const AXActionData& data) override;

  //
  // Localized strings.
  //

  base::string16 GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const override;
  base::string16 GetLocalizedRoleDescriptionForUnlabeledImage() const override;
  base::string16 GetLocalizedStringForLandmarkType() const override;
  base::string16 GetLocalizedStringForRoleDescription() const override;
  base::string16 GetStyleNameAttributeAsLocalizedString() const override;

  //
  // Testing.
  //

  // Accessibility objects can have the "hot tracked" state set when
  // the mouse is hovering over them, but this makes tests flaky because
  // the test behaves differently when the mouse happens to be over an
  // element. The default value should be falses if not in testing mode.
  bool ShouldIgnoreHoveredStateForTesting() override;

 protected:
  std::string SubtreeToStringHelper(size_t level) override;

  // Given a list of node ids, return the nodes in this delegate's tree to
  // which they correspond.
  std::set<ui::AXPlatformNode*> GetNodesForNodeIds(
      const std::set<int32_t>& ids);

  AXPlatformNodeDelegate* GetParentDelegate();

 private:
  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeDelegateBase);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_BASE_H_
