// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/content_client/examples_browser_main_parts.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/aura/env.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/gfx/screen.h"
#include "ui/views/examples/examples_window_with_content.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/wm/core/wm_state.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/wm/test/wm_test_helper.h"
#endif

namespace views {
namespace examples {

ExamplesBrowserMainParts::ExamplesBrowserMainParts(
    const content::MainFunctionParams& parameters) {
}

ExamplesBrowserMainParts::~ExamplesBrowserMainParts() {
}

void ExamplesBrowserMainParts::ToolkitInitialized() {
  wm_state_.reset(new views::corewm::WMState);
}

void ExamplesBrowserMainParts::PreMainMessageLoopRun() {
  ui::InitializeInputMethodForTesting();
  browser_context_.reset(new content::ShellBrowserContext(false, NULL));

  gfx::NativeView window_context = NULL;
#if defined(OS_CHROMEOS)
  gfx::Screen::SetScreenInstance(
      gfx::SCREEN_TYPE_NATIVE, aura::TestScreen::Create());
  // Set up basic pieces of views::corewm.
  wm_test_helper_.reset(new wm::WMTestHelper(gfx::Size(800, 600)));
  // Ensure the X window gets mapped.
  wm_test_helper_->host()->Show();
  // Ensure Aura knows where to open new windows.
  window_context = wm_test_helper_->host()->window();
#else
  aura::Env::CreateInstance();
  gfx::Screen::SetScreenInstance(
      gfx::SCREEN_TYPE_NATIVE, CreateDesktopScreen());
#endif
  views_delegate_.reset(new DesktopTestViewsDelegate);

  ShowExamplesWindowWithContent(
      QUIT_ON_CLOSE, browser_context_.get(), window_context);
}

void ExamplesBrowserMainParts::PostMainMessageLoopRun() {
  browser_context_.reset();
#if defined(OS_CHROMEOS)
  wm_test_helper_.reset();
#endif
  views_delegate_.reset();
  aura::Env::DeleteInstance();
}

bool ExamplesBrowserMainParts::MainMessageLoopRun(int* result_code) {
  base::RunLoop run_loop;
  run_loop.Run();
  return true;
}

}  // namespace examples
}  // namespace views
