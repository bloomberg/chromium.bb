// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/platform_window_cast.h"

#include "base/bind.h"
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

PlatformWindowCast::PlatformWindowCast(PlatformWindowDelegate* delegate,
                                       const gfx::Rect& bounds)
    : delegate_(delegate), bounds_(bounds) {
  widget_ = (bounds.width() << 16) + bounds.height();
  delegate_->OnAcceleratedWidgetAvailable(widget_, 1.f);

  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

PlatformWindowCast::~PlatformWindowCast() {
  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

gfx::Rect PlatformWindowCast::GetBounds() {
  return bounds_;
}

void PlatformWindowCast::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

void PlatformWindowCast::SetTitle(const base::string16& title) {
}

bool PlatformWindowCast::HasCapture() const {
  return false;
}

PlatformImeController* PlatformWindowCast::GetPlatformImeController() {
  return nullptr;
}

bool PlatformWindowCast::CanDispatchEvent(const PlatformEvent& ne) {
  return true;
}

uint32_t PlatformWindowCast::DispatchEvent(const PlatformEvent& native_event) {
  DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate_)));

  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui
