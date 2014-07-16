// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/window/platform_window_compat.h"

#include "ui/events/event.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/event_factory_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

PlatformWindowCompat::PlatformWindowCompat(PlatformWindowDelegate* delegate,
                                           const gfx::Rect& bounds)
    : delegate_(delegate), bounds_(bounds) {
  widget_ = SurfaceFactoryOzone::GetInstance()->GetAcceleratedWidget();
  delegate_->OnAcceleratedWidgetAvailable(widget_);
  ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

PlatformWindowCompat::~PlatformWindowCompat() {
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

bool PlatformWindowCompat::CanDispatchEvent(const ui::PlatformEvent& ne) {
  CHECK(ne);
  ui::Event* event = static_cast<ui::Event*>(ne);
  if (event->IsMouseEvent() || event->IsScrollEvent())
    return ui::CursorFactoryOzone::GetInstance()->GetCursorWindow() == widget_;

  return true;
}

uint32_t PlatformWindowCompat::DispatchEvent(const ui::PlatformEvent& ne) {
  ui::Event* event = static_cast<ui::Event*>(ne);
  delegate_->DispatchEvent(event);
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

gfx::Rect PlatformWindowCompat::GetBounds() {
  return bounds_;
}

void PlatformWindowCompat::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

void PlatformWindowCompat::Show() {
}

void PlatformWindowCompat::Hide() {
}

void PlatformWindowCompat::Close() {
}

void PlatformWindowCompat::SetCapture() {
}

void PlatformWindowCompat::ReleaseCapture() {
}

void PlatformWindowCompat::ToggleFullscreen() {
}

void PlatformWindowCompat::Maximize() {
}

void PlatformWindowCompat::Minimize() {
}

void PlatformWindowCompat::Restore() {
}

}  // namespace ui
