// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_platform.h"

#include <utility>

#include "ash/host/root_window_transformer.h"
#include "ash/host/transformer_helper.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/window_factory.h"
#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/null_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/events/event_sink.h"
#include "ui/events/null_event_targeter.h"
#include "ui/events/ozone/chromeos/cursor_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ash {

AshWindowTreeHostPlatform::AshWindowTreeHostPlatform(
    ui::PlatformWindowInitProperties properties)
    : aura::WindowTreeHostPlatform(std::move(properties),
                                   window_factory::NewWindow()),
      transformer_helper_(this) {
  CommonInit();
}

AshWindowTreeHostPlatform::AshWindowTreeHostPlatform()
    : aura::WindowTreeHostPlatform(window_factory::NewWindow()),
      transformer_helper_(this) {
  CreateCompositor(viz::FrameSinkId(),
                   /* force_software_compositor */ false,
                   /* use_external_begin_frame_control */ false);
  CommonInit();
}

AshWindowTreeHostPlatform::~AshWindowTreeHostPlatform() = default;

void AshWindowTreeHostPlatform::ConfineCursorToRootWindow() {
  if (!allow_confine_cursor())
    return;

  gfx::Rect confined_bounds(GetBoundsInPixels().size());
  confined_bounds.Inset(transformer_helper_.GetHostInsets());
  last_cursor_confine_bounds_in_pixels_ = confined_bounds;
  platform_window()->ConfineCursorToBounds(confined_bounds);
}

void AshWindowTreeHostPlatform::ConfineCursorToBoundsInRoot(
    const gfx::Rect& bounds_in_root) {
  if (!allow_confine_cursor())
    return;

  gfx::RectF bounds_f(bounds_in_root);
  GetRootTransform().TransformRect(&bounds_f);
  last_cursor_confine_bounds_in_pixels_ = gfx::ToEnclosingRect(bounds_f);
  platform_window()->ConfineCursorToBounds(
      last_cursor_confine_bounds_in_pixels_);
}

gfx::Rect AshWindowTreeHostPlatform::GetLastCursorConfineBoundsInPixels()
    const {
  return last_cursor_confine_bounds_in_pixels_;
}

void AshWindowTreeHostPlatform::SetCursorConfig(
    const display::Display& display,
    display::Display::Rotation rotation) {
  // Scale all motion on High-DPI displays.
  float scale = display.device_scale_factor();

  if (!display.IsInternal())
    scale *= 1.2;

  ui::CursorController::GetInstance()->SetCursorConfigForWindow(
      GetAcceleratedWidget(), rotation, scale);
}

void AshWindowTreeHostPlatform::ClearCursorConfig() {
  ui::CursorController::GetInstance()->ClearCursorConfigForWindow(
      GetAcceleratedWidget());
}

void AshWindowTreeHostPlatform::SetRootWindowTransformer(
    std::unique_ptr<RootWindowTransformer> transformer) {
  transformer_helper_.SetRootWindowTransformer(std::move(transformer));
  ConfineCursorToRootWindow();
}

gfx::Insets AshWindowTreeHostPlatform::GetHostInsets() const {
  return transformer_helper_.GetHostInsets();
}

aura::WindowTreeHost* AshWindowTreeHostPlatform::AsWindowTreeHost() {
  return this;
}

void AshWindowTreeHostPlatform::PrepareForShutdown() {
  // Block the root window from dispatching events because it is weird for a
  // ScreenPositionClient not to be attached to the root window and for
  // ui::EventHandlers to be unable to convert the event's location to screen
  // coordinates.
  window()->SetEventTargeter(std::make_unique<aura::NullWindowTargeter>());

  // Do anything platform specific necessary before shutdown (eg. stop
  // listening for configuration XEvents).
  platform_window()->PrepareForShutdown();
}

void AshWindowTreeHostPlatform::SetRootTransform(
    const gfx::Transform& transform) {
  transformer_helper_.SetTransform(transform);
}

gfx::Transform AshWindowTreeHostPlatform::GetRootTransform() const {
  return transformer_helper_.GetTransform();
}

gfx::Transform AshWindowTreeHostPlatform::GetInverseRootTransform() const {
  return transformer_helper_.GetInverseTransform();
}

gfx::Rect AshWindowTreeHostPlatform::GetTransformedRootWindowBoundsInPixels(
    const gfx::Size& host_size_in_pixels) const {
  return transformer_helper_.GetTransformedWindowBounds(host_size_in_pixels);
}

void AshWindowTreeHostPlatform::OnCursorVisibilityChangedNative(bool show) {
  SetTapToClickPaused(!show);
}

void AshWindowTreeHostPlatform::SetBoundsInPixels(const gfx::Rect& bounds) {
  WindowTreeHostPlatform::SetBoundsInPixels(bounds);
  ConfineCursorToRootWindow();
}

void AshWindowTreeHostPlatform::CommonInit() {
  transformer_helper_.Init();
}

void AshWindowTreeHostPlatform::SetTapToClickPaused(bool state) {
  // Temporarily pause tap-to-click when the cursor is hidden.
  ui::OzonePlatform::GetInstance()->GetInputController()->SetTapToClickPaused(
      state);
}

void AshWindowTreeHostPlatform::DispatchEvent(ui::Event* event) {
  TRACE_EVENT0("input", "AshWindowTreeHostPlatform::DispatchEvent");
  if (event->IsLocatedEvent())
    TranslateLocatedEvent(event->AsLocatedEvent());
  return aura::WindowTreeHostPlatform::DispatchEvent(event);
}

}  // namespace ash
