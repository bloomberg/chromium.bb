// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_window_obj_wrapper.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/widget/widget.h"

namespace views {

// A helper to fire an event on a window, taking into account its associated
// widget and that widget's root view.
void FireEvent(aura::Window* window, ax::mojom::Event event_type) {
  AXAuraObjCache::GetInstance()->FireEvent(
      AXAuraObjCache::GetInstance()->GetOrCreate(window), event_type);

  Widget* widget = Widget::GetWidgetForNativeView(window);
  if (widget) {
    AXAuraObjCache::GetInstance()->FireEvent(
        AXAuraObjCache::GetInstance()->GetOrCreate(widget), event_type);

    views::View* root_view = widget->GetRootView();
    if (root_view)
      root_view->NotifyAccessibilityEvent(event_type, true);
  }

  aura::Window::Windows children = window->children();
  for (size_t i = 0; i < children.size(); ++i)
    FireEvent(children[i], ax::mojom::Event::kLocationChanged);
}

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

bool AXWindowObjWrapper::IsIgnored() {
  return false;
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
  out_node_data->id = GetUniqueId();
  ax::mojom::Role role = window_->GetProperty(ui::kAXRoleOverride);
  if (role != ax::mojom::Role::kNone)
    out_node_data->role = role;
  else
    out_node_data->role =
        is_alert_ ? ax::mojom::Role::kAlert : ax::mojom::Role::kWindow;
  out_node_data->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                    base::UTF16ToUTF8(window_->GetTitle()));
  if (!window_->IsVisible())
    out_node_data->AddState(ax::mojom::State::kInvisible);

  out_node_data->relative_bounds.bounds =
      gfx::RectF(window_->GetBoundsInScreen());
  std::string* child_ax_tree_id_ptr = window_->GetProperty(ui::kChildAXTreeID);
  if (child_ax_tree_id_ptr && ui::AXTreeID::FromString(*child_ax_tree_id_ptr) !=
                                  ui::AXTreeIDUnknown()) {
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

    out_node_data->AddStringAttribute(ax::mojom::StringAttribute::kChildTreeId,
                                      *child_ax_tree_id_ptr);
  }
}

int32_t AXWindowObjWrapper::GetUniqueId() const {
  return unique_id_.Get();
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

  FireEvent(window_, ax::mojom::Event::kLocationChanged);
}

void AXWindowObjWrapper::OnWindowPropertyChanged(aura::Window* window,
                                                 const void* key,
                                                 intptr_t old) {
  if (window == window_ && key == ui::kChildAXTreeID) {
    AXAuraObjCache::GetInstance()->FireEvent(
        this, ax::mojom::Event::kChildrenChanged);
  }
}

void AXWindowObjWrapper::OnWindowVisibilityChanged(aura::Window* window,
                                                   bool visible) {
  AXAuraObjCache::GetInstance()->FireEvent(this,
                                           ax::mojom::Event::kStateChanged);
}

void AXWindowObjWrapper::OnWindowTransformed(aura::Window* window,
                                             ui::PropertyChangeReason reason) {
  if (window != window_)
    return;

  FireEvent(window_, ax::mojom::Event::kLocationChanged);
}

void AXWindowObjWrapper::OnWindowTitleChanged(aura::Window* window) {
  FireEvent(window, ax::mojom::Event::kTextChanged);
}

}  // namespace views
