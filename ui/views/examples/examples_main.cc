// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
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

#if defined(USE_AURA)
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/test/compositor_test_support.h"
#include "ui/views/widget/native_widget_aura.h"
#endif

#if defined(USE_AURA)
class RootWindowStackingClient : public aura::client::StackingClient {
 public:
  explicit RootWindowStackingClient() {
    aura::client::SetStackingClient(this);
  }

  virtual ~RootWindowStackingClient() {
    aura::client::SetStackingClient(NULL);
  }

  // Overridden from aura::client::StackingClient:
  virtual aura::Window* GetDefaultParent(aura::Window* window) OVERRIDE {
    return window->GetRootWindow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RootWindowStackingClient);
};
#endif

int main(int argc, char** argv) {
#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer;
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
#if defined(USE_AURA)

  // TURN ON THE HAX.
  views::NativeWidgetAura::set_aura_desktop_hax();

  ui::CompositorTestSupport::Initialize();

  {
  RootWindowStackingClient root_window_stacking_client;
#endif

  views::TestViewsDelegate delegate;

  views::examples::ShowExamplesWindow(views::examples::QUIT_ON_CLOSE);

  // xxx: Hax here because this kills event handling.
#if !defined(USE_AURA)
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->RunWithDispatcher(&accelerator_handler);
#else
  MessageLoopForUI::current()->Run();
#endif

#if defined(USE_AURA)
  }
  aura::Env::DeleteInstance();
  ui::CompositorTestSupport::Terminate();
#endif

  return 0;
}
