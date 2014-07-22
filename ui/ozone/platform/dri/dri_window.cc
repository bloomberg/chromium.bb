// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window.h"

#include "ui/events/event.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

DriWindow::DriWindow(PlatformWindowDelegate* delegate,
                     const gfx::Rect& bounds,
                     DriSurfaceFactory* surface_factory)
    : delegate_(delegate), bounds_(bounds) {
  widget_ = surface_factory->GetAcceleratedWidget();
  delegate_->OnAcceleratedWidgetAvailable(widget_);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

DriWindow::~DriWindow() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void DriWindow::Show() {}

void DriWindow::Hide() {}

void DriWindow::Close() {}

void DriWindow::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

gfx::Rect DriWindow::GetBounds() {
  return bounds_;
}

void DriWindow::SetCapture() {}

void DriWindow::ReleaseCapture() {}

void DriWindow::ToggleFullscreen() {}

void DriWindow::Maximize() {}

void DriWindow::Minimize() {}

void DriWindow::Restore() {}

bool DriWindow::CanDispatchEvent(const PlatformEvent& ne) {
  CHECK(ne);
  Event* event = static_cast<Event*>(ne);
  if (event->IsMouseEvent() || event->IsScrollEvent())
    return ui::CursorFactoryOzone::GetInstance()->GetCursorWindow() == widget_;

  return true;
}

uint32_t DriWindow::DispatchEvent(const PlatformEvent& ne) {
  Event* event = static_cast<Event*>(ne);
  delegate_->DispatchEvent(event);
  return POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui
