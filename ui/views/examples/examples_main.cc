// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"

int main(int argc, char** argv) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#elif defined(OS_LINUX)
  // Initializes gtk stuff.
  g_type_init();
  gtk_init(&argc, &argv);
#endif
  CommandLine::Init(argc, argv);

  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  bool icu_result = icu_util::Initialize();
  CHECK(icu_result);
  ui::ResourceBundle::InitSharedInstance("en-US");

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  views::TestViewsDelegate delegate;

  // We do not use this header: chrome/common/chrome_switches.h
  // because that would create a bad dependency back on Chrome.
  views::Widget::SetPureViews(
      CommandLine::ForCurrentProcess()->HasSwitch("use-pure-views"));

  views::examples::ShowExamplesWindow(true);

  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->RunWithDispatcher(&accelerator_handler);

#if defined(OS_WIN)
  OleUninitialize();
#endif
  return 0;
}
