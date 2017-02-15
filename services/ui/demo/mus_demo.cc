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

void DrawSquare(const gfx::Rect& bounds,
                double angle,
                SkCanvas* canvas,
                int size) {
  // Create SkRect to draw centered inside the bounds.
  gfx::Point top_left = bounds.CenterPoint();
  top_left.Offset(-size / 2, -size / 2);
  SkRect rect = SkRect::MakeXYWH(top_left.x(), top_left.y(), size, size);

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

class MusDemo::WindowTreeData {
 public:
  explicit WindowTreeData(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
      int square_size)
      : square_size_(square_size) {
    Init(std::move(window_tree_host));
  }

 private:
  // Initializes the window tree host and start drawing frames.
  void Init(std::unique_ptr<aura::WindowTreeHostMus> window_tree_host);

  // Draws one frame, incrementing the rotation angle.
  void DrawFrame();

  // Helper function to retrieve the window to which we draw the bitmap.
  aura::Window* bitmap_window() {
    DCHECK(!window_tree_host_->window()->children().empty());
    return window_tree_host_->window()->children()[0];
  }

  // The Window tree host corresponding to this data.
  std::unique_ptr<aura::WindowTreeHostMus> window_tree_host_;

  // Destroys itself when the window gets destroyed.
  aura_extra::ImageWindowDelegate* window_delegate_ = nullptr;

  // Timer for calling DrawFrame().
  base::RepeatingTimer timer_;

  // Current rotation angle for drawing.
  double angle_ = 0.0;

  // Size in pixels of the square to draw.
  const int square_size_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeData);
};

void MusDemo::WindowTreeData::Init(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  window_tree_host->InitHost();
  window_tree_host->Show();
  // Take ownership of the WTH.
  window_tree_host_ = std::move(window_tree_host);

  // Initialize the window for the bitmap.
  window_delegate_ = new aura_extra::ImageWindowDelegate();
  aura::Window* root_window = window_tree_host_->window();
  aura::Window* bitmap_window = new aura::Window(window_delegate_);
  bitmap_window->Init(LAYER_TEXTURED);
  bitmap_window->SetBounds(gfx::Rect(root_window->bounds().size()));
  bitmap_window->Show();
  bitmap_window->SetName("Bitmap");
  root_window->AddChild(bitmap_window);

  // Draw initial frame and start the timer to regularly draw frames.
  DrawFrame();
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kFrameDelay),
               base::Bind(&WindowTreeData::DrawFrame, base::Unretained(this)));
}

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
  window_tree_client_.reset();
  window_tree_data_.reset();
}

void MusDemo::OnPointerEventObserved(const PointerEvent& event,
                                     aura::Window* target) {}

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

void MusDemo::OnWmSetCanFocus(aura::Window* window, bool can_focus) {}

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
  DCHECK(!window_tree_data_);  // Only support one display.
  window_tree_data_ = base::MakeUnique<WindowTreeData>(
      std::move(window_tree_host), kSquareSize);
}

void MusDemo::OnWmDisplayRemoved(aura::WindowTreeHostMus* window_tree_host) {
  window_tree_data_.reset();
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

void MusDemo::WindowTreeData::DrawFrame() {
  angle_ += 2.0;
  if (angle_ >= 360.0)
    angle_ = 0.0;

  const gfx::Rect& bounds = bitmap_window()->bounds();

  // Allocate a bitmap of correct size.
  SkBitmap bitmap;
  SkImageInfo image_info = SkImageInfo::MakeN32(bounds.width(), bounds.height(),
                                                kPremul_SkAlphaType);
  bitmap.allocPixels(image_info);

  // Draw the rotated square on background in bitmap.
  SkCanvas canvas(bitmap);
  canvas.clear(kBgColor);
  // TODO(kylechar): Add GL drawing instead of software rasterization in future.
  DrawSquare(bounds, angle_, &canvas, square_size_);
  canvas.flush();

  gfx::ImageSkiaRep image_skia_rep(bitmap, 1);
  gfx::ImageSkia image_skia(image_skia_rep);
  gfx::Image image(image_skia);

  window_delegate_->SetImage(image);
  bitmap_window()->SchedulePaintInRect(gfx::Rect(bounds.size()));
}

}  // namespace demo
}  // namespace aura
