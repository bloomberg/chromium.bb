// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_

#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_export.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/native_widget_types.h"

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
  // Get the accessibility data that should be exposed for this node.
  // Virtually all of the information is obtained from this structure
  // (role, state, name, cursor position, etc.) - the rest of this interface
  // is mostly to implement support for walking the accessibility tree.
  virtual const AXNodeData& GetData() const = 0;

  // Get the accessibility tree data for this node.
  virtual const AXTreeData& GetTreeData() const = 0;

  // Get the window the node is contained in.
  virtual gfx::NativeWindow GetTopLevelWidget() = 0;

  // Get the parent of the node, which may be an AXPlatformNode or it may
  // be a native accessible object implemented by another class.
  virtual gfx::NativeViewAccessible GetParent() = 0;

  // Get the index in parent. Typically this is the AXNode's index_in_parent_.
  virtual int GetIndexInParent() const = 0;

  // Get the number of children of this node.
  virtual int GetChildCount() = 0;

  // Get the child of a node given a 0-based index.
  virtual gfx::NativeViewAccessible ChildAtIndex(int index) = 0;

  // Get the bounds of this node in screen coordinates.
  virtual gfx::Rect GetScreenBoundsRect() const = 0;

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

  virtual AXPlatformNode* GetFromNodeID(int32_t id) = 0;

  //
  // Events.
  //

  // Return the platform-native GUI object that should be used as a target
  // for accessibility events.
  virtual gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() = 0;

  //
  // Actions.
  //

  // Perform an accessibility action, switching on the AXAction
  // provided in |data|.
  virtual bool AccessibilityPerformAction(const AXActionData& data) = 0;

  //
  // Testing.
  //

  // Accessibility objects can have the "hot tracked" state set when
  // the mouse is hovering over them, but this makes tests flaky because
  // the test behaves differently when the mouse happens to be over an
  // element. The default value should be falses if not in testing mode.
  virtual bool ShouldIgnoreHoveredStateForTesting() = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_
