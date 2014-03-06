// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_ozone.h"

#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/ozone/event_factory_ozone.h"
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

  surface_factory->AttemptToResizeAcceleratedWidget(widget_, bounds_);

  base::MessagePumpOzone::Current()->AddDispatcherForRootWindow(this);
  CreateCompositor(GetAcceleratedWidget());
}

WindowTreeHostOzone::~WindowTreeHostOzone() {
  base::MessagePumpOzone::Current()->RemoveDispatcherForRootWindow(0);
  DestroyCompositor();
  DestroyDispatcher();
}

uint32_t WindowTreeHostOzone::Dispatch(const base::NativeEvent& ne) {
  ui::Event* event = static_cast<ui::Event*>(ne);
  ui::EventDispatchDetails details ALLOW_UNUSED = SendEventToProcessor(event);
  return POST_DISPATCH_NONE;
}

gfx::AcceleratedWidget WindowTreeHostOzone::GetAcceleratedWidget() {
  return widget_;
}

void WindowTreeHostOzone::Show() { NOTIMPLEMENTED(); }

void WindowTreeHostOzone::Hide() { NOTIMPLEMENTED(); }

void WindowTreeHostOzone::ToggleFullScreen() { NOTIMPLEMENTED(); }

gfx::Rect WindowTreeHostOzone::GetBounds() const { return bounds_; }

void WindowTreeHostOzone::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Insets WindowTreeHostOzone::GetInsets() const { return gfx::Insets(); }

void WindowTreeHostOzone::SetInsets(const gfx::Insets& insets) {
  NOTIMPLEMENTED();
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

bool WindowTreeHostOzone::ConfineCursorToRootWindow() {
  NOTIMPLEMENTED();
  return false;
}

void WindowTreeHostOzone::UnConfineCursor() { NOTIMPLEMENTED(); }

void WindowTreeHostOzone::PostNativeEvent(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
}

void WindowTreeHostOzone::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  NOTIMPLEMENTED();
}

void WindowTreeHostOzone::PrepareForShutdown() { NOTIMPLEMENTED(); }

void WindowTreeHostOzone::SetCursorNative(gfx::NativeCursor cursor) {
  gfx::SurfaceFactoryOzone::GetInstance()->SetCursorImage(*cursor.platform());
}

void WindowTreeHostOzone::MoveCursorToNative(const gfx::Point& location) {
  gfx::SurfaceFactoryOzone::GetInstance()->MoveCursorTo(location);
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
