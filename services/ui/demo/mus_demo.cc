// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/demo/mus_demo.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/public/cpp/gpu/gpu.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/aura_extra/image_window_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/wm/core/wm_state.h"

namespace ui {
namespace demo {

namespace {

// Milliseconds between frames.
const int64_t kFrameDelay = 33;

// Size of square in pixels to draw.
const int kSquareSize = 300;

const SkColor kBgColor = SK_ColorRED;
const SkColor kFgColor = SK_ColorYELLOW;

void DrawSquare(const gfx::Rect& bounds, double angle, SkCanvas* canvas) {
  // Create SkRect to draw centered inside the bounds.
  gfx::Point top_left = bounds.CenterPoint();
  top_left.Offset(-kSquareSize / 2, -kSquareSize / 2);
  SkRect rect =
      SkRect::MakeXYWH(top_left.x(), top_left.y(), kSquareSize, kSquareSize);

  // Set SkPaint to fill solid color.
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kFgColor);

  // Rotate the canvas.
  const gfx::Size canvas_size = bounds.size();
  if (angle != 0.0) {
    canvas->translate(SkFloatToScalar(canvas_size.width() * 0.5f),
                      SkFloatToScalar(canvas_size.height() * 0.5f));
    canvas->rotate(angle);
    canvas->translate(-SkFloatToScalar(canvas_size.width() * 0.5f),
                      -SkFloatToScalar(canvas_size.height() * 0.5f));
  }

  canvas->drawRect(rect, paint);
}

}  // namespace

MusDemo::MusDemo() {}

MusDemo::~MusDemo() {
  display::Screen::SetScreenInstance(nullptr);
}

void MusDemo::OnStart() {
  screen_ = base::MakeUnique<display::ScreenBase>();
  display::Screen::SetScreenInstance(screen_.get());

  env_ = aura::Env::CreateInstance(aura::Env::Mode::MUS);
  capture_client_ = base::MakeUnique<aura::client::DefaultCaptureClient>();
  property_converter_ = base::MakeUnique<aura::PropertyConverter>();
  wm_state_ = base::MakeUnique<::wm::WMState>();

  window_tree_client_ = base::MakeUnique<aura::WindowTreeClient>(
      context()->connector(), this, this);
  window_tree_client_->ConnectAsWindowManager();

  env_->SetWindowTreeClient(window_tree_client_.get());
}

bool MusDemo::OnConnect(const service_manager::ServiceInfo& remote_info,
                        service_manager::InterfaceRegistry* registry) {
  return true;
}

void MusDemo::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  // Not called for the WindowManager.
  NOTREACHED();
}

void MusDemo::OnUnembed(aura::Window* root) {
  NOTREACHED();
}

void MusDemo::OnEmbedRootDestroyed(aura::WindowTreeHostMus* window_tree_host) {
  // Not called for the WindowManager.
  NOTREACHED();
}

void MusDemo::OnLostConnection(aura::WindowTreeClient* client) {
  root_window_ = nullptr;
  window_tree_client_.reset();
  timer_.Stop();
}

void MusDemo::OnPointerEventObserved(const PointerEvent& event,
                                     aura::Window* target) {}

aura::client::CaptureClient* MusDemo::GetCaptureClient() {
  return capture_client_.get();
}

aura::PropertyConverter* MusDemo::GetPropertyConverter() {
  return property_converter_.get();
}

void MusDemo::SetWindowManagerClient(aura::WindowManagerClient* client) {}

bool MusDemo::OnWmSetBounds(aura::Window* window, gfx::Rect* bounds) {
  return true;
}

bool MusDemo::OnWmSetProperty(aura::Window* window,
                              const std::string& name,
                              std::unique_ptr<std::vector<uint8_t>>* new_data) {
  return true;
}

aura::Window* MusDemo::OnWmCreateTopLevelWindow(
    mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  NOTREACHED();
  return nullptr;
}

void MusDemo::OnWmClientJankinessChanged(
    const std::set<aura::Window*>& client_windows,
    bool janky) {
  // Don't care
}

void MusDemo::OnWmWillCreateDisplay(const display::Display& display) {
  screen_->display_list().AddDisplay(display,
                                     display::DisplayList::Type::PRIMARY);
}

void MusDemo::OnWmNewDisplay(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
    const display::Display& display) {
  DCHECK(!root_window_);  // Only support one display.

  window_tree_host->InitHost();
  window_tree_host->Show();
  root_window_ = window_tree_host->window();
  // Take ownership of the WTH.
  window_tree_host_ = std::move(window_tree_host);

  // Initialize the window for the bitmap.
  window_delegate_ = new aura_extra::ImageWindowDelegate();
  bitmap_window_ = base::MakeUnique<aura::Window>(window_delegate_);
  bitmap_window_->Init(LAYER_TEXTURED);
  bitmap_window_->SetBounds(root_window_->bounds());
  bitmap_window_->Show();
  bitmap_window_->SetName("Bitmap");

  root_window_->AddChild(bitmap_window_.get());

  // Draw initial frame and start the timer to regularly draw frames.
  DrawFrame();
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kFrameDelay),
               base::Bind(&MusDemo::DrawFrame, base::Unretained(this)));
}

void MusDemo::OnWmDisplayRemoved(aura::WindowTreeHostMus* window_tree_host) {
  timer_.Stop();
  root_window_->RemoveChild(bitmap_window_.get());
  bitmap_window_.reset();
}

void MusDemo::OnWmDisplayModified(const display::Display& display) {}

mojom::EventResult MusDemo::OnAccelerator(uint32_t id, const Event& event) {
  return mojom::EventResult::UNHANDLED;
}

void MusDemo::OnWmPerformMoveLoop(aura::Window* window,
                                  mojom::MoveLoopSource source,
                                  const gfx::Point& cursor_location,
                                  const base::Callback<void(bool)>& on_done) {
  // Don't care
}

void MusDemo::OnWmCancelMoveLoop(aura::Window* window) {}

void MusDemo::OnWmSetClientArea(
    aura::Window* window,
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {}

bool MusDemo::IsWindowActive(aura::Window* window) { return false; }

void MusDemo::OnWmDeactivateWindow(aura::Window* window) {}

void MusDemo::DrawFrame() {
  base::TimeTicks now = base::TimeTicks::Now();

  VLOG(1) << (now - last_draw_frame_time_).InMilliseconds()
          << "ms since the last frame was drawn.";
  last_draw_frame_time_ = now;

  angle_ += 2.0;
  if (angle_ >= 360.0)
    angle_ = 0.0;

  const gfx::Rect& bounds = bitmap_window_->bounds();

  // Allocate a bitmap of correct size.
  SkBitmap bitmap;
  SkImageInfo image_info = SkImageInfo::MakeN32(bounds.width(), bounds.height(),
                                                kPremul_SkAlphaType);
  bitmap.allocPixels(image_info);

  // Draw the rotated square on background in bitmap.
  SkCanvas canvas(bitmap);
  canvas.clear(kBgColor);
  // TODO(kylechar): Add GL drawing instead of software rasterization in future.
  DrawSquare(bounds, angle_, &canvas);
  canvas.flush();

  gfx::ImageSkiaRep image_skia_rep(bitmap, 1);
  gfx::ImageSkia image_skia(image_skia_rep);
  gfx::Image image(image_skia);

  window_delegate_->SetImage(image);
  bitmap_window_->SchedulePaintInRect(bitmap_window_->bounds());
}

}  // namespace demo
}  // namespace aura
