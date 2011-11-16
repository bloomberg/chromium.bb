// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/desktop/desktop_views_delegate.h"
#include "ui/views/desktop/desktop_window_view.h"
#include "views/focus/accelerator_handler.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include <ole2.h>
#endif

#if defined(USE_WAYLAND)
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/wayland/wayland_display.h"
#include "ui/wayland/wayland_message_pump.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#elif defined(OS_LINUX)
#include <glib.h>
#include <glib-object.h>
#endif

int main(int argc, char** argv) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#elif defined(OS_LINUX)
  // Initializes gtk stuff.
  g_type_init();
#if defined(TOOLKIT_USES_GTK) && !defined(USE_WAYLAND)
  gtk_init(&argc, &argv);
#endif
#endif

  CommandLine::Init(argc, argv);

  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  icu_util::Initialize();

  ResourceBundle::InitSharedInstance("en-US");

#if defined(USE_WAYLAND)
  // Wayland uses EGL for drawing, so we need to initialize this as early as
  // possible.
  if (!gfx::GLSurface::InitializeOneOff()) {
    LOG(ERROR) << "Failed to initialize GLSurface";
    return -1;
  }
  ui::WaylandMessagePump wayland_message_pump(
      ui::WaylandDisplay::GetDisplay(gfx::GLSurfaceEGL::GetNativeDisplay()));
#endif
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  views::desktop::DesktopViewsDelegate views_delegate;

  // Desktop mode only supports a pure-views configuration.
  views::Widget::SetPureViews(true);

  views::desktop::DesktopWindowView::CreateDesktopWindow(
      views::desktop::DesktopWindowView::DESKTOP_DEFAULT);
  views::desktop::DesktopWindowView::desktop_window_view->CreateTestWindow(
      ASCIIToUTF16("Sample Window 1"), SK_ColorWHITE,
      gfx::Rect(500, 200, 400, 400), true);
  views::desktop::DesktopWindowView::desktop_window_view->CreateTestWindow(
      ASCIIToUTF16("Sample Window 2"), SK_ColorRED,
      gfx::Rect(600, 450, 450, 300), false);

  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->RunWithDispatcher(&accelerator_handler);

#if defined(OS_WIN)
  OleUninitialize();
#endif
  return 0;
}
