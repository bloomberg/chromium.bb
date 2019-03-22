// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view.h"

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
const char AXVirtualView::kViewClassName[] = "AXVirtualView";

AXVirtualView::AXVirtualView()
    : parent_view_(nullptr), virtual_parent_view_(nullptr) {
  ax_platform_node_ = ui::AXPlatformNode::Create(this);
  DCHECK(ax_platform_node_);
  custom_data_.AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                                  GetViewClassName());
}

AXVirtualView::~AXVirtualView() {
  DCHECK(!parent_view_ || !virtual_parent_view_)
      << "Either |parent_view_| or |virtual_parent_view_| could be set but "
         "not both.";

  if (ax_platform_node_) {
    ax_platform_node_->Destroy();
    ax_platform_node_ = nullptr;
  }
}

void AXVirtualView::AddChildView(std::unique_ptr<AXVirtualView> view) {
  DCHECK(view);
  if (view->virtual_parent_view_ == this)
    return;
  AddChildViewAt(std::move(view), GetChildCount());
}

void AXVirtualView::AddChildViewAt(std::unique_ptr<AXVirtualView> view,
                                   int index) {
  DCHECK(view);
  CHECK_NE(view.get(), this)
      << "You cannot add an AXVirtualView as its own child.";
  DCHECK(!view->parent_view_) << "This |view| already has a View "
                                 "parent. Call RemoveVirtualChildView first.";
  DCHECK(!view->virtual_parent_view_) << "This |view| already has an "
                                         "AXVirtualView parent. Call "
                                         "RemoveChildView first.";
  DCHECK_GE(index, 0);
  DCHECK_LE(index, GetChildCount());

  view->virtual_parent_view_ = this;
  children_.insert(children_.begin() + index, std::move(view));
}

void AXVirtualView::ReorderChildView(AXVirtualView* view, int index) {
  DCHECK(view);
  if (index >= GetChildCount())
    return;
  if (index < 0)
    index = GetChildCount() - 1;

  DCHECK_EQ(view->virtual_parent_view_, this);
  if (children_[index].get() == view)
    return;

  int cur_index = GetIndexOf(view);
  if (cur_index < 0)
    return;

  std::unique_ptr<AXVirtualView> child = std::move(children_[cur_index]);
  children_.erase(children_.begin() + cur_index);
  children_.insert(children_.begin() + index, std::move(child));
}

std::unique_ptr<AXVirtualView> AXVirtualView::RemoveChildView(
    AXVirtualView* view) {
  DCHECK(view);
  int cur_index = GetIndexOf(view);
  if (cur_index < 0)
    return {};

  if (GetOwnerView()) {
    ViewAccessibility& view_accessibility =
        GetOwnerView()->GetViewAccessibility();
    if (view_accessibility.FocusedVirtualChild() &&
        Contains(view_accessibility.FocusedVirtualChild())) {
      view_accessibility.OverrideFocus(nullptr);
    }
  }

  std::unique_ptr<AXVirtualView> child = std::move(children_[cur_index]);
  children_.erase(children_.begin() + cur_index);
  child->virtual_parent_view_ = nullptr;
  return child;
}

void AXVirtualView::RemoveAllChildViews() {
  while (!children_.empty())
    RemoveChildView(children_.back().get());
}

const AXVirtualView* AXVirtualView::child_at(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(children_.size()));
  return children_[index].get();
}

AXVirtualView* AXVirtualView::child_at(int index) {
  return const_cast<AXVirtualView*>(
      const_cast<const AXVirtualView*>(this)->child_at(index));
}

bool AXVirtualView::Contains(const AXVirtualView* view) const {
  DCHECK(view);
  for (const AXVirtualView* v = view; v; v = v->virtual_parent_view_) {
    if (v == this)
      return true;
  }
  return false;
}

int AXVirtualView::GetIndexOf(const AXVirtualView* view) const {
  DCHECK(view);
  const auto iter =
      std::find_if(children_.begin(), children_.end(),
                   [view](const auto& child) { return child.get() == view; });
  return iter != children_.end() ? static_cast<int>(iter - children_.begin())
                                 : -1;
}

const char* AXVirtualView::GetViewClassName() const {
  return kViewClassName;
}

gfx::NativeViewAccessible AXVirtualView::GetNativeObject() const {
  DCHECK(ax_platform_node_);
  return ax_platform_node_->GetNativeViewAccessible();
}

void AXVirtualView::NotifyAccessibilityEvent(ax::mojom::Event event_type) {
  DCHECK(ax_platform_node_);
  ax_platform_node_->NotifyAccessibilityEvent(event_type);
}

ui::AXNodeData& AXVirtualView::GetCustomData() {
  return custom_data_;
}

// ui::AXPlatformNodeDelegate

const ui::AXNodeData& AXVirtualView::GetData() const {
  // Make a copy of our |custom_data_| so that any modifications will not be
  // made to the data that users of this class will be manipulating.
  static ui::AXNodeData node_data;
  node_data = custom_data_;
  if (!GetOwnerView() || !GetOwnerView()->enabled())
    node_data.SetRestriction(ax::mojom::Restriction::kDisabled);

  if (!GetOwnerView() || !GetOwnerView()->IsDrawn())
    node_data.AddState(ax::mojom::State::kInvisible);

  if (GetOwnerView() && GetOwnerView()->context_menu_controller())
    node_data.AddAction(ax::mojom::Action::kShowContextMenu);

  return node_data;
}

int AXVirtualView::GetChildCount() {
  return static_cast<int>(children_.size());
}

gfx::NativeViewAccessible AXVirtualView::ChildAtIndex(int index) {
  DCHECK_GE(index, 0) << "Child indices should be greater or equal to 0.";
  DCHECK_LT(index, GetChildCount())
      << "Child indices should be less than the child count.";
  if (index >= 0 && index < GetChildCount())
    return children_[index]->GetNativeObject();
  return nullptr;
}

gfx::NativeViewAccessible AXVirtualView::GetNSWindow() {
  NOTREACHED();
  return nullptr;
}

gfx::NativeViewAccessible AXVirtualView::GetParent() {
  if (parent_view_)
    return parent_view_->GetNativeObject();

  if (virtual_parent_view_)
    return virtual_parent_view_->GetNativeObject();

  // This virtual view hasn't been added to a parent view yet.
  return nullptr;
}

gfx::Rect AXVirtualView::GetClippedScreenBoundsRect() const {
  // We could optionally add clipping here if ever needed.
  // TODO(nektar): Implement bounds that are relative to the parent.
  return gfx::ToEnclosingRect(custom_data_.relative_bounds.bounds);
}

gfx::Rect AXVirtualView::GetUnclippedScreenBoundsRect() const {
  // TODO(nektar): Implement bounds that are relative to the parent.
  return gfx::ToEnclosingRect(custom_data_.relative_bounds.bounds);
}

gfx::NativeViewAccessible AXVirtualView::HitTestSync(int x, int y) {
  // TODO(nektar): Implement.
  return GetNativeObject();
}

gfx::NativeViewAccessible AXVirtualView::GetFocus() {
  if (parent_view_)
    return parent_view_->GetFocusedDescendant();

  if (virtual_parent_view_)
    return virtual_parent_view_->GetFocus();

  // This virtual view hasn't been added to a parent view yet.
  return nullptr;
}

ui::AXPlatformNode* AXVirtualView::GetFromNodeID(int32_t id) {
  // TODO(nektar): Implement.
  return nullptr;
}

bool AXVirtualView::AccessibilityPerformAction(const ui::AXActionData& data) {
  bool result = false;
  if (custom_data_.HasAction(data.action))
    result = HandleAccessibleAction(data);
  if (!result && GetOwnerView())
    return GetOwnerView()->HandleAccessibleAction(data);
  return result;
}

bool AXVirtualView::ShouldIgnoreHoveredStateForTesting() {
  // TODO(nektar): Implement.
  return false;
}

bool AXVirtualView::IsOffscreen() const {
  // TODO(nektar): Implement.
  return false;
}

const ui::AXUniqueId& AXVirtualView::GetUniqueId() const {
  return unique_id_;
}

bool AXVirtualView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  return false;
}

View* AXVirtualView::GetOwnerView() const {
  if (parent_view_)
    return parent_view_->view();

  if (virtual_parent_view_)
    return virtual_parent_view_->GetOwnerView();

  // This virtual view hasn't been added to a parent view yet.
  return nullptr;
}

}  // namespace views
