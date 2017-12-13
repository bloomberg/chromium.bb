// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_window_obj_wrapper.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/widget/widget.h"

namespace views {

AXWindowObjWrapper::AXWindowObjWrapper(aura::Window* window)
    : window_(window),
      is_alert_(false),
      is_root_window_(window->IsRootWindow()) {
  window->AddObserver(this);

  if (is_root_window_)
    AXAuraObjCache::GetInstance()->OnRootWindowObjCreated(window);
}

AXWindowObjWrapper::~AXWindowObjWrapper() {
  if (is_root_window_)
    AXAuraObjCache::GetInstance()->OnRootWindowObjDestroyed(window_);

  window_->RemoveObserver(this);
  window_ = NULL;
}

AXAuraObjWrapper* AXWindowObjWrapper::GetParent() {
  if (!window_->parent())
    return NULL;

  return AXAuraObjCache::GetInstance()->GetOrCreate(window_->parent());
}

void AXWindowObjWrapper::GetChildren(
    std::vector<AXAuraObjWrapper*>* out_children) {
  aura::Window::Windows children = window_->children();
  for (size_t i = 0; i < children.size(); ++i) {
    out_children->push_back(
        AXAuraObjCache::GetInstance()->GetOrCreate(children[i]));
  }

  // Also consider any associated widgets as children.
  Widget* widget = Widget::GetWidgetForNativeView(window_);
  if (widget && widget->IsVisible())
    out_children->push_back(AXAuraObjCache::GetInstance()->GetOrCreate(widget));
}

void AXWindowObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->id = GetID();
  ui::AXRole role = window_->GetProperty(ui::kAXRoleOverride);
  if (role != ui::AX_ROLE_NONE)
    out_node_data->role = role;
  else
    out_node_data->role = is_alert_ ? ui::AX_ROLE_ALERT : ui::AX_ROLE_WINDOW;
  out_node_data->AddStringAttribute(ui::AX_ATTR_NAME,
                                    base::UTF16ToUTF8(window_->GetTitle()));
  if (!window_->IsVisible())
    out_node_data->AddState(ui::AX_STATE_INVISIBLE);

  out_node_data->location = gfx::RectF(window_->bounds());
  if (window_->parent()) {
    out_node_data->offset_container_id =
        AXAuraObjCache::GetInstance()->GetID(window_->parent());
  }

  ui::AXTreeIDRegistry::AXTreeID child_ax_tree_id =
      window_->GetProperty(ui::kChildAXTreeID);
  if (child_ax_tree_id != ui::AXTreeIDRegistry::kNoAXTreeID) {
    // Most often, child AX trees are parented to Views. We need to handle
    // the case where they're not here, but we don't want the same AX tree
    // to be a child of two different parents.
    //
    // To avoid this double-parenting, only add the child tree ID of this
    // window if the top-level window doesn't have an associated Widget.
    if (!window_->GetToplevelWindow() ||
        Widget::GetWidgetForNativeView(window_->GetToplevelWindow())) {
      return;
    }

    out_node_data->AddIntAttribute(ui::AX_ATTR_CHILD_TREE_ID, child_ax_tree_id);
  }
}

int32_t AXWindowObjWrapper::GetID() {
  return AXAuraObjCache::GetInstance()->GetID(window_);
}

void AXWindowObjWrapper::OnWindowDestroyed(aura::Window* window) {
  AXAuraObjCache::GetInstance()->Remove(window, nullptr);
}

void AXWindowObjWrapper::OnWindowDestroying(aura::Window* window) {
  Widget* widget = Widget::GetWidgetForNativeView(window);
  if (widget)
    AXAuraObjCache::GetInstance()->Remove(widget);
}

void AXWindowObjWrapper::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.phase == WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED)
    AXAuraObjCache::GetInstance()->Remove(params.target, params.old_parent);
}

void AXWindowObjWrapper::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (window != window_)
    return;

  AXAuraObjCache::GetInstance()->FireEvent(this, ui::AX_EVENT_LOCATION_CHANGED);

  Widget* widget = Widget::GetWidgetForNativeView(window);
  if (widget) {
    AXAuraObjCache::GetInstance()->FireEvent(
        AXAuraObjCache::GetInstance()->GetOrCreate(widget),
        ui::AX_EVENT_LOCATION_CHANGED);

    views::View* root_view = widget->GetRootView();
    if (root_view)
      root_view->NotifyAccessibilityEvent(ui::AX_EVENT_LOCATION_CHANGED, true);
  }
}

void AXWindowObjWrapper::OnWindowPropertyChanged(aura::Window* window,
                                                 const void* key,
                                                 intptr_t old) {
  if (window == window_ && key == ui::kChildAXTreeID) {
    AXAuraObjCache::GetInstance()->FireEvent(this,
                                             ui::AX_EVENT_CHILDREN_CHANGED);
  }
}

}  // namespace views
