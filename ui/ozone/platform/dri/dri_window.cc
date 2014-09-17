// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window.h"

#include "base/bind.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/dri/dri_cursor.h"
#include "ui/ozone/platform/dri/dri_window_delegate.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
#include "ui/ozone/platform/dri/dri_window_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

DriWindow::DriWindow(PlatformWindowDelegate* delegate,
                     const gfx::Rect& bounds,
                     scoped_ptr<DriWindowDelegate> dri_window_delegate,
                     EventFactoryEvdev* event_factory,
                     DriWindowDelegateManager* window_delegate_manager,
                     DriWindowManager* window_manager)
    : delegate_(delegate),
      bounds_(bounds),
      widget_(dri_window_delegate->GetAcceleratedWidget()),
      dri_window_delegate_(dri_window_delegate.get()),
      event_factory_(event_factory),
      window_delegate_manager_(window_delegate_manager),
      window_manager_(window_manager) {
  window_delegate_manager_->AddWindowDelegate(widget_,
                                              dri_window_delegate.Pass());
  window_manager_->AddWindow(widget_, this);
}

DriWindow::~DriWindow() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  dri_window_delegate_->Shutdown();
  window_manager_->RemoveWindow(widget_);
  window_delegate_manager_->RemoveWindowDelegate(widget_);
}

void DriWindow::Initialize() {
  dri_window_delegate_->Initialize();
  dri_window_delegate_->OnBoundsChanged(bounds_);
  delegate_->OnAcceleratedWidgetAvailable(widget_);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

void DriWindow::Show() {}

void DriWindow::Hide() {}

void DriWindow::Close() {}

void DriWindow::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
  if (window_manager_->cursor()->GetCursorWindow() == widget_)
    window_manager_->cursor()->HideCursor();

  dri_window_delegate_->OnBoundsChanged(bounds);

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
  window_manager_->cursor()->SetCursor(widget_, cursor);
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
      native_event,
      base::Bind(&PlatformWindowDelegate::DispatchEvent,
                 base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui
