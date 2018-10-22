// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/test_client_controlled_state_delegate.h"

#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ash/wm/window_state.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace exo {
namespace test {

TestClientControlledStateDelegate::TestClientControlledStateDelegate() =
    default;
TestClientControlledStateDelegate::~TestClientControlledStateDelegate() =
    default;

void TestClientControlledStateDelegate::HandleWindowStateRequest(
    ash::wm::WindowState* window_state,
    ash::mojom::WindowStateType next_state) {
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(window_state->window());
  ClientControlledShellSurface* shell_surface =
      static_cast<ClientControlledShellSurface*>(widget->widget_delegate());
  switch (next_state) {
    case ash::mojom::WindowStateType::NORMAL:
    case ash::mojom::WindowStateType::DEFAULT:
      shell_surface->SetRestored();
      break;
    case ash::mojom::WindowStateType::MINIMIZED:
      shell_surface->SetMinimized();
      break;
    case ash::mojom::WindowStateType::MAXIMIZED:
      shell_surface->SetMaximized();
      break;
    case ash::mojom::WindowStateType::FULLSCREEN:
      shell_surface->SetFullscreen(true);
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
  shell_surface->OnSurfaceCommit();
}

void TestClientControlledStateDelegate::HandleBoundsRequest(
    ash::wm::WindowState* window_state,
    ash::mojom::WindowStateType requested_state,
    const gfx::Rect& bounds) {
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(window_state->window());
  ClientControlledShellSurface* shell_surface =
      static_cast<ClientControlledShellSurface*>(widget->widget_delegate());
  if (!shell_surface->host_window()->GetRootWindow())
    return;

  int64_t display_id = display::Screen::GetScreen()
                           ->GetDisplayNearestWindow(window_state->window())
                           .id();
  shell_surface->SetBounds(display_id, bounds);

  if (requested_state != window_state->GetStateType()) {
    DCHECK(requested_state == ash::mojom::WindowStateType::LEFT_SNAPPED ||
           requested_state == ash::mojom::WindowStateType::RIGHT_SNAPPED);
    if (requested_state == ash::mojom::WindowStateType::LEFT_SNAPPED)
      shell_surface->SetSnappedToLeft();
    else
      shell_surface->SetSnappedToRight();
  }

  shell_surface->OnSurfaceCommit();
}

// static
void TestClientControlledStateDelegate::InstallFactory() {
  ClientControlledShellSurface::SetClientControlledStateDelegateFactoryForTest(
      base::BindRepeating([]() {
        return base::WrapUnique<ash::wm::ClientControlledState::Delegate>(
            new TestClientControlledStateDelegate());
      }));
};

// static
void TestClientControlledStateDelegate::UninstallFactory() {
  ClientControlledShellSurface::SetClientControlledStateDelegateFactoryForTest(
      base::RepeatingCallback<
          std::unique_ptr<ash::wm::ClientControlledState::Delegate>(void)>());
}

}  // namespace test
}  // namespace exo
