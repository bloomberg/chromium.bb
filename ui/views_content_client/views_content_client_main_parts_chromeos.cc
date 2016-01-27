// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "content/public/browser/context_factory.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"
#include "ui/views_content_client/views_content_client.h"
#include "ui/views_content_client/views_content_client_main_parts_aura.h"
#include "ui/wm/core/nested_accelerator_controller.h"
#include "ui/wm/core/nested_accelerator_delegate.h"
#include "ui/wm/test/wm_test_helper.h"

namespace ui {

namespace {

// A dummy version of the delegate usually provided by the Ash Shell.
class NestedAcceleratorDelegate : public ::wm::NestedAcceleratorDelegate {
 public:
  NestedAcceleratorDelegate() {}
  ~NestedAcceleratorDelegate() override {}

  // ::wm::NestedAcceleratorDelegate:
  Result ProcessAccelerator(const ui::Accelerator& accelerator) override {
    return RESULT_NOT_PROCESSED;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NestedAcceleratorDelegate);
};

class ViewsContentClientMainPartsChromeOS
    : public ViewsContentClientMainPartsAura {
 public:
  ViewsContentClientMainPartsChromeOS(
      const content::MainFunctionParams& content_params,
      ViewsContentClient* views_content_client);
  ~ViewsContentClientMainPartsChromeOS() override {}

  // content::BrowserMainParts:
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

 private:
  // Enable a minimal set of views::corewm to be initialized.
  scoped_ptr<gfx::Screen> test_screen_;
  scoped_ptr< ::wm::WMTestHelper> wm_test_helper_;
  scoped_ptr< ::wm::NestedAcceleratorController> nested_accelerator_controller_;

  DISALLOW_COPY_AND_ASSIGN(ViewsContentClientMainPartsChromeOS);
};

ViewsContentClientMainPartsChromeOS::ViewsContentClientMainPartsChromeOS(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client)
    : ViewsContentClientMainPartsAura(content_params, views_content_client) {
}

void ViewsContentClientMainPartsChromeOS::PreMainMessageLoopRun() {
  ViewsContentClientMainPartsAura::PreMainMessageLoopRun();

  gfx::Size host_size(800, 600);
  test_screen_.reset(aura::TestScreen::Create(host_size));
  gfx::Screen::SetScreenInstance(test_screen_.get());
  // Set up basic pieces of views::corewm.
  wm_test_helper_.reset(
      new ::wm::WMTestHelper(host_size, content::GetContextFactory()));
  // Ensure the X window gets mapped.
  wm_test_helper_->host()->Show();

  // Ensure Aura knows where to open new windows.
  aura::Window* root_window = wm_test_helper_->host()->window();
  views_content_client()->task().Run(browser_context(), root_window);

  nested_accelerator_controller_.reset(
      new ::wm::NestedAcceleratorController(new NestedAcceleratorDelegate));
  aura::client::SetDispatcherClient(root_window,
                                    nested_accelerator_controller_.get());
}

void ViewsContentClientMainPartsChromeOS::PostMainMessageLoopRun() {
  aura::client::SetDispatcherClient(wm_test_helper_->host()->window(), NULL);
  nested_accelerator_controller_.reset();
  wm_test_helper_.reset();
  test_screen_.reset();

  ViewsContentClientMainPartsAura::PostMainMessageLoopRun();
}

}  // namespace

// static
ViewsContentClientMainParts* ViewsContentClientMainParts::Create(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client) {
  return new ViewsContentClientMainPartsChromeOS(
      content_params, views_content_client);
}

}  // namespace ui
