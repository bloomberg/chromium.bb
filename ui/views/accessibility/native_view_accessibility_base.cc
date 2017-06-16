// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/native_view_accessibility_base.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

bool IsAccessibilityFocusableWhenEnabled(View* view) {
  return view->focus_behavior() != View::FocusBehavior::NEVER &&
         view->IsDrawn();
}

// Used to determine if a View should be ignored by accessibility clients by
// being a non-keyboard-focusable child of a keyboard-focusable ancestor. E.g.,
// LabelButtons contain Labels, but a11y should just show that there's a button.
bool IsViewUnfocusableChildOfFocusableAncestor(View* view) {
  if (IsAccessibilityFocusableWhenEnabled(view))
    return false;

  while (view->parent()) {
    view = view->parent();
    if (IsAccessibilityFocusableWhenEnabled(view))
      return true;
  }
  return false;
}

}  // namespace

NativeViewAccessibilityBase::NativeViewAccessibilityBase(View* view)
    : view_(view),
      parent_widget_(nullptr),
      ax_node_(ui::AXPlatformNode::Create(this)) {
  DCHECK(ax_node_);
}

NativeViewAccessibilityBase::~NativeViewAccessibilityBase() {
  ax_node_->Destroy();
  if (parent_widget_)
    parent_widget_->RemoveObserver(this);
}

gfx::NativeViewAccessible NativeViewAccessibilityBase::GetNativeObject() {
  return ax_node_->GetNativeViewAccessible();
}

void NativeViewAccessibilityBase::NotifyAccessibilityEvent(
    ui::AXEvent event_type) {
  ax_node_->NotifyAccessibilityEvent(event_type);
}

// ui::AXPlatformNodeDelegate

const ui::AXNodeData& NativeViewAccessibilityBase::GetData() const {
  data_ = ui::AXNodeData();

  // Views may misbehave if their widget is closed; return an unknown role
  // rather than possibly crashing.
  if (!view_->GetWidget() || view_->GetWidget()->IsClosed()) {
    data_.role = ui::AX_ROLE_UNKNOWN;
    data_.AddState(ui::AX_STATE_DISABLED);
    return data_;
  }

  view_->GetAccessibleNodeData(&data_);
  data_.location = GetBoundsInScreen();
  base::string16 description;
  view_->GetTooltipText(gfx::Point(), &description);
  data_.AddStringAttribute(ui::AX_ATTR_DESCRIPTION,
                           base::UTF16ToUTF8(description));

  if (view_->IsAccessibilityFocusable())
    data_.AddState(ui::AX_STATE_FOCUSABLE);

  if (!view_->enabled())
    data_.AddState(ui::AX_STATE_DISABLED);

  if (!view_->IsDrawn())
    data_.AddState(ui::AX_STATE_INVISIBLE);

  // Make sure this element is excluded from the a11y tree if there's a
  // focusable parent. All keyboard focusable elements should be leaf nodes.
  // Exceptions to this rule will themselves be accessibility focusable.
  if (IsViewUnfocusableChildOfFocusableAncestor(view_))
    data_.role = ui::AX_ROLE_IGNORED;
  return data_;
}

const ui::AXTreeData& NativeViewAccessibilityBase::GetTreeData() const {
  CR_DEFINE_STATIC_LOCAL(ui::AXTreeData, empty_data, ());
  return empty_data;
}

int NativeViewAccessibilityBase::GetChildCount() {
  int child_count = view_->child_count();

  std::vector<Widget*> child_widgets;
  PopulateChildWidgetVector(&child_widgets);
  child_count += child_widgets.size();

  return child_count;
}

gfx::NativeViewAccessible NativeViewAccessibilityBase::ChildAtIndex(int index) {
  // If this is a root view, our widget might have child widgets. Include
  std::vector<Widget*> child_widgets;
  PopulateChildWidgetVector(&child_widgets);
  int child_widget_count = static_cast<int>(child_widgets.size());

  if (index < view_->child_count()) {
    return view_->child_at(index)->GetNativeViewAccessible();
  } else if (index < view_->child_count() + child_widget_count) {
    Widget* child_widget = child_widgets[index - view_->child_count()];
    return child_widget->GetRootView()->GetNativeViewAccessible();
  }

  return nullptr;
}

gfx::NativeWindow NativeViewAccessibilityBase::GetTopLevelWidget() {
  if (view_->GetWidget())
    return view_->GetWidget()->GetTopLevelWidget()->GetNativeWindow();
  return nullptr;
}

gfx::NativeViewAccessible NativeViewAccessibilityBase::GetParent() {
  if (view_->parent())
    return view_->parent()->GetNativeViewAccessible();

  if (parent_widget_)
    return parent_widget_->GetRootView()->GetNativeViewAccessible();

  return nullptr;
}

gfx::Rect NativeViewAccessibilityBase::GetScreenBoundsRect() const {
  return view_->GetBoundsInScreen();
}

gfx::NativeViewAccessible NativeViewAccessibilityBase::HitTestSync(int x,
                                                                   int y) {
  if (!view_ || !view_->GetWidget())
    return nullptr;

  // Search child widgets first, since they're on top in the z-order.
  std::vector<Widget*> child_widgets;
  PopulateChildWidgetVector(&child_widgets);
  for (Widget* child_widget : child_widgets) {
    View* child_root_view = child_widget->GetRootView();
    gfx::Point point(x, y);
    View::ConvertPointFromScreen(child_root_view, &point);
    if (child_root_view->HitTestPoint(point))
      return child_root_view->GetNativeViewAccessible();
  }

  gfx::Point point(x, y);
  View::ConvertPointFromScreen(view_, &point);
  if (!view_->HitTestPoint(point))
    return nullptr;

  // Check if the point is within any of the immediate children of this
  // view. We don't have to search further because AXPlatformNode will
  // do a recursive hit test if we return anything other than |this| or NULL.
  for (int i = view_->child_count() - 1; i >= 0; --i) {
    View* child_view = view_->child_at(i);
    if (!child_view->visible())
      continue;

    gfx::Point point_in_child_coords(point);
    view_->ConvertPointToTarget(view_, child_view, &point_in_child_coords);
    if (child_view->HitTestPoint(point_in_child_coords))
      return child_view->GetNativeViewAccessible();
  }

  // If it's not inside any of our children, it's inside this view.
  return GetNativeObject();
}

gfx::NativeViewAccessible NativeViewAccessibilityBase::GetFocus() {
  FocusManager* focus_manager = view_->GetFocusManager();
  View* focused_view =
      focus_manager ? focus_manager->GetFocusedView() : nullptr;
  return focused_view ? focused_view->GetNativeViewAccessible() : nullptr;
}

gfx::AcceleratedWidget
NativeViewAccessibilityBase::GetTargetForNativeAccessibilityEvent() {
  return gfx::kNullAcceleratedWidget;
}

bool NativeViewAccessibilityBase::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  return view_->HandleAccessibleAction(data);
}

void NativeViewAccessibilityBase::OnWidgetDestroying(Widget* widget) {
  if (parent_widget_ == widget) {
    parent_widget_->RemoveObserver(this);
    parent_widget_ = nullptr;
  }
}

void NativeViewAccessibilityBase::SetParentWidget(Widget* parent_widget) {
  if (parent_widget_)
    parent_widget_->RemoveObserver(this);
  parent_widget_ = parent_widget;
  parent_widget_->AddObserver(this);
}

gfx::RectF NativeViewAccessibilityBase::GetBoundsInScreen() const {
  return gfx::RectF(view_->GetBoundsInScreen());
}

void NativeViewAccessibilityBase::PopulateChildWidgetVector(
    std::vector<Widget*>* result_child_widgets) {
  // Only attach child widgets to the root view.
  Widget* widget = view_->GetWidget();
  // Note that during window close, a Widget may exist in a state where it has
  // no NativeView, but hasn't yet torn down its view hierarchy.
  if (!widget || !widget->GetNativeView() || widget->GetRootView() != view_)
    return;

  std::set<Widget*> child_widgets;
  Widget::GetAllOwnedWidgets(widget->GetNativeView(), &child_widgets);
  for (auto iter = child_widgets.begin(); iter != child_widgets.end(); ++iter) {
    Widget* child_widget = *iter;
    DCHECK_NE(widget, child_widget);

    if (!child_widget->IsVisible())
      continue;

    if (widget->GetNativeWindowProperty(kWidgetNativeViewHostKey))
      continue;

    gfx::NativeViewAccessible child_widget_accessible =
        child_widget->GetRootView()->GetNativeViewAccessible();
    ui::AXPlatformNode* child_widget_platform_node =
        ui::AXPlatformNode::FromNativeViewAccessible(child_widget_accessible);
    if (child_widget_platform_node) {
      NativeViewAccessibilityBase* child_widget_view_accessibility =
          static_cast<NativeViewAccessibilityBase*>(
              child_widget_platform_node->GetDelegate());
      if (child_widget_view_accessibility->parent_widget() != widget)
        child_widget_view_accessibility->SetParentWidget(widget);
    }

    result_child_widgets->push_back(child_widget);
  }
}

}  // namespace views
