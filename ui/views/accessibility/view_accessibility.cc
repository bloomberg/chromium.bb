// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/ui_features.h"
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

#if !BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY()
// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  return std::make_unique<ViewAccessibility>(view);
}
#endif

ViewAccessibility::ViewAccessibility(View* view) : view_(view) {}

ViewAccessibility::~ViewAccessibility() {}

ui::AXNodeData ViewAccessibility::GetAccessibleNodeData() const {
  ui::AXNodeData data;

  // Views may misbehave if their widget is closed; return an unknown role
  // rather than possibly crashing.
  if (!view_->GetWidget() || view_->GetWidget()->IsClosed()) {
    data.role = ui::AX_ROLE_UNKNOWN;
    data.AddIntAttribute(ui::AX_ATTR_RESTRICTION, ui::AX_RESTRICTION_DISABLED);
    return data;
  }

  view_->GetAccessibleNodeData(&data);
  if (custom_data_.role != ui::AX_ROLE_UNKNOWN)
    data.role = custom_data_.role;
  if (!custom_data_.GetStringAttribute(ui::AX_ATTR_NAME).empty())
    data.SetName(custom_data_.GetStringAttribute(ui::AX_ATTR_NAME));

  data.location = gfx::RectF(view_->GetBoundsInScreen());
  if (!data.HasStringAttribute(ui::AX_ATTR_DESCRIPTION)) {
    base::string16 description;
    view_->GetTooltipText(gfx::Point(), &description);
    data.AddStringAttribute(ui::AX_ATTR_DESCRIPTION,
                            base::UTF16ToUTF8(description));
  }

  data.AddStringAttribute(ui::AX_ATTR_CLASS_NAME, view_->GetClassName());

  if (view_->IsAccessibilityFocusable())
    data.AddState(ui::AX_STATE_FOCUSABLE);

  if (!view_->enabled()) {
    data.AddIntAttribute(ui::AX_ATTR_RESTRICTION, ui::AX_RESTRICTION_DISABLED);
  }

  if (!view_->IsDrawn())
    data.AddState(ui::AX_STATE_INVISIBLE);

  if (view_->context_menu_controller())
    data.AddAction(ui::AX_ACTION_SHOW_CONTEXT_MENU);

  // Make sure this element is excluded from the a11y tree if there's a
  // focusable parent. All keyboard focusable elements should be leaf nodes.
  // Exceptions to this rule will themselves be accessibility focusable.
  if (IsViewUnfocusableChildOfFocusableAncestor(view_))
    data.role = ui::AX_ROLE_IGNORED;

  return data;
}

void ViewAccessibility::SetRole(ui::AXRole role) {
  custom_data_.role = role;
}

void ViewAccessibility::SetName(const std::string& name) {
  custom_data_.SetName(name);
}

gfx::NativeViewAccessible ViewAccessibility::GetNativeObject() {
  return nullptr;
}

}  // namespace views
