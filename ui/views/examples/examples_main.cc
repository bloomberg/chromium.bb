// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

int main(int argc, char** argv) {
#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer;
#elif defined(OS_LINUX)
  // Initializes gtk stuff.
  g_type_init();
  gtk_init(&argc, &argv);
#endif
  CommandLine::Init(argc, argv);

  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::LOCK_LOG_FILE,
                       logging::DELETE_OLD_LOG_FILE,
                       logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  bool icu_result = icu_util::Initialize();
  CHECK(icu_result);
  ui::ResourceBundle::InitSharedInstanceWithLocale("en-US");

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);

  views::TestViewsDelegate delegate;

  views::examples::ShowExamplesWindow(true);

  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->RunWithDispatcher(&accelerator_handler);

  return 0;
}
