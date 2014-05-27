// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_content_client/views_content_client_main_parts.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/context_factory.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/aura/env.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/gfx/screen.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views_content_client/views_content_client.h"
#include "ui/wm/core/wm_state.h"

#if defined(OS_CHROMEOS)
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/wm/test/wm_test_helper.h"
#else  // !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

namespace ui {

ViewsContentClientMainParts::ViewsContentClientMainParts(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client)
    : views_content_client_(views_content_client) {
}

ViewsContentClientMainParts::~ViewsContentClientMainParts() {
}

void ViewsContentClientMainParts::ToolkitInitialized() {
  wm_state_.reset(new ::wm::WMState);
}

void ViewsContentClientMainParts::PreMainMessageLoopRun() {
  ui::InitializeInputMethodForTesting();
  browser_context_.reset(new content::ShellBrowserContext(false, NULL));

  gfx::NativeView window_context = NULL;
#if defined(OS_CHROMEOS)
  gfx::Screen::SetScreenInstance(
      gfx::SCREEN_TYPE_NATIVE, aura::TestScreen::Create());
  // Set up basic pieces of views::corewm.
  wm_test_helper_.reset(new ::wm::WMTestHelper(gfx::Size(800, 600),
                                               content::GetContextFactory()));
  // Ensure the X window gets mapped.
  wm_test_helper_->host()->Show();
  // Ensure Aura knows where to open new windows.
  window_context = wm_test_helper_->host()->window();
#else
  aura::Env::CreateInstance(true);
  gfx::Screen::SetScreenInstance(
      gfx::SCREEN_TYPE_NATIVE, views::CreateDesktopScreen());
#endif
  views_delegate_.reset(new views::DesktopTestViewsDelegate);

  views_content_client_->task().Run(browser_context_.get(), window_context);
}

void ViewsContentClientMainParts::PostMainMessageLoopRun() {
  browser_context_.reset();
#if defined(OS_CHROMEOS)
  wm_test_helper_.reset();
#endif
  views_delegate_.reset();
  aura::Env::DeleteInstance();
}

bool ViewsContentClientMainParts::MainMessageLoopRun(int* result_code) {
  base::RunLoop run_loop;
  run_loop.Run();
  return true;
}

}  // namespace ui
