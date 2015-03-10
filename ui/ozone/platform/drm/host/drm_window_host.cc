// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_window_host.h"

#include "base/bind.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/display.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/host/display_manager.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

DrmWindowHost::DrmWindowHost(PlatformWindowDelegate* delegate,
                             const gfx::Rect& bounds,
                             DrmGpuPlatformSupportHost* sender,
                             EventFactoryEvdev* event_factory,
                             DrmCursor* cursor,
                             DrmWindowHostManager* window_manager,
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

DrmWindowHost::~DrmWindowHost() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  window_manager_->RemoveWindow(widget_);
  cursor_->OnWindowRemoved(widget_);

  sender_->RemoveChannelObserver(this);
  sender_->Send(new OzoneGpuMsg_DestroyWindowDelegate(widget_));
}

void DrmWindowHost::Initialize() {
  sender_->AddChannelObserver(this);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  cursor_->OnWindowAdded(widget_, bounds_, GetCursorConfinedBounds());
  delegate_->OnAcceleratedWidgetAvailable(widget_);
}

gfx::AcceleratedWidget DrmWindowHost::GetAcceleratedWidget() {
  return widget_;
}

gfx::Rect DrmWindowHost::GetCursorConfinedBounds() const {
  return cursor_confined_bounds_.IsEmpty() ? gfx::Rect(bounds_.size())
                                           : cursor_confined_bounds_;
}

void DrmWindowHost::Show() {
}

void DrmWindowHost::Hide() {
}

void DrmWindowHost::Close() {
}

void DrmWindowHost::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
  SendBoundsChange();
}

gfx::Rect DrmWindowHost::GetBounds() {
  return bounds_;
}

void DrmWindowHost::SetCapture() {
  window_manager_->GrabEvents(widget_);
}

void DrmWindowHost::ReleaseCapture() {
  window_manager_->UngrabEvents(widget_);
}

void DrmWindowHost::ToggleFullscreen() {
}

void DrmWindowHost::Maximize() {
}

void DrmWindowHost::Minimize() {
}

void DrmWindowHost::Restore() {
}

void DrmWindowHost::SetCursor(PlatformCursor cursor) {
  cursor_->SetCursor(widget_, cursor);
}

void DrmWindowHost::MoveCursorTo(const gfx::Point& location) {
  event_factory_->WarpCursorTo(widget_, location);
}

void DrmWindowHost::ConfineCursorToBounds(const gfx::Rect& bounds) {
  if (cursor_confined_bounds_ == bounds)
    return;

  cursor_confined_bounds_ = bounds;
  cursor_->ConfineCursorToBounds(widget_, bounds);
}

bool DrmWindowHost::CanDispatchEvent(const PlatformEvent& ne) {
  DCHECK(ne);
  Event* event = static_cast<Event*>(ne);

  // If there is a grab, capture events here.
  gfx::AcceleratedWidget grabber = window_manager_->event_grabber();
  if (grabber != gfx::kNullAcceleratedWidget)
    return grabber == widget_;

  if (event->IsTouchEvent()) {
    // Dispatch the event if it is from the touchscreen associated with the
    // DrmWindowHost. We cannot check the event's location because if the
    // touchscreen has a bezel, touches in the bezel have a location outside of
    // |bounds_|.
    int64_t display_id =
        DeviceDataManager::GetInstance()->GetTargetDisplayForTouchDevice(
            event->source_device_id());

    if (display_id == gfx::Display::kInvalidDisplayID)
      return false;

    DisplaySnapshot* snapshot = display_manager_->GetDisplay(display_id);
    if (!snapshot || !snapshot->current_mode())
      return false;

    gfx::Rect display_bounds(snapshot->origin(),
                             snapshot->current_mode()->size());
    return display_bounds == bounds_;
  } else if (event->IsLocatedEvent()) {
    LocatedEvent* located_event = static_cast<LocatedEvent*>(event);
    return bounds_.Contains(gfx::ToFlooredPoint(located_event->location()));
  }

  // TODO(spang): For non-ash builds we would need smarter keyboard focus.
  return true;
}

uint32_t DrmWindowHost::DispatchEvent(const PlatformEvent& native_event) {
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

void DrmWindowHost::OnChannelEstablished() {
  sender_->Send(new OzoneGpuMsg_CreateWindowDelegate(widget_));
  SendBoundsChange();
  cursor_->ConfineCursorToBounds(widget_, GetCursorConfinedBounds());
}

void DrmWindowHost::OnChannelDestroyed() {
}

void DrmWindowHost::SendBoundsChange() {
  cursor_->PrepareForBoundsChange(widget_);
  sender_->Send(new OzoneGpuMsg_WindowBoundsChanged(widget_, bounds_));
  cursor_->CommitBoundsChange(widget_, bounds_, GetCursorConfinedBounds());
}

}  // namespace ui
