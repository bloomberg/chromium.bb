// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/platform_display.h"

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "cc/ipc/quads.mojom.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/delegated_frame_data.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/display/platform_screen.h"
#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/platform_display_factory.h"
#include "services/ui/ws/platform_display_init_params.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/window_coordinate_conversions.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/platform_window/platform_ime_controller.h"
#include "ui/platform_window/platform_window.h"

#if defined(OS_WIN)
#include "ui/platform_window/win/win_window.h"
#elif defined(USE_X11)
#include "ui/platform_window/x11/x11_window.h"
#elif defined(OS_ANDROID)
#include "ui/platform_window/android/platform_window_android.h"
#elif defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ui {

namespace ws {

// static
PlatformDisplayFactory* PlatformDisplay::factory_ = nullptr;

// static
PlatformDisplay* PlatformDisplay::Create(
    const PlatformDisplayInitParams& init_params) {
  if (factory_)
    return factory_->CreatePlatformDisplay();

  return new DefaultPlatformDisplay(init_params);
}

DefaultPlatformDisplay::DefaultPlatformDisplay(
    const PlatformDisplayInitParams& init_params)
    : id_(init_params.display_id),
#if !defined(OS_ANDROID)
      cursor_loader_(ui::CursorLoader::Create()),
#endif
      frame_generator_(new FrameGenerator(this,
                                          init_params.root_window,
                                          init_params.display_compositor)),
      metrics_(init_params.metrics) {
}

void DefaultPlatformDisplay::Init(PlatformDisplayDelegate* delegate) {
  delegate_ = delegate;

  DCHECK(!metrics_.pixel_size.IsEmpty());

  // TODO(kylechar): The origin here isn't right if any displays have
  // scale_factor other than 1.0 but will prevent windows from being stacked.
  gfx::Rect bounds(metrics_.bounds.origin(), metrics_.pixel_size);
#if defined(OS_WIN)
  platform_window_ = base::MakeUnique<ui::WinWindow>(this, bounds);
#elif defined(USE_X11)
  platform_window_ = base::MakeUnique<ui::X11Window>(this);
#elif defined(OS_ANDROID)
  platform_window_ = base::MakeUnique<ui::PlatformWindowAndroid>(this);
#elif defined(USE_OZONE)
  platform_window_ =
      ui::OzonePlatform::GetInstance()->CreatePlatformWindow(this, bounds);
#else
  NOTREACHED() << "Unsupported platform";
#endif

  platform_window_->SetBounds(bounds);
  platform_window_->Show();
}

int64_t DefaultPlatformDisplay::GetId() const {
  return id_;
}

DefaultPlatformDisplay::~DefaultPlatformDisplay() {
  // Don't notify the delegate from the destructor.
  delegate_ = nullptr;

  frame_generator_.reset();
  // Destroy the PlatformWindow early on as it may call us back during
  // destruction and we want to be in a known state. But destroy the surface
  // first because it can still be using the platform window.
  platform_window_.reset();
}

void DefaultPlatformDisplay::SchedulePaint(const ServerWindow* window,
                                           const gfx::Rect& bounds) {
  DCHECK(window);
  if (!window->IsDrawn())
    return;
  const gfx::Rect root_relative_rect =
      ConvertRectBetweenWindows(window, delegate_->GetRootWindow(), bounds);
  if (root_relative_rect.IsEmpty())
    return;
  frame_generator_->RequestRedraw(root_relative_rect);
}

void DefaultPlatformDisplay::SetViewportSize(const gfx::Size& size) {
  platform_window_->SetBounds(gfx::Rect(size));
}

void DefaultPlatformDisplay::SetTitle(const base::string16& title) {
  platform_window_->SetTitle(title);
}

void DefaultPlatformDisplay::SetCapture() {
  platform_window_->SetCapture();
}

void DefaultPlatformDisplay::ReleaseCapture() {
  platform_window_->ReleaseCapture();
}

void DefaultPlatformDisplay::SetCursorById(mojom::Cursor cursor_id) {
#if !defined(OS_ANDROID)
  // TODO(erg): This still isn't sufficient, and will only use native cursors
  // that chrome would use, not custom image cursors. For that, we should
  // delegate to the window manager to load images from resource packs.
  //
  // We probably also need to deal with different DPIs.
  ui::Cursor cursor(static_cast<int32_t>(cursor_id));
  cursor_loader_->SetPlatformCursor(&cursor);
  platform_window_->SetCursor(cursor.platform());
#endif
}

void DefaultPlatformDisplay::UpdateTextInputState(
    const ui::TextInputState& state) {
  ui::PlatformImeController* ime = platform_window_->GetPlatformImeController();
  if (ime)
    ime->UpdateTextInputState(state);
}

void DefaultPlatformDisplay::SetImeVisibility(bool visible) {
  ui::PlatformImeController* ime = platform_window_->GetPlatformImeController();
  if (ime)
    ime->SetImeVisibility(visible);
}

bool DefaultPlatformDisplay::IsFramePending() const {
  return frame_generator_->is_frame_pending();
}

gfx::Rect DefaultPlatformDisplay::GetBounds() const {
  return metrics_.bounds;
}

bool DefaultPlatformDisplay::IsPrimaryDisplay() const {
  return display::PlatformScreen::GetInstance()->GetPrimaryDisplayId() == id_;
}

void DefaultPlatformDisplay::OnGpuChannelEstablished(
    scoped_refptr<gpu::GpuChannelHost> channel) {
  frame_generator_->OnGpuChannelEstablished(channel);
}

bool DefaultPlatformDisplay::UpdateViewportMetrics(
    const display::ViewportMetrics& metrics) {
  if (metrics_ == metrics)
    return false;

  gfx::Rect bounds = platform_window_->GetBounds();
  if (bounds.size() != metrics.pixel_size) {
    bounds.set_size(metrics.pixel_size);
    platform_window_->SetBounds(bounds);
  }

  metrics_ = metrics;
  return true;
}

const display::ViewportMetrics& DefaultPlatformDisplay::GetViewportMetrics()
    const {
  return metrics_;
}

void DefaultPlatformDisplay::UpdateEventRootLocation(ui::LocatedEvent* event) {
  gfx::Point location = event->location();
  location.Offset(metrics_.bounds.x(), metrics_.bounds.y());
  event->set_root_location(location);
}

void DefaultPlatformDisplay::OnBoundsChanged(const gfx::Rect& new_bounds) {
  // We only care if the window size has changed.
  if (new_bounds.size() == metrics_.pixel_size)
    return;

  // TODO(kylechar): Maybe do something here. For CrOS we don't need to support
  // PlatformWindow initiated resizes. For other platforms we need to do
  // something but that isn't fully flushed out.
}

void DefaultPlatformDisplay::OnDamageRect(const gfx::Rect& damaged_region) {
  frame_generator_->RequestRedraw(damaged_region);
}

void DefaultPlatformDisplay::DispatchEvent(ui::Event* event) {
  if (event->IsLocatedEvent())
    UpdateEventRootLocation(event->AsLocatedEvent());

  if (event->IsScrollEvent()) {
    // TODO(moshayedi): crbug.com/602859. Dispatch scroll events as
    // they are once we have proper support for scroll events.
    delegate_->OnEvent(
        ui::PointerEvent(ui::MouseWheelEvent(*event->AsScrollEvent())));
  } else if (event->IsMouseEvent()) {
    delegate_->OnEvent(ui::PointerEvent(*event->AsMouseEvent()));
  } else if (event->IsTouchEvent()) {
    delegate_->OnEvent(ui::PointerEvent(*event->AsTouchEvent()));
  } else {
    delegate_->OnEvent(*event);
  }

#if defined(USE_X11) || defined(USE_OZONE)
  // We want to emulate the WM_CHAR generation behaviour of Windows.
  //
  // On Linux, we've previously inserted characters by having
  // InputMethodAuraLinux take all key down events and send a character event
  // to the TextInputClient. This causes a mismatch in code that has to be
  // shared between Windows and Linux, including blink code. Now that we're
  // trying to have one way of doing things, we need to standardize on and
  // emulate Windows character events.
  //
  // This is equivalent to what we're doing in the current Linux port, but
  // done once instead of done multiple times in different places.
  if (event->type() == ui::ET_KEY_PRESSED) {
    ui::KeyEvent* key_press_event = event->AsKeyEvent();
    ui::KeyEvent char_event(key_press_event->GetCharacter(),
                            key_press_event->key_code(),
                            key_press_event->flags());
    // We don't check that GetCharacter() is equal because changing a key event
    // with an accelerator to a character event can change the character, for
    // example, from 'M' to '^M'.
    DCHECK_EQ(key_press_event->key_code(), char_event.key_code());
    DCHECK_EQ(key_press_event->flags(), char_event.flags());
    delegate_->OnEvent(char_event);
  }
#endif
}

void DefaultPlatformDisplay::OnCloseRequest() {
  display::PlatformScreen::GetInstance()->RequestCloseDisplay(GetId());
}

void DefaultPlatformDisplay::OnClosed() {}

void DefaultPlatformDisplay::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {}

void DefaultPlatformDisplay::OnLostCapture() {
  delegate_->OnNativeCaptureLost();
}

void DefaultPlatformDisplay::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_scale_factor) {
  frame_generator_->OnAcceleratedWidgetAvailable(widget);
}

void DefaultPlatformDisplay::OnAcceleratedWidgetDestroyed() {
  NOTREACHED();
}

void DefaultPlatformDisplay::OnActivationChanged(bool active) {}

ServerWindow* DefaultPlatformDisplay::GetRootWindow() {
  return delegate_->GetRootWindow();
}

bool DefaultPlatformDisplay::IsInHighContrastMode() {
  return delegate_ ? delegate_->IsInHighContrastMode() : false;
}

}  // namespace ws

}  // namespace ui
