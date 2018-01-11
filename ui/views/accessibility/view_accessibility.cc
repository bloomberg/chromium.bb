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

bool IsValidRoleForViews(ui::AXRole role) {
  switch (role) {
    // These roles all have special meaning and shouldn't ever be
    // set on a View.
    case ui::AX_ROLE_DESKTOP:
    case ui::AX_ROLE_NONE:
    case ui::AX_ROLE_ROOT_WEB_AREA:
    case ui::AX_ROLE_SVG_ROOT:
    case ui::AX_ROLE_UNKNOWN:
    case ui::AX_ROLE_WEB_AREA:
      return false;

    default:
      return true;
  }
}

}  // namespace

#if !BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY()
// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  return base::WrapUnique(new ViewAccessibility(view));
}
#endif

ViewAccessibility::ViewAccessibility(View* view) : owner_view_(view) {}

ViewAccessibility::~ViewAccessibility() {}

const ui::AXUniqueId& ViewAccessibility::GetUniqueId() const {
  return unique_id_;
}

void ViewAccessibility::GetAccessibleNodeData(ui::AXNodeData* data) const {
  // Views may misbehave if their widget is closed; return an unknown role
  // rather than possibly crashing.
  if (!owner_view_->GetWidget() || owner_view_->GetWidget()->IsClosed()) {
    data->role = ui::AX_ROLE_UNKNOWN;
    data->AddIntAttribute(ui::AX_ATTR_RESTRICTION, ui::AX_RESTRICTION_DISABLED);
    return;
  }

  owner_view_->GetAccessibleNodeData(data);
  if (custom_data_.role != ui::AX_ROLE_UNKNOWN)
    data->role = custom_data_.role;
  if (custom_data_.HasStringAttribute(ui::AX_ATTR_NAME))
    data->SetName(custom_data_.GetStringAttribute(ui::AX_ATTR_NAME));

  data->location = gfx::RectF(owner_view_->GetBoundsInScreen());
  if (!data->HasStringAttribute(ui::AX_ATTR_DESCRIPTION)) {
    base::string16 description;
    owner_view_->GetTooltipText(gfx::Point(), &description);
    data->AddStringAttribute(ui::AX_ATTR_DESCRIPTION,
                             base::UTF16ToUTF8(description));
  }

  data->AddStringAttribute(ui::AX_ATTR_CLASS_NAME, owner_view_->GetClassName());

  if (owner_view_->IsAccessibilityFocusable())
    data->AddState(ui::AX_STATE_FOCUSABLE);

  if (!owner_view_->enabled())
    data->AddIntAttribute(ui::AX_ATTR_RESTRICTION, ui::AX_RESTRICTION_DISABLED);

  if (!owner_view_->visible())
    data->AddState(ui::AX_STATE_INVISIBLE);

  if (owner_view_->context_menu_controller())
    data->AddAction(ui::AX_ACTION_SHOW_CONTEXT_MENU);
}

void ViewAccessibility::OverrideRole(ui::AXRole role) {
  DCHECK(IsValidRoleForViews(role));

  custom_data_.role = role;
}

void ViewAccessibility::OverrideName(const std::string& name) {
  custom_data_.SetName(name);
}

gfx::NativeViewAccessible ViewAccessibility::GetNativeObject() {
  return nullptr;
}

}  // namespace views
