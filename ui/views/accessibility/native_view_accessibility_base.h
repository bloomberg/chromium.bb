// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_BASE_H_
#define UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_BASE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class View;
class Widget;

// Shared base class for platforms that require an implementation of
// NativeViewAccessibility to interface with the native accessibility toolkit.
class VIEWS_EXPORT NativeViewAccessibilityBase
    : public NativeViewAccessibility,
      public ui::AXPlatformNodeDelegate,
      public WidgetObserver {
 public:
  ~NativeViewAccessibilityBase() override;

  // NativeViewAccessibility:
  gfx::NativeViewAccessible GetNativeObject() override;
  void NotifyAccessibilityEvent(ui::AXEvent event_type) override;

  // ui::AXPlatformNodeDelegate
  const ui::AXNodeData& GetData() const override;
  const ui::AXTreeData& GetTreeData() const override;
  int GetChildCount() override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  gfx::NativeWindow GetTopLevelWidget() override;
  gfx::NativeViewAccessible GetParent() override;
  gfx::Rect GetScreenBoundsRect() const override;
  gfx::NativeViewAccessible HitTestSync(int x, int y) override;
  gfx::NativeViewAccessible GetFocus() override;
  ui::AXPlatformNode* GetFromNodeID(int32_t id) override;
  int GetIndexInParent() const override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;
  bool AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  bool IsOffscreen() const override;

  // WidgetObserver
  void OnWidgetDestroying(Widget* widget) override;

  Widget* parent_widget() const { return parent_widget_; }
  void SetParentWidget(Widget* parent_widget);

 protected:
  explicit NativeViewAccessibilityBase(View* view);

  // Weak. Owns this.
  View* view_;

  // Weak. Uses WidgetObserver to clear. This is set on the root view for
  // a widget that's owned by another widget, so we can walk back up the
  // tree.
  Widget* parent_widget_;

 protected:
  virtual gfx::RectF GetBoundsInScreen() const;

 private:
  void PopulateChildWidgetVector(std::vector<Widget*>* result_child_widgets);

  // We own this, but it is reference-counted on some platforms so we can't use
  // a scoped_ptr. It is dereferenced in the destructor.
  ui::AXPlatformNode* ax_node_;

  mutable ui::AXNodeData data_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewAccessibilityBase);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_BASE_H_
