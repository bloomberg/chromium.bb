// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window.h"

#include "base/bind.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/display.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/display_manager.h"
#include "ui/ozone/platform/dri/dri_cursor.h"
#include "ui/ozone/platform/dri/dri_gpu_platform_support_host.h"
#include "ui/ozone/platform/dri/dri_window_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

DriWindow::DriWindow(PlatformWindowDelegate* delegate,
                     const gfx::Rect& bounds,
                     DriGpuPlatformSupportHost* sender,
                     EventFactoryEvdev* event_factory,
                     DriCursor* cursor,
                     DriWindowManager* window_manager,
                     DisplayManager* display_manager)
    : delegate_(delegate),
      sender_(sender),
      event_factory_(event_factory),
      cursor_(cursor),
      window_manager_(window_manager),
      display_manager_(display_manager),
      bounds_(bounds),
      widget_(window_manager->NextAcceleratedWidget()) {
  window_manager_->AddWindow(widget_, this);
}

DriWindow::~DriWindow() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  window_manager_->RemoveWindow(widget_);
  cursor_->OnWindowRemoved(widget_);

  sender_->RemoveChannelObserver(this);
  sender_->Send(new OzoneGpuMsg_DestroyWindowDelegate(widget_));
}

void DriWindow::Initialize() {
  sender_->AddChannelObserver(this);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  cursor_->OnWindowAdded(widget_, bounds_);
  delegate_->OnAcceleratedWidgetAvailable(widget_);
}

gfx::AcceleratedWidget DriWindow::GetAcceleratedWidget() {
  return widget_;
}

void DriWindow::Show() {}

void DriWindow::Hide() {}

void DriWindow::Close() {}

void DriWindow::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
  SendBoundsChange();
}

gfx::Rect DriWindow::GetBounds() {
  return bounds_;
}

void DriWindow::SetCapture() {
  window_manager_->GrabEvents(widget_);
}

void DriWindow::ReleaseCapture() {
  window_manager_->UngrabEvents(widget_);
}

void DriWindow::ToggleFullscreen() {}

void DriWindow::Maximize() {}

void DriWindow::Minimize() {}

void DriWindow::Restore() {}

void DriWindow::SetCursor(PlatformCursor cursor) {
  cursor_->SetCursor(widget_, cursor);
}

void DriWindow::MoveCursorTo(const gfx::Point& location) {
  event_factory_->WarpCursorTo(widget_, location);
}

bool DriWindow::CanDispatchEvent(const PlatformEvent& ne) {
  DCHECK(ne);
  Event* event = static_cast<Event*>(ne);

  // If there is a grab, capture events here.
  gfx::AcceleratedWidget grabber = window_manager_->event_grabber();
  if (grabber != gfx::kNullAcceleratedWidget)
    return grabber == widget_;

  if (event->IsLocatedEvent()) {
    LocatedEvent* located_event = static_cast<LocatedEvent*>(event);
    return bounds_.Contains(gfx::ToFlooredPoint(located_event->location()));
  }

  // TODO(spang): For non-ash builds we would need smarter keyboard focus.
  return true;
}

uint32_t DriWindow::DispatchEvent(const PlatformEvent& native_event) {
  DCHECK(native_event);

  Event* event = static_cast<Event*>(native_event);
  if (event->IsLocatedEvent()) {
    // Make the event location relative to this window's origin.
    LocatedEvent* located_event = static_cast<LocatedEvent*>(event);
    gfx::PointF location = located_event->location();
    location -= bounds_.OffsetFromOrigin();
    located_event->set_location(location);
    located_event->set_root_location(location);
  }
  DispatchEventFromNativeUiEvent(
      native_event, base::Bind(&PlatformWindowDelegate::DispatchEvent,
                               base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void DriWindow::OnChannelEstablished() {
  sender_->Send(new OzoneGpuMsg_CreateWindowDelegate(widget_));
  SendBoundsChange();
}

void DriWindow::OnChannelDestroyed() {
}

void DriWindow::SendBoundsChange() {
  cursor_->PrepareForBoundsChange(widget_);
  sender_->Send(new OzoneGpuMsg_WindowBoundsChanged(widget_, bounds_));
  cursor_->CommitBoundsChange(widget_, bounds_);
}

}  // namespace ui
