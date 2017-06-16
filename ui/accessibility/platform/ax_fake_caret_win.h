// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_FAKE_CARET_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_FAKE_CARET_WIN_H_

#include <oleacc.h>

#include "base/macros.h"
#include "base/win/scoped_comptr.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class AXPlatformNodeWin;

// A class representing the position of the caret to assistive software on
// Windows. This is required because Chrome doesn't use the standard system
// caret and because some assistive software still relies on specific
// accessibility APIs to retrieve the caret position.
class AX_EXPORT AXFakeCaretWin : private AXPlatformNodeDelegate {
 public:
  AXFakeCaretWin(gfx::AcceleratedWidget event_target);
  virtual ~AXFakeCaretWin();

  base::win::ScopedComPtr<IAccessible> GetCaret() const;
  void MoveCaretTo(const gfx::Rect& bounds);

 private:
  // |AXPlatformNodeDelegate| members.
  const AXNodeData& GetData() const override;
  const ui::AXTreeData& GetTreeData() const override;
  gfx::NativeWindow GetTopLevelWidget() override;
  gfx::NativeViewAccessible GetParent() override;
  int GetChildCount() override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  gfx::Rect GetScreenBoundsRect() const override;
  gfx::NativeViewAccessible HitTestSync(int x, int y) override;
  gfx::NativeViewAccessible GetFocus() override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;
  bool AccessibilityPerformAction(const ui::AXActionData& data) override;

  AXPlatformNodeWin* caret_;
  gfx::AcceleratedWidget event_target_;
  AXNodeData data_;

  friend class AXPlatformNodeWin;
  DISALLOW_COPY_AND_ASSIGN(AXFakeCaretWin);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_FAKE_CARET_WIN_H_
