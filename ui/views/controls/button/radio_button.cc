// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/radio_button.h"

#include "base/logging.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
const char RadioButton::kViewClassName[] = "RadioButton";

RadioButton::RadioButton(const base::string16& label, int group_id)
    : Checkbox(label, nullptr) {
  SetGroup(group_id);
}

RadioButton::~RadioButton() = default;

const char* RadioButton::GetClassName() const {
  return kViewClassName;
}

void RadioButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Checkbox::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kRadioButton;
}

View* RadioButton::GetSelectedViewForGroup(int group) {
  Views views;
  GetViewsInGroupFromParent(group, &views);
  if (views.empty())
    return nullptr;

  for (Views::const_iterator i(views.begin()); i != views.end(); ++i) {
    // REVIEW: why don't we check the runtime type like is done above?
    RadioButton* radio_button = static_cast<RadioButton*>(*i);
    if (radio_button->checked())
      return radio_button;
  }
  return nullptr;
}

bool RadioButton::IsGroupFocusTraversable() const {
  // When focusing a radio button with tab/shift+tab, only the selected button
  // from the group should be focused.
  return false;
}

void RadioButton::OnFocus() {
  Checkbox::OnFocus();
  SetChecked(true);
}

void RadioButton::RequestFocusFromEvent() {
  Checkbox::RequestFocusFromEvent();
  // Take focus only if another radio button in the group has focus.
  Views views;
  GetViewsInGroupFromParent(GetGroup(), &views);
  if (std::find_if(views.begin(), views.end(), [](View* v) -> bool {
        return v->HasFocus();
      }) != views.end()) {
    RequestFocus();
  }
}

void RadioButton::NotifyClick(const ui::Event& event) {
  // Set the checked state to true only if we are unchecked, since we can't
  // be toggled on and off like a checkbox.
  if (!checked())
    SetChecked(true);
  LabelButton::NotifyClick(event);
}

ui::NativeTheme::Part RadioButton::GetThemePart() const {
  return ui::NativeTheme::kRadio;
}

void RadioButton::SetChecked(bool checked) {
  if (checked == RadioButton::checked())
    return;
  if (checked) {
    // We can't start from the root view because other parts of the UI might use
    // radio buttons with the same group. This can happen when re-using the same
    // component or even if views want to use the group for a different purpose.
    Views other;
    GetViewsInGroupFromParent(GetGroup(), &other);
    for (auto i(other.begin()); i != other.end(); ++i) {
      if (*i != this) {
        if (strcmp((*i)->GetClassName(), kViewClassName)) {
          NOTREACHED() << "radio-button-nt has same group as other non "
                          "radio-button-nt views.";
          continue;
        }
        RadioButton* peer = static_cast<RadioButton*>(*i);
        peer->SetChecked(false);
      }
    }
  }
  Checkbox::SetChecked(checked);
}

const gfx::VectorIcon& RadioButton::GetVectorIcon() const {
  return checked() ? kRadioButtonActiveIcon : kRadioButtonNormalIcon;
}

SkPath RadioButton::GetFocusRingPath() const {
  SkPath path;
  path.addOval(gfx::RectToSkRect(image()->GetMirroredBounds()));
  return path;
}

void RadioButton::GetViewsInGroupFromParent(int group, Views* views) {
  if (!parent())
    return;
  parent()->GetViewsInGroup(group, views);
}

}  // namespace views
