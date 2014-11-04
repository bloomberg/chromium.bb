// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window.h"

#include "base/bind.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/dri_cursor.h"
#include "ui/ozone/platform/dri/dri_gpu_platform_support_host.h"
#include "ui/ozone/platform/dri/dri_window_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

DriWindow::DriWindow(PlatformWindowDelegate* delegate,
                     const gfx::Rect& bounds,
                     DriGpuPlatformSupportHost* sender,
                     EventFactoryEvdev* event_factory,
                     DriWindowManager* window_manager)
    : delegate_(delegate),
      sender_(sender),
      event_factory_(event_factory),
      window_manager_(window_manager),
      bounds_(bounds),
      widget_(window_manager->NextAcceleratedWidget()) {
  window_manager_->AddWindow(widget_, this);
}

DriWindow::~DriWindow() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  window_manager_->RemoveWindow(widget_);

  sender_->RemoveChannelObserver(this);
  if (!sender_->IsConnected())
    return;

  sender_->Send(new OzoneGpuMsg_DestroyWindowDelegate(widget_));
}

void DriWindow::Initialize() {
  sender_->AddChannelObserver(this);
  delegate_->OnAcceleratedWidgetAvailable(widget_);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

void DriWindow::Show() {}

void DriWindow::Hide() {}

void DriWindow::Close() {}

void DriWindow::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);

  if (!sender_->IsConnected())
    return;

  if (window_manager_->cursor()->GetCursorWindow() == widget_)
    window_manager_->cursor()->HideCursor();

  sender_->Send(new OzoneGpuMsg_WindowBoundsChanged(widget_, bounds));

  if (window_manager_->cursor()->GetCursorWindow() == widget_)
    window_manager_->cursor()->ShowCursor();
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

void DriWindow::SetCursor(PlatformCursor cursor) {
  DriCursor* dri_cursor = window_manager_->cursor();
  dri_cursor->SetCursor(widget_, cursor);
  // ShowCursor results in a IPC call to GPU. So, we make sure the channel
  // is connected. OnChannelEstablished guarantees that ShowCursor is called
  // eventually.
  if (sender_->IsConnected() && dri_cursor->GetCursorWindow() == widget_)
    dri_cursor->ShowCursor();
}

void DriWindow::MoveCursorTo(const gfx::Point& location) {
  event_factory_->WarpCursorTo(widget_, location);
}

bool DriWindow::CanDispatchEvent(const PlatformEvent& ne) {
  DCHECK(ne);
  Event* event = static_cast<Event*>(ne);
  if (event->IsMouseEvent() || event->IsScrollEvent())
    return window_manager_->cursor()->GetCursorWindow() == widget_;

  return true;
}

uint32_t DriWindow::DispatchEvent(const PlatformEvent& native_event) {
  DispatchEventFromNativeUiEvent(
      native_event, base::Bind(&PlatformWindowDelegate::DispatchEvent,
                               base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void DriWindow::OnChannelEstablished() {
  sender_->Send(new OzoneGpuMsg_CreateWindowDelegate(widget_));
  sender_->Send(new OzoneGpuMsg_WindowBoundsChanged(widget_, bounds_));

  DriCursor* dri_cursor = window_manager_->cursor();
  if (dri_cursor->GetCursorWindow() == widget_)
    dri_cursor->ShowCursor();
}

void DriWindow::OnChannelDestroyed() {
}

}  // namespace ui
