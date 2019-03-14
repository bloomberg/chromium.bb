// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_

#include <set>
#include <utility>
#include <vector>

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

class Rect;
}

namespace ui {

struct AXActionData;
struct AXNodeData;
struct AXTreeData;
class AXPlatformNode;

// An object that wants to be accessible should derive from this class.
// AXPlatformNode subclasses use this interface to query all of the information
// about the object in order to implement native accessibility APIs.
//
// Note that AXPlatformNode has support for accessibility trees where some
// of the objects in the tree are not implemented using AXPlatformNode.
// For example, you may have a native window with platform-native widgets
// in it, but in that window you have custom controls that use AXPlatformNode
// to provide accessibility. That's why GetParent, ChildAtIndex, HitTestSync,
// and GetFocus all return a gfx::NativeViewAccessible - so you can return a
// native accessible if necessary, and AXPlatformNode::GetNativeViewAccessible
// otherwise.
class AX_EXPORT AXPlatformNodeDelegate {
 public:
  virtual ~AXPlatformNodeDelegate() = default;

  // Get the accessibility data that should be exposed for this node.
  // Virtually all of the information is obtained from this structure
  // (role, state, name, cursor position, etc.) - the rest of this interface
  // is mostly to implement support for walking the accessibility tree.
  virtual const AXNodeData& GetData() const = 0;

  // Get the accessibility tree data for this node.
  virtual const AXTreeData& GetTreeData() const = 0;

  // Creates a text position rooted at this object.
  virtual AXNodePosition::AXPositionInstance CreateTextPositionAt(
      int offset,
      ax::mojom::TextAffinity affinity =
          ax::mojom::TextAffinity::kDownstream) const = 0;

  // Get the accessibility node for the NSWindow the node is contained in. This
  // method is only meaningful on macOS.
  virtual gfx::NativeViewAccessible GetNSWindow() = 0;

  // Get the parent of the node, which may be an AXPlatformNode or it may
  // be a native accessible object implemented by another class.
  virtual gfx::NativeViewAccessible GetParent() = 0;

  // Get the index in parent. Typically this is the AXNode's index_in_parent_.
  virtual int GetIndexInParent() const = 0;

  // Get the number of children of this node.
  virtual int GetChildCount() = 0;

  // Get the child of a node given a 0-based index.
  virtual gfx::NativeViewAccessible ChildAtIndex(int index) = 0;

  // Get the bounds of this node in screen coordinates, applying clipping
  // to all bounding boxes so that the resulting rect is within the window.
  virtual gfx::Rect GetClippedScreenBoundsRect() const = 0;

  // Get the bounds of this node in screen coordinates without applying
  // any clipping; it may be outside of the window or offscreen.
  virtual gfx::Rect GetUnclippedScreenBoundsRect() const = 0;

  // Returns the bounds of the given range in screen coordinates. Only valid
  // when the role is WebAXRoleStaticText.
  virtual gfx::Rect GetScreenBoundsForRange(int start,
                                            int len,
                                            bool clipped = false) const = 0;

  // Do a *synchronous* hit test of the given location in global screen
  // coordinates, and the node within this node's subtree (inclusive) that's
  // hit, if any.
  //
  // If the result is anything other than this object or NULL, it will be
  // hit tested again recursively - that allows hit testing to work across
  // implementation classes. It's okay to take advantage of this and return
  // only an immediate child and not the deepest descendant.
  //
  // This function is mainly used by accessibility debugging software.
  // Platforms with touch accessibility use a different asynchronous interface.
  virtual gfx::NativeViewAccessible HitTestSync(int x, int y) = 0;

  // Return the node within this node's subtree (inclusive) that currently
  // has focus.
  virtual gfx::NativeViewAccessible GetFocus() = 0;

  // Get whether this node is offscreen.
  virtual bool IsOffscreen() const = 0;

  // Get whether this node is in web content.
  virtual bool IsWebContent() const = 0;

  virtual AXPlatformNode* GetFromNodeID(int32_t id) = 0;

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true), return
  // a target nodes for which this delegate's node has that relationship
  // attribute or NULL if there is no such relationship.
  virtual AXPlatformNode* GetTargetNodeForRelation(
      ax::mojom::IntAttribute attr) = 0;

  // Given a node ID attribute (one where IsNodeIdIntListAttribute is true),
  // return a set of all target nodes for which this delegate's node has that
  // relationship attribute.
  virtual std::set<AXPlatformNode*> GetTargetNodesForRelation(
      ax::mojom::IntListAttribute attr) = 0;

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true), return
  // a set of all source nodes that have that relationship attribute between
  // them and this delegate's node.
  virtual std::set<AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntAttribute attr) = 0;

  // Given a node ID list attribute (one where IsNodeIdIntListAttribute is
  // true), return a set of all source nodes that have that relationship
  // attribute between them and this delegate's node.
  virtual std::set<AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntListAttribute attr) = 0;

  virtual const AXUniqueId& GetUniqueId() const = 0;

  // This method finds text boundaries in the text used for platform text APIs.
  // Implementations may use side-channel data such as line or word indices to
  // produce appropriate results.
  using EnclosingBoundaryOffsets = base::Optional<std::pair<size_t, size_t>>;
  virtual EnclosingBoundaryOffsets FindTextBoundariesAtOffset(
      TextBoundaryType boundary_type,
      int offset,
      ax::mojom::TextAffinity affinity) const = 0;

  //
  // Tables. All of these should be called on a node that's a table-like
  // role.
  //
  virtual bool IsTable() const = 0;
  virtual int32_t GetTableColCount() const = 0;
  virtual int32_t GetTableRowCount() const = 0;
  virtual base::Optional<int32_t> GetTableAriaColCount() const = 0;
  virtual base::Optional<int32_t> GetTableAriaRowCount() const = 0;
  virtual int32_t GetTableCellCount() const = 0;
  virtual const std::vector<int32_t> GetColHeaderNodeIds() const = 0;
  virtual const std::vector<int32_t> GetColHeaderNodeIds(
      int32_t col_index) const = 0;
  virtual const std::vector<int32_t> GetRowHeaderNodeIds() const = 0;
  virtual const std::vector<int32_t> GetRowHeaderNodeIds(
      int32_t row_index) const = 0;
  virtual AXPlatformNode* GetTableCaption() = 0;

  // Table row-like nodes.
  virtual bool IsTableRow() const = 0;
  virtual int32_t GetTableRowRowIndex() const = 0;

  // Table cell-like nodes.
  virtual bool IsTableCellOrHeader() const = 0;
  virtual int32_t GetTableCellIndex() const = 0;
  virtual int32_t GetTableCellColIndex() const = 0;
  virtual int32_t GetTableCellRowIndex() const = 0;
  virtual int32_t GetTableCellColSpan() const = 0;
  virtual int32_t GetTableCellRowSpan() const = 0;
  virtual int32_t GetTableCellAriaColIndex() const = 0;
  virtual int32_t GetTableCellAriaRowIndex() const = 0;
  virtual int32_t GetCellId(int32_t row_index, int32_t col_index) const = 0;
  virtual int32_t CellIndexToId(int32_t cell_index) const = 0;

  // Ordered-set-like and item-like nodes.
  virtual bool IsOrderedSetItem() const = 0;
  virtual bool IsOrderedSet() const = 0;
  virtual int32_t GetPosInSet() const = 0;
  virtual int32_t GetSetSize() const = 0;

  //
  // Events.
  //

  // Return the platform-native GUI object that should be used as a target
  // for accessibility events.
  virtual gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() = 0;

  //
  // Actions.
  //

  // Perform an accessibility action, switching on the ax::mojom::Action
  // provided in |data|.
  virtual bool AccessibilityPerformAction(const AXActionData& data) = 0;

  //
  // Localized strings.
  //

  virtual base::string16 GetLocalizedRoleDescriptionForUnlabeledImage()
      const = 0;
  virtual base::string16 GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const = 0;

  //
  // Testing.
  //

  // Accessibility objects can have the "hot tracked" state set when
  // the mouse is hovering over them, but this makes tests flaky because
  // the test behaves differently when the mouse happens to be over an
  // element. The default value should be falses if not in testing mode.
  virtual bool ShouldIgnoreHoveredStateForTesting() = 0;

 protected:
  AXPlatformNodeDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeDelegate);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_
