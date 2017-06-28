// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/window_event_filter.h"

#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

WindowEventFilter::WindowEventFilter(DesktopWindowTreeHost* window_tree_host)
    : window_tree_host_(window_tree_host), click_component_(HTNOWHERE) {}

WindowEventFilter::~WindowEventFilter() {}

void WindowEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_PRESSED)
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (!target->delegate())
    return;

  int previous_click_component = HTNOWHERE;
  int component = target->delegate()->GetNonClientComponent(event->location());
  if (event->IsLeftMouseButton()) {
    previous_click_component = click_component_;
    click_component_ = component;
  }

  if (component == HTCAPTION) {
    OnClickedCaption(event, previous_click_component);
  } else if (component == HTMAXBUTTON) {
    OnClickedMaximizeButton(event);
  } else {
    if (target->GetProperty(aura::client::kResizeBehaviorKey) &
        ui::mojom::kResizeBehaviorCanResize) {
      MaybeDispatchHostWindowDragMovement(component, event);
    }
  }
}

void WindowEventFilter::OnClickedCaption(ui::MouseEvent* event,
                                         int previous_click_component) {
  aura::Window* target = static_cast<aura::Window*>(event->target());

  if (event->IsMiddleMouseButton()) {
    LinuxUI::NonClientMiddleClickAction action =
        LinuxUI::MIDDLE_CLICK_ACTION_LOWER;
    LinuxUI* linux_ui = LinuxUI::instance();
    if (linux_ui)
      action = linux_ui->GetNonClientMiddleClickAction();

    switch (action) {
      case LinuxUI::MIDDLE_CLICK_ACTION_NONE:
        break;
      case LinuxUI::MIDDLE_CLICK_ACTION_LOWER:
        LowerWindow();
        break;
      case LinuxUI::MIDDLE_CLICK_ACTION_MINIMIZE:
        window_tree_host_->Minimize();
        break;
      case LinuxUI::MIDDLE_CLICK_ACTION_TOGGLE_MAXIMIZE:
        if (target->GetProperty(aura::client::kResizeBehaviorKey) &
            ui::mojom::kResizeBehaviorCanMaximize)
          ToggleMaximizedState();
        break;
    }

    event->SetHandled();
    return;
  }

  if (event->IsLeftMouseButton() && event->flags() & ui::EF_IS_DOUBLE_CLICK) {
    click_component_ = HTNOWHERE;
    if ((target->GetProperty(aura::client::kResizeBehaviorKey) &
         ui::mojom::kResizeBehaviorCanMaximize) &&
        previous_click_component == HTCAPTION) {
      // Our event is a double click in the caption area in a window that can be
      // maximized. We are responsible for dispatching this as a minimize/
      // maximize on X11 (Windows converts this to min/max events for us).
      ToggleMaximizedState();
      event->SetHandled();
      return;
    }
  }

  MaybeDispatchHostWindowDragMovement(HTCAPTION, event);
}

void WindowEventFilter::OnClickedMaximizeButton(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  views::Widget* widget = views::Widget::GetWidgetForNativeView(target);
  if (!widget)
    return;

  gfx::Rect display_work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(target).work_area();
  gfx::Rect bounds = widget->GetWindowBoundsInScreen();
  if (event->IsMiddleMouseButton()) {
    bounds.set_y(display_work_area.y());
    bounds.set_height(display_work_area.height());
    widget->SetBounds(bounds);
    event->StopPropagation();
  } else if (event->IsRightMouseButton()) {
    bounds.set_x(display_work_area.x());
    bounds.set_width(display_work_area.width());
    widget->SetBounds(bounds);
    event->StopPropagation();
  }
}

void WindowEventFilter::ToggleMaximizedState() {
  if (window_tree_host_->IsMaximized())
    window_tree_host_->Restore();
  else
    window_tree_host_->Maximize();
}

void WindowEventFilter::LowerWindow() {}

void WindowEventFilter::MaybeDispatchHostWindowDragMovement(
    int hittest,
    ui::MouseEvent* event) {}

}  // namespace views
