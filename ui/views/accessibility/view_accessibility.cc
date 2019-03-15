// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility.h"

#include <algorithm>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/buildflags.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

bool IsValidRoleForViews(ax::mojom::Role role) {
  switch (role) {
    // These roles all have special meaning and shouldn't ever be
    // set on a View.
    case ax::mojom::Role::kDesktop:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kSvgRoot:
    case ax::mojom::Role::kUnknown:
    case ax::mojom::Role::kWebArea:
      return false;

    default:
      return true;
  }
}

}  // namespace

#if !BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY()
// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  // Cannot use std::make_unique because constructor is protected.
  return base::WrapUnique(new ViewAccessibility(view));
}
#endif

ViewAccessibility::ViewAccessibility(View* view)
    : view_(view),
      focused_virtual_child_(nullptr),
      is_leaf_(false),
      is_ignored_(false) {}

ViewAccessibility::~ViewAccessibility() = default;

void ViewAccessibility::AddVirtualChildView(
    std::unique_ptr<AXVirtualView> virtual_view) {
  DCHECK(virtual_view);
  if (virtual_view->parent_view() == this)
    return;
  AddVirtualChildViewAt(std::move(virtual_view), virtual_child_count());
}

void ViewAccessibility::AddVirtualChildViewAt(
    std::unique_ptr<AXVirtualView> virtual_view,
    int index) {
  DCHECK(virtual_view);
  DCHECK(!virtual_view->parent_view()) << "This |view| already has a View "
                                          "parent. Call RemoveVirtualChildView "
                                          "first.";
  DCHECK(!virtual_view->virtual_parent_view()) << "This |view| already has an "
                                                  "AXVirtualView parent. Call "
                                                  "RemoveChildView first.";
  DCHECK_GE(index, 0);
  DCHECK_LE(index, virtual_child_count());

  virtual_view->set_parent_view(this);
  virtual_children_.insert(virtual_children_.begin() + index,
                           std::move(virtual_view));
}

std::unique_ptr<AXVirtualView> ViewAccessibility::RemoveVirtualChildView(
    AXVirtualView* virtual_view) {
  DCHECK(virtual_view);
  int cur_index = GetIndexOf(virtual_view);
  if (cur_index < 0)
    return {};

  std::unique_ptr<AXVirtualView> child =
      std::move(virtual_children_[cur_index]);
  virtual_children_.erase(virtual_children_.begin() + cur_index);
  child->set_parent_view(nullptr);
  child->UnsetPopulateDataCallback();
  if (focused_virtual_child_ && child->Contains(focused_virtual_child_))
    focused_virtual_child_ = nullptr;
  return child;
}

void ViewAccessibility::RemoveAllVirtualChildViews() {
  while (!virtual_children_.empty())
    RemoveVirtualChildView(virtual_children_.back().get());
}

bool ViewAccessibility::Contains(const AXVirtualView* virtual_view) const {
  DCHECK(virtual_view);
  for (const auto& virtual_child : virtual_children_) {
    // AXVirtualView::Contains() also checks if the provided virtual view is the
    // same as |this|.
    if (virtual_child->Contains(virtual_view))
      return true;
  }
  return false;
}

int ViewAccessibility::GetIndexOf(const AXVirtualView* virtual_view) const {
  DCHECK(virtual_view);
  const auto iter =
      std::find_if(virtual_children_.begin(), virtual_children_.end(),
                   [virtual_view](const auto& child) {
                     return child.get() == virtual_view;
                   });
  return iter != virtual_children_.end()
             ? static_cast<int>(iter - virtual_children_.begin())
             : -1;
}

const ui::AXUniqueId& ViewAccessibility::GetUniqueId() const {
  return unique_id_;
}

void ViewAccessibility::GetAccessibleNodeData(ui::AXNodeData* data) const {
  // Views may misbehave if their widget is closed; return an unknown role
  // rather than possibly crashing.
  const views::Widget* widget = view_->GetWidget();
  if (!widget || !widget->widget_delegate() || widget->IsClosed()) {
    data->role = ax::mojom::Role::kUnknown;
    data->SetRestriction(ax::mojom::Restriction::kDisabled);
    return;
  }

  view_->GetAccessibleNodeData(data);
  if (custom_data_.role != ax::mojom::Role::kUnknown)
    data->role = custom_data_.role;

  if (custom_data_.HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    data->SetName(
        custom_data_.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  if (custom_data_.HasStringAttribute(
          ax::mojom::StringAttribute::kDescription)) {
    data->SetDescription(custom_data_.GetStringAttribute(
        ax::mojom::StringAttribute::kDescription));
  }

  static const ax::mojom::IntAttribute kOverridableIntAttributes[]{
      ax::mojom::IntAttribute::kPosInSet,
      ax::mojom::IntAttribute::kSetSize,
  };

  for (auto attribute : kOverridableIntAttributes) {
    if (custom_data_.HasIntAttribute(attribute))
      data->AddIntAttribute(attribute, custom_data_.GetIntAttribute(attribute));
  }

  if (!data->HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    base::string16 tooltip = view_->GetTooltipText(gfx::Point());
    // Some screen readers announce the accessible description right after the
    // accessible name. Only use the tooltip as the accessible description if
    // it's different from the name, otherwise users might be puzzled as to why
    // their screen reader is announcing the same thing twice.
    if (tooltip !=
        data->GetString16Attribute(ax::mojom::StringAttribute::kName)) {
      data->AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                               base::UTF16ToUTF8(tooltip));
    }
  }

  data->relative_bounds.bounds = gfx::RectF(view_->GetBoundsInScreen());
  if (!custom_data_.relative_bounds.bounds.IsEmpty())
    data->relative_bounds.bounds = custom_data_.relative_bounds.bounds;

  data->AddStringAttribute(ax::mojom::StringAttribute::kClassName,
                           view_->GetClassName());

  if (view_->IsAccessibilityFocusable())
    data->AddState(ax::mojom::State::kFocusable);

  if (!view_->enabled())
    data->SetRestriction(ax::mojom::Restriction::kDisabled);

  if (!view_->visible() && data->role != ax::mojom::Role::kAlert)
    data->AddState(ax::mojom::State::kInvisible);

  if (view_->context_menu_controller())
    data->AddAction(ax::mojom::Action::kShowContextMenu);
}

void ViewAccessibility::OverrideFocus(AXVirtualView* virtual_view) {
  DCHECK(!virtual_view || Contains(virtual_view))
      << "|virtual_view| must be nullptr or a descendant of this view.";
  focused_virtual_child_ = virtual_view;
}

void ViewAccessibility::OverrideRole(const ax::mojom::Role role) {
  DCHECK(IsValidRoleForViews(role)) << "Invalid role for Views.";
  custom_data_.role = role;
}

void ViewAccessibility::OverrideName(const std::string& name) {
  custom_data_.SetName(name);
}

void ViewAccessibility::OverrideName(const base::string16& name) {
  custom_data_.SetName(name);
}

void ViewAccessibility::OverrideDescription(const std::string& description) {
  custom_data_.SetDescription(description);
}

void ViewAccessibility::OverrideDescription(const base::string16& description) {
  custom_data_.SetDescription(description);
}

void ViewAccessibility::OverrideIsLeaf(bool value) {
  is_leaf_ = value;
}

void ViewAccessibility::OverrideIsIgnored(bool value) {
  is_ignored_ = value;
}

void ViewAccessibility::OverrideBounds(const gfx::RectF& bounds) {
  custom_data_.relative_bounds.bounds = bounds;
}

void ViewAccessibility::OverridePosInSet(int pos_in_set, int set_size) {
  custom_data_.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, pos_in_set);
  custom_data_.AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size);
}

gfx::NativeViewAccessible ViewAccessibility::GetNativeObject() {
  return nullptr;
}

gfx::NativeViewAccessible ViewAccessibility::GetFocusedDescendant() {
  if (focused_virtual_child_)
    return focused_virtual_child_->GetNativeObject();
  return view_->GetNativeViewAccessible();
}

}  // namespace views
