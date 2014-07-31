// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/native_view_accessibility.h"

#include "ui/accessibility/ax_view_state.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

#if !defined(OS_WIN)
// static
NativeViewAccessibility* NativeViewAccessibility::Create(View* view) {
  DCHECK(view);
  NativeViewAccessibility* instance = new NativeViewAccessibility();
  instance->set_view(view);
  return instance;
}
#endif

NativeViewAccessibility::NativeViewAccessibility()
    : view_(NULL), ax_node_(ui::AXPlatformNode::Create(this)) {
}

NativeViewAccessibility::~NativeViewAccessibility() {
  if (ax_node_)
    ax_node_->Destroy();
}

gfx::NativeViewAccessible NativeViewAccessibility::GetNativeObject() {
  return ax_node_ ? ax_node_->GetNativeViewAccessible() : NULL;
}

void NativeViewAccessibility::Destroy() {
  delete this;
}

#if !defined(OS_WIN)
// static
void NativeViewAccessibility::RegisterWebView(View* web_view) {
}

// static
void NativeViewAccessibility::UnregisterWebView(View* web_view) {
}
#endif

// ui::AXPlatformNodeDelegate

ui::AXNodeData* NativeViewAccessibility::GetData() {
  ui::AXViewState state;
  view_->GetAccessibleState(&state);
  data_.role = state.role;
  data_.location = view_->GetBoundsInScreen();
  return &data_;
}

int NativeViewAccessibility::GetChildCount() {
  return view_->child_count();
}

gfx::NativeViewAccessible NativeViewAccessibility::ChildAtIndex(int index) {
  if (index < 0 || index >= view_->child_count())
    return NULL;
  return view_->child_at(index)->GetNativeViewAccessible();
}

gfx::NativeViewAccessible NativeViewAccessibility::GetParent() {
  if (view_->parent())
    return view_->parent()->GetNativeViewAccessible();

#if defined(OS_MACOSX)
  if (view_->GetWidget())
    return view_->GetWidget()->GetNativeView();
#endif

  return NULL;
}

gfx::Vector2d NativeViewAccessibility::GetGlobalCoordinateOffset() {
  return gfx::Vector2d(0, 0);  // location is already in screen coordinates.
}

void NativeViewAccessibility::NotifyAccessibilityEvent(ui::AXEvent event_type) {
}

}  // namespace views
