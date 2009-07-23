// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/native/native_view_host_gtk.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/widget_gtk.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostGtk, public:

NativeViewHostGtk::NativeViewHostGtk(NativeViewHost* host)
    : host_(host),
      installed_clip_(false),
      destroy_signal_id_(0),
      fixed_(NULL) {
  CreateFixed(false);
}

NativeViewHostGtk::~NativeViewHostGtk() {
  if (fixed_)
    gtk_widget_destroy(fixed_);
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostGtk, NativeViewHostWrapper implementation:

void NativeViewHostGtk::NativeViewAttached() {
  DCHECK(host_->native_view());
  if (gtk_widget_get_parent(host_->native_view()))
    gtk_widget_reparent(host_->native_view(), fixed_);
  else
    gtk_container_add(GTK_CONTAINER(fixed_), host_->native_view());

  if (!destroy_signal_id_) {
    destroy_signal_id_ = g_signal_connect(G_OBJECT(host_->native_view()),
                                          "destroy", G_CALLBACK(CallDestroy),
                                          this);
  }

  // Always layout though.
  host_->Layout();

  // TODO(port): figure out focus.
}

void NativeViewHostGtk::NativeViewDetaching() {
  DCHECK(host_->native_view());

  g_signal_handler_disconnect(G_OBJECT(host_->native_view()),
                              destroy_signal_id_);
  destroy_signal_id_ = 0;

  // TODO(port): focus.
  // FocusManager::UninstallFocusSubclass(native_view());
  installed_clip_ = false;
}

void NativeViewHostGtk::AddedToWidget() {
  if (gtk_widget_get_parent(fixed_))
    GetHostWidget()->ReparentChild(fixed_);
  else
    GetHostWidget()->AddChild(fixed_);

  if (!host_->native_view())
    return;

  if (gtk_widget_get_parent(host_->native_view()))
    gtk_widget_reparent(host_->native_view(), fixed_);
  else
    gtk_container_add(GTK_CONTAINER(fixed_), host_->native_view());

  if (host_->IsVisibleInRootView())
    gtk_widget_show(fixed_);
  else
    gtk_widget_hide(fixed_);
  host_->Layout();
}

void NativeViewHostGtk::RemovedFromWidget() {
  if (!host_->native_view())
    return;

  // TODO(beng): We leak host_->native_view() here. Fix: make all widgets not be
  //             refcounted.
  DestroyFixed();
}

void NativeViewHostGtk::InstallClip(int x, int y, int w, int h) {
  DCHECK(w > 0 && h > 0);
  installed_clip_bounds_.SetRect(x, y, w, h);
  installed_clip_ = true;

  // We only re-create the fixed with a window when a cliprect is installed.
  // Because the presence of a X Window will prevent transparency from working
  // properly, we only want it to be active for the duration of a clip
  // (typically during animations and scrolling.)
  CreateFixed(true);
}

bool NativeViewHostGtk::HasInstalledClip() {
  return installed_clip_;
}

void NativeViewHostGtk::UninstallClip() {
  installed_clip_ = false;
  // We now re-create the fixed without a X Window so transparency works again.
  CreateFixed(false);
}

void NativeViewHostGtk::ShowWidget(int x, int y, int w, int h) {
  // x and y are the desired position of host_ in WidgetGtk coordiantes.
  int fixed_x = x;
  int fixed_y = y;
  int fixed_w = w;
  int fixed_h = h;
  int child_x = 0;
  int child_y = 0;
  int child_w = w;
  int child_h = h;
  if (installed_clip_) {
    child_x = -installed_clip_bounds_.x();
    child_y = -installed_clip_bounds_.y();
    fixed_x += -child_x;
    fixed_y += -child_y;
    fixed_w = std::min(installed_clip_bounds_.width(), w);
    fixed_h = std::min(installed_clip_bounds_.height(), h);
  }

  // Size and place the fixed_.
  GetHostWidget()->PositionChild(fixed_, fixed_x, fixed_y, fixed_w, fixed_h);

  // Size and place the hosted NativeView.
  gtk_widget_set_size_request(host_->native_view(), child_w, child_h);
  gtk_fixed_move(GTK_FIXED(fixed_), host_->native_view(), child_x, child_y);

  gtk_widget_show(fixed_);
}

void NativeViewHostGtk::HideWidget() {
  gtk_widget_hide(fixed_);
}

void NativeViewHostGtk::SetFocus() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostGtk, private:

void NativeViewHostGtk::CreateFixed(bool needs_window) {
  bool native_view_addrefed = DestroyFixed();

  fixed_ = gtk_fixed_new();
  gtk_fixed_set_has_window(GTK_FIXED(fixed_), needs_window);
  // Defeat refcounting. We need to own the fixed.
  gtk_widget_ref(fixed_);

  WidgetGtk* widget_gtk = GetHostWidget();
  if (widget_gtk)
    widget_gtk->AddChild(fixed_);
  if (host_->native_view())
    gtk_container_add(GTK_CONTAINER(fixed_), host_->native_view());
  if (native_view_addrefed)
    gtk_widget_unref(host_->native_view());
}

bool NativeViewHostGtk::DestroyFixed() {
  bool native_view_addrefed = false;
  if (!fixed_)
    return native_view_addrefed;

  gtk_widget_hide(fixed_);
  GetHostWidget()->RemoveChild(fixed_);

  if (host_->native_view()) {
    // We can't allow the hosted NativeView's refcount to drop to zero.
    gtk_widget_ref(host_->native_view());
    native_view_addrefed = true;
    gtk_container_remove(GTK_CONTAINER(fixed_), host_->native_view());
  }

  gtk_widget_destroy(fixed_);
  fixed_ = NULL;
  return native_view_addrefed;
}

WidgetGtk* NativeViewHostGtk::GetHostWidget() const {
  return static_cast<WidgetGtk*>(host_->GetWidget());
}

// static
void NativeViewHostGtk::CallDestroy(GtkObject* object,
                                    NativeViewHostGtk* host) {
  return host->host_->NativeViewDestroyed();
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostWrapper, public:

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  return new NativeViewHostGtk(host);
}

}  // namespace views
