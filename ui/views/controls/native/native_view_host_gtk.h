// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_GTK_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_GTK_H_
#pragma once

#include <gtk/gtk.h>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/native/native_view_host_wrapper.h"

namespace views {

class View;
class NativeWidgetGtk;

// Note that the NativeViewHostGtk assumes ownership of the GtkWidget attached
// to it for the duration of its attachment. This is so the NativeViewHostGtk
// can safely reparent the GtkWidget in multiple passes without having to worry
// about the GtkWidget's refcnt falling to 0.
class NativeViewHostGtk : public NativeViewHostWrapper {
 public:
  explicit NativeViewHostGtk(NativeViewHost* host);
  virtual ~NativeViewHostGtk();

  // Overridden from NativeViewHostWrapper:
  virtual void NativeViewAttached() OVERRIDE;
  virtual void NativeViewDetaching(bool destroyed) OVERRIDE;
  virtual void AddedToWidget() OVERRIDE;
  virtual void RemovedFromWidget() OVERRIDE;
  virtual void InstallClip(int x, int y, int w, int h) OVERRIDE;
  virtual bool HasInstalledClip() OVERRIDE;
  virtual void UninstallClip() OVERRIDE;
  virtual void ShowWidget(int x, int y, int w, int h) OVERRIDE;
  virtual void HideWidget() OVERRIDE;
  virtual void SetFocus() OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;

 private:
  // Create and Destroy the GtkFixed that performs clipping on our hosted
  // GtkWidget. |needs_window| is true when a clip is installed and implies the
  // fixed is backed by a X Window which actually performs the clipping.
  // It's kind of retarded that Gtk/Cairo doesn't clip painting of child windows
  // regardless of whether or not there's an X Window. It's not that hard.
  void CreateFixed(bool needs_window);

  // Destroys the GtkFixed that performs clipping on our hosted GtkWidget.
  void DestroyFixed();

  NativeWidgetGtk* GetHostWidget() const;

  // Returns the descendant of fixed_ that has focus, or NULL if focus is not
  // on a descendant of fixed_.
  GtkWidget* GetFocusedDescendant();

  // Connects a new host widget.
  void AttachHostWidget();

  // Invoked from the 'destroy' signal.
  static void CallDestroy(GtkObject* object, NativeViewHostGtk* host);

  // Invoked from the 'focus-in-event' signal.
  static gboolean CallFocusIn(GtkWidget* gtk_widget,
                              GdkEventFocus* event,
                              NativeViewHostGtk* button);

  // Our associated NativeViewHost.
  NativeViewHost* host_;

  // Have we installed a region on the gfx::NativeView used to clip to only the
  // visible portion of the gfx::NativeView ?
  bool installed_clip_;

  // The installed clip rect. InstallClip doesn't actually perform the clipping,
  // a call to ShowWidget will.
  gfx::Rect installed_clip_bounds_;

  // Signal handle id for 'destroy' signal.
  gulong destroy_signal_id_;

  // Signal handle id for 'focus-in-event' signal.
  gulong focus_signal_id_;

  // The GtkFixed that contains the attached gfx::NativeView (used for
  // clipping).
  GtkWidget* fixed_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostGtk);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_GTK_H_
