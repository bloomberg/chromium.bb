// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_X_GTK_UI_PLATFORM_X11_H_
#define UI_GTK_X_GTK_UI_PLATFORM_X11_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gtk/gtk_ui_platform.h"

using GdkDisplay = struct _GdkDisplay;

namespace gtk {

class GtkEventLoopX11;

// GtkUiPlatform implementation for desktop Linux X11 backends.
class GtkUiPlatformX11 : public GtkUiPlatform {
 public:
  GtkUiPlatformX11();
  GtkUiPlatformX11(const GtkUiPlatformX11&) = delete;
  GtkUiPlatformX11& operator=(const GtkUiPlatformX11&) = delete;
  ~GtkUiPlatformX11() override;

  // GtkUiPlatform:
  void OnInitialized(GtkWidget* widget) override;
  GdkKeymap* GetGdkKeymap() override;
  GdkModifierType GetGdkKeyEventState(const ui::KeyEvent& key_event) override;
  int GetGdkKeyEventGroup(const ui::KeyEvent& key_event) override;
  GdkWindow* GetGdkWindow(gfx::AcceleratedWidget window_id) override;
  bool ExportWindowHandle(
      gfx::AcceleratedWidget window_id,
      base::OnceCallback<void(std::string)> callback) override;
  bool SetGtkWidgetTransientFor(GtkWidget* widget,
                                gfx::AcceleratedWidget parent) override;
  void ClearTransientFor(gfx::AcceleratedWidget parent) override;
  void ShowGtkWindow(GtkWindow* window) override;
  bool PreferGtkIme() override;

 private:
  GdkDisplay* GetGdkDisplay();

  x11::Connection* const connection_;
  GdkDisplay* display_ = nullptr;
  std::unique_ptr<GtkEventLoopX11> event_loop_;
};

}  // namespace gtk

#endif  // UI_GTK_X_GTK_UI_PLATFORM_X11_H_
