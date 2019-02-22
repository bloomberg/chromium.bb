// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/demo_window.h"

#include "base/threading/thread_task_runner_handle.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/ozone/demo/renderer.h"
#include "ui/ozone/demo/renderer_factory.h"
#include "ui/ozone/demo/window_manager.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if defined(OS_FUCHSIA)
#include <fuchsia/ui/policy/cpp/fidl.h>
#include "base/fuchsia/component_context.h"
#endif

namespace ui {

DemoWindow::DemoWindow(WindowManager* window_manager,
                       RendererFactory* renderer_factory,
                       const gfx::Rect& bounds)
    : window_manager_(window_manager),
      renderer_factory_(renderer_factory),
      weak_ptr_factory_(this) {
  PlatformWindowInitProperties properties;
  properties.bounds = bounds;

#if defined(OS_FUCHSIA)
  // When using Scenic Ozone platform we need to supply a ViewOwner request to
  // the window. This is not necessary when using the headless ozone platform.
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformProperties()
          .needs_view_owner_request) {
    // Initialize view_owner_request for the new instance.
    fidl::InterfaceHandle<fuchsia::ui::viewsv1token::ViewOwner> view_owner;
    properties.view_owner_request = view_owner.NewRequest();

    // Request Presenter to show the view full-screen.
    auto presenter = base::fuchsia::ComponentContext::GetDefault()
                         ->ConnectToService<fuchsia::ui::policy::Presenter>();
    presenter->Present(std::move(view_owner), nullptr);
  }
#endif

  platform_window_ = OzonePlatform::GetInstance()->CreatePlatformWindow(
      this, std::move(properties));
  platform_window_->Show();
}

DemoWindow::~DemoWindow() {}

gfx::AcceleratedWidget DemoWindow::GetAcceleratedWidget() {
  // TODO(spang): We should start rendering asynchronously.
  DCHECK_NE(widget_, gfx::kNullAcceleratedWidget)
      << "Widget not available synchronously";
  return widget_;
}

gfx::Size DemoWindow::GetSize() {
  return platform_window_->GetBounds().size();
}

void DemoWindow::Start() {
  StartRendererIfNecessary();
}

void DemoWindow::Quit() {
  window_manager_->Quit();
}

void DemoWindow::OnBoundsChanged(const gfx::Rect& new_bounds) {
  StartRendererIfNecessary();
}

void DemoWindow::OnDamageRect(const gfx::Rect& damaged_region) {}

void DemoWindow::DispatchEvent(Event* event) {
  if (event->IsKeyEvent() && event->AsKeyEvent()->code() == DomCode::US_Q)
    Quit();
}

void DemoWindow::OnCloseRequest() {
  Quit();
}

void DemoWindow::OnClosed() {}

void DemoWindow::OnWindowStateChanged(PlatformWindowState new_state) {}

void DemoWindow::OnLostCapture() {}

void DemoWindow::OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  widget_ = widget;
}

void DemoWindow::OnAcceleratedWidgetDestroyed() {
  widget_ = gfx::kNullAcceleratedWidget;
}

void DemoWindow::OnActivationChanged(bool active) {}

void DemoWindow::StartRendererIfNecessary() {
  if (renderer_ || GetSize().IsEmpty())
    return;
  renderer_ =
      renderer_factory_->CreateRenderer(GetAcceleratedWidget(), GetSize());
  if (!renderer_->Initialize())
    LOG(ERROR) << "Failed to initialize renderer.";
}

}  // namespace ui
