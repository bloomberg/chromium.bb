// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/native/native_view_host_views.h"

#include "base/logging.h"
#include "ui/gfx/canvas.h"
#include "ui/views/focus/focus_manager.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostViews, public:

NativeViewHostViews::NativeViewHostViews(NativeViewHost* host)
    : host_(host),
      installed_clip_(false) {
}

NativeViewHostViews::~NativeViewHostViews() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostViews, NativeViewHostWrapper implementation:
void NativeViewHostViews::NativeViewAttached() {
  host_->AddChildView(host_->views_view());
  host_->Layout();
}

void NativeViewHostViews::NativeViewDetaching(bool destroyed) {
  host_->RemoveChildView(host_->views_view());
}

void NativeViewHostViews::AddedToWidget() {
  // nothing to do
}

void NativeViewHostViews::RemovedFromWidget() {
  // nothing to do
}

void NativeViewHostViews::InstallClip(int x, int y, int w, int h) {
  NOTIMPLEMENTED();
}

bool NativeViewHostViews::HasInstalledClip() {
  return installed_clip_;
}

void NativeViewHostViews::UninstallClip() {
  installed_clip_ = false;
}

void NativeViewHostViews::ShowWidget(int x, int y, int w, int h) {
  // x, y are in the coordinate system of the root view, but we're
  // already properly positioned by virtue of being an actual views
  // child of the NativeHostView, so disregard the origin.
  // It is important to update the visibility first, so that when the bounds is
  // set, the contents get notified of the resize (because resizing a hidden
  // views may not actually resize the contents).
  host_->views_view()->SetVisible(true);
  host_->views_view()->SetBounds(0, 0, w, h);
}

void NativeViewHostViews::HideWidget() {
  host_->views_view()->SetVisible(false);
}

void NativeViewHostViews::SetFocus() {
  host_->views_view()->RequestFocus();
}

gfx::NativeViewAccessible NativeViewHostViews::GetNativeViewAccessible() {
  return NULL;
}

}  // namespace views
