// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/app_paths.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/process_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "views/desktop/desktop_views_delegate.h"
#include "views/desktop/desktop_window.h"
#include "views/focus/accelerator_handler.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include <ole2.h>
#endif

int main(int argc, char** argv) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#elif defined(OS_LINUX)
  // Initializes gtk stuff.
  g_thread_init(NULL);
  g_type_init();
  gtk_init(&argc, &argv);
#endif

  CommandLine::Init(argc, argv);

  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  app::RegisterPathProvider();
  ui::RegisterPathProvider();

  icu_util::Initialize();

  ResourceBundle::InitSharedInstance("en-US");

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  views::desktop::DesktopViewsDelegate views_delegate;

  // Desktop mode only supports a pure-views configuration.
  views::Widget::SetPureViews(true);

  views::desktop::DesktopWindow::CreateDesktopWindow();

  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->Run(&accelerator_handler);

#if defined(OS_WIN)
  OleUninitialize();
#endif
  return 0;
}
