// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_data_aura.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "content/shell/browser/shell.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/default_activation_client.h"

#if defined(OS_FUCHSIA)
#include <fuchsia/ui/policy/cpp/fidl.h>
#include <lib/zx/eventpair.h>
#include "base/fuchsia/component_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#endif

#if defined(USE_OZONE)
#include "ui/aura/screen_ozone.h"
#endif

namespace content {

namespace {

class FillLayout : public aura::LayoutManager {
 public:
  explicit FillLayout(aura::Window* root)
      : root_(root), has_bounds_(!root->bounds().IsEmpty()) {}

  ~FillLayout() override {}

 private:
  // aura::LayoutManager:
  void OnWindowResized() override {
    // If window bounds were not set previously then resize all children to
    // match the size of the parent.
    if (!has_bounds_) {
      has_bounds_ = true;
      for (aura::Window* child : root_->children())
        SetChildBoundsDirect(child, gfx::Rect(root_->bounds().size()));
    }
  }

  void OnWindowAddedToLayout(aura::Window* child) override {
    child->SetBounds(root_->bounds());
  }

  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}

  void OnWindowRemovedFromLayout(aura::Window* child) override {}

  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}

  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* root_;
  bool has_bounds_;

  DISALLOW_COPY_AND_ASSIGN(FillLayout);
};

}

ShellPlatformDataAura* Shell::platform_ = nullptr;

ShellPlatformDataAura::ShellPlatformDataAura(const gfx::Size& initial_size) {
  CHECK(aura::Env::GetInstance());

  // Setup global display::Screen singleton.
  if (!display::Screen::GetScreen()) {
#if defined(USE_OZONE)
    auto platform_screen = ui::OzonePlatform::GetInstance()->CreateScreen();
    if (platform_screen)
      screen_ = std::make_unique<aura::ScreenOzone>(std::move(platform_screen));
#endif  // defined(USE_OZONE)

    // Use aura::TestScreen for Ozone platforms that don't provide
    // PlatformScreen.
    // TODO(https://crbug.com/872339): Implement PlatformScreen for all
    // platforms and remove this code.
    if (!screen_) {
      // Some layout tests expect to be able to resize the window, so the screen
      // must be larger than the window.
      screen_.reset(
          aura::TestScreen::Create(gfx::ScaleToCeiledSize(initial_size, 2.0)));
    }
    display::Screen::SetScreenInstance(screen_.get());
  }

  ui::PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(initial_size);

#if defined(OS_FUCHSIA)
  // When using Scenic Ozone platform we need to supply a view_token to the
  // window. This is not necessary when using the headless ozone platform.
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformProperties()
          .needs_view_token) {
    // Create view_token and view_holder_token.
    zx::eventpair view_holder_token;
    zx_status_t status = zx::eventpair::create(
        /*options=*/0, &properties.view_token, &view_holder_token);
    ZX_CHECK(status == ZX_OK, status) << "zx_eventpair_create";

    // Request Presenter to show the view full-screen.
    auto presenter = base::fuchsia::ComponentContext::GetDefault()
                         ->ConnectToService<fuchsia::ui::policy::Presenter>();

    presenter->Present2(std::move(view_holder_token), nullptr);
  }
#endif

  host_ = aura::WindowTreeHost::Create(std::move(properties));
  host_->InitHost();
  host_->window()->Show();
  host_->window()->SetLayoutManager(new FillLayout(host_->window()));

  focus_client_.reset(new aura::test::TestFocusClient());
  aura::client::SetFocusClient(host_->window(), focus_client_.get());

  new wm::DefaultActivationClient(host_->window());
  capture_client_.reset(
      new aura::client::DefaultCaptureClient(host_->window()));
  window_parenting_client_.reset(
      new aura::test::TestWindowParentingClient(host_->window()));
}

ShellPlatformDataAura::~ShellPlatformDataAura() {
  if (screen_)
    display::Screen::SetScreenInstance(nullptr);
}

void ShellPlatformDataAura::ShowWindow() {
  host_->Show();
}

void ShellPlatformDataAura::ResizeWindow(const gfx::Size& size) {
  host_->SetBoundsInPixels(gfx::Rect(size));
}

}  // namespace content
