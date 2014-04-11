// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_ozone.h"

#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/ozone/cursor_factory_ozone.h"
#include "ui/events/ozone/event_factory_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"
#include "ui/ozone/ozone_platform.h"

namespace aura {

WindowTreeHostOzone::WindowTreeHostOzone(const gfx::Rect& bounds)
    : widget_(0),
      bounds_(bounds) {
  ui::OzonePlatform::Initialize();

  // EventFactoryOzone creates converters that obtain input events from the
  // underlying input system and dispatch them as |ui::Event| instances into
  // Aura.
  ui::EventFactoryOzone::GetInstance()->StartProcessingEvents();

  gfx::SurfaceFactoryOzone* surface_factory =
      gfx::SurfaceFactoryOzone::GetInstance();
  widget_ = surface_factory->GetAcceleratedWidget();

  ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  CreateCompositor(GetAcceleratedWidget());
}

WindowTreeHostOzone::~WindowTreeHostOzone() {
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  DestroyCompositor();
  DestroyDispatcher();
}

bool WindowTreeHostOzone::CanDispatchEvent(const ui::PlatformEvent& ne) {
  CHECK(ne);
  ui::Event* event = static_cast<ui::Event*>(ne);
  if (event->IsMouseEvent() || event->IsScrollEvent() || event->IsTouchEvent())
    return bounds_.Contains(static_cast<ui::LocatedEvent*>(event)->location());
  return true;
}

uint32_t WindowTreeHostOzone::DispatchEvent(const ui::PlatformEvent& ne) {
  ui::Event* event = static_cast<ui::Event*>(ne);
  ui::EventDispatchDetails details ALLOW_UNUSED = SendEventToProcessor(event);
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

gfx::AcceleratedWidget WindowTreeHostOzone::GetAcceleratedWidget() {
  return widget_;
}

void WindowTreeHostOzone::Show() { NOTIMPLEMENTED(); }

void WindowTreeHostOzone::Hide() { NOTIMPLEMENTED(); }

gfx::Rect WindowTreeHostOzone::GetBounds() const { return bounds_; }

void WindowTreeHostOzone::SetBounds(const gfx::Rect& bounds) {
  bool origin_changed = bounds_.origin() != bounds.origin();
  bool size_changed = bounds_.size() != bounds.size();
  bounds_ = bounds;
  if (size_changed)
    OnHostResized(bounds_.size());
  if (origin_changed)
    OnHostMoved(bounds_.origin());
}

gfx::Point WindowTreeHostOzone::GetLocationOnNativeScreen() const {
  return bounds_.origin();
}

void WindowTreeHostOzone::SetCapture() { NOTIMPLEMENTED(); }

void WindowTreeHostOzone::ReleaseCapture() { NOTIMPLEMENTED(); }

bool WindowTreeHostOzone::QueryMouseLocation(gfx::Point* location_return) {
  NOTIMPLEMENTED();
  return false;
}

void WindowTreeHostOzone::PostNativeEvent(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
}

void WindowTreeHostOzone::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  NOTIMPLEMENTED();
}

void WindowTreeHostOzone::SetCursorNative(gfx::NativeCursor cursor) {
  ui::CursorFactoryOzone::GetInstance()->SetCursor(GetAcceleratedWidget(),
                                                   cursor.platform());
}

void WindowTreeHostOzone::MoveCursorToNative(const gfx::Point& location) {
  ui::EventFactoryOzone::GetInstance()->WarpCursorTo(GetAcceleratedWidget(),
                                                     location);
}

void WindowTreeHostOzone::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

ui::EventProcessor* WindowTreeHostOzone::GetEventProcessor() {
  return dispatcher();
}

// static
WindowTreeHost* WindowTreeHost::Create(const gfx::Rect& bounds) {
  return new WindowTreeHostOzone(bounds);
}

// static
gfx::Size WindowTreeHost::GetNativeScreenSize() {
  NOTIMPLEMENTED();
  return gfx::Size();
}

}  // namespace aura
