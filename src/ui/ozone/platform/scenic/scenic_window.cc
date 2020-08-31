// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"

namespace ui {

ScenicWindow::ScenicWindow(ScenicWindowManager* window_manager,
                           PlatformWindowDelegate* delegate,
                           fuchsia::ui::views::ViewToken view_token,
                           scenic::ViewRefPair view_ref_pair)
    : manager_(window_manager),
      delegate_(delegate),
      window_id_(manager_->AddWindow(this)),
      event_dispatcher_(this),
      scenic_session_(manager_->GetScenic()),
      view_(&scenic_session_,
            std::move(view_token),
            std::move(view_ref_pair.control_ref),
            std::move(view_ref_pair.view_ref),
            "chromium window"),
      node_(&scenic_session_),
      input_node_(&scenic_session_),
      render_node_(&scenic_session_),
      background_node_(&scenic_session_) {
  scenic_session_.set_error_handler(
      fit::bind_member(this, &ScenicWindow::OnScenicError));
  scenic_session_.set_event_handler(
      fit::bind_member(this, &ScenicWindow::OnScenicEvents));
  scenic_session_.SetDebugName("Chromium ScenicWindow");

  // Subscribe to metrics events from the node. These events are used to
  // get the device pixel ratio for the screen.
  node_.SetEventMask(fuchsia::ui::gfx::kMetricsEventMask);

  // Add input shape.
  node_.AddChild(input_node_);

  // Add rendering subtree, rooted at Z=-2 to make room for background layers in
  // the Z-order (lesser values are higher in the visual ordering).
  constexpr float kRenderNodeZPosition = -2.;
  constexpr float kBackgroundNodeZPosition = kRenderNodeZPosition + 1.;
  render_node_.SetTranslation(0., 0., kRenderNodeZPosition);
  node_.AddChild(render_node_);

  // Initialize a black background to be just behind |render_node_|.
  scenic::Material background_color(&scenic_session_);
  background_color.SetColor(0, 0, 0, 255);  // RGBA (0,0,0,255) = opaque black.
  background_node_.SetMaterial(background_color);
  scenic::Rectangle background_shape(&scenic_session_, 1., 1.);
  background_node_.SetShape(background_shape);
  background_node_.SetTranslation(0., 0., kBackgroundNodeZPosition);
  node_.AddChild(background_node_);

  // Render the background immediately.
  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});

  delegate_->OnAcceleratedWidgetAvailable(window_id_);
}

ScenicWindow::~ScenicWindow() {
  manager_->RemoveWindow(window_id_, this);
}

void ScenicWindow::AttachSurfaceView(
    fuchsia::ui::views::ViewHolderToken token) {
  surface_view_holder_ = std::make_unique<scenic::ViewHolder>(
      &scenic_session_, std::move(token), "chromium window surface");

  // Configure the ViewHolder not to be focusable, or hit-testable, to ensure
  // that it cannot receive input.
  fuchsia::ui::gfx::ViewProperties view_properties;
  view_properties.bounding_box = {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
  view_properties.focus_change = false;
  surface_view_holder_->SetViewProperties(std::move(view_properties));
  surface_view_holder_->SetHitTestBehavior(
      fuchsia::ui::gfx::HitTestBehavior::kSuppress);

  render_node_.DetachChildren();
  render_node_.AddChild(*surface_view_holder_);

  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});
}

gfx::Rect ScenicWindow::GetBounds() {
  return gfx::Rect(size_pixels_);
}

void ScenicWindow::SetBounds(const gfx::Rect& bounds) {
  // View dimensions are controlled by the containing view, it's not possible to
  // set them here.
}

void ScenicWindow::SetTitle(const base::string16& title) {
  NOTIMPLEMENTED();
}

void ScenicWindow::Show(bool inactive) {
  view_.AddChild(node_);

  // Call Present2() to ensure that the scenic session commands are processed,
  // which is necessary to receive metrics event from Scenic.
  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});
}

void ScenicWindow::Hide() {
  node_.Detach();
}

void ScenicWindow::Close() {
  Hide();
  delegate_->OnClosed();
}

bool ScenicWindow::IsVisible() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

void ScenicWindow::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

void ScenicWindow::SetCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::ReleaseCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool ScenicWindow::HasCapture() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void ScenicWindow::ToggleFullscreen() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Maximize() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Minimize() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Restore() {
  NOTIMPLEMENTED();
}

PlatformWindowState ScenicWindow::GetPlatformWindowState() const {
  return PlatformWindowState::kNormal;
}

void ScenicWindow::Activate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::SetUseNativeFrame(bool use_native_frame) {}

bool ScenicWindow::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void ScenicWindow::SetCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void ScenicWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Rect ScenicWindow::GetRestoredBoundsInPixels() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void ScenicWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                  const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED();
}

void ScenicWindow::SizeConstraintsChanged() {
  NOTIMPLEMENTED();
}

void ScenicWindow::UpdateSize() {
  gfx::SizeF scaled = ScaleSize(size_dips_, device_pixel_ratio_);
  size_pixels_ = gfx::Size(ceilf(scaled.width()), ceilf(scaled.height()));
  gfx::Rect size_rect(size_pixels_);

  // Update this window's Screen's dimensions to match the new size.
  ScenicScreen* screen = manager_->screen();
  if (screen)
    screen->OnWindowBoundsChanged(window_id_, size_rect);

  // Translate the node by half of the view dimensions to put it in the center
  // of the view.
  node_.SetTranslation(size_dips_.width() / 2.0, size_dips_.height() / 2.0,
                       0.f);

  // Scale the render node so that surface rect can always be 1x1.
  render_node_.SetScale(size_dips_.width(), size_dips_.height(), 1.f);

  // Resize input node to cover the whole surface.
  scenic::Rectangle window_rect(&scenic_session_, size_dips_.width(),
                                size_dips_.height());
  input_node_.SetShape(window_rect);

  // Resize the input and background nodes to cover the whole surface.
  background_node_.SetShape(window_rect);

  // This is necessary when using vulkan because ImagePipes are presented
  // separately and we need to make sure our sizes change is committed.
  scenic_session_.Present2(
      /*requested_presentation_time=*/0,
      /*requested_prediction_span=*/0,
      [](fuchsia::scenic::scheduling::FuturePresentationTimes info) {});

  delegate_->OnBoundsChanged(size_rect);
}

void ScenicWindow::OnScenicError(zx_status_t status) {
  LOG(ERROR) << "scenic::Session failed with code " << status << ".";
  delegate_->OnClosed();
}

void ScenicWindow::OnScenicEvents(
    std::vector<fuchsia::ui::scenic::Event> events) {
  for (const auto& event : events) {
    if (event.is_gfx()) {
      switch (event.gfx().Which()) {
        case fuchsia::ui::gfx::Event::kMetrics: {
          if (event.gfx().metrics().node_id != node_.id())
            continue;
          OnViewMetrics(event.gfx().metrics().metrics);
          break;
        }
        case fuchsia::ui::gfx::Event::kViewPropertiesChanged: {
          DCHECK(event.gfx().view_properties_changed().view_id == view_.id());
          OnViewProperties(event.gfx().view_properties_changed().properties);
          break;
        }
        case fuchsia::ui::gfx::Event::kViewAttachedToScene: {
          DCHECK(event.gfx().view_attached_to_scene().view_id == view_.id());
          OnViewAttachedChanged(true);
          break;
        }
        case fuchsia::ui::gfx::Event::kViewDetachedFromScene: {
          DCHECK(event.gfx().view_detached_from_scene().view_id == view_.id());
          OnViewAttachedChanged(false);
          break;
        }
        default:
          break;
      }
    } else if (event.is_input()) {
      OnInputEvent(event.input());
    }
  }
}

void ScenicWindow::OnViewMetrics(const fuchsia::ui::gfx::Metrics& metrics) {
  device_pixel_ratio_ = std::max(metrics.scale_x, metrics.scale_y);

  ScenicScreen* screen = manager_->screen();
  if (screen)
    screen->OnWindowMetrics(window_id_, device_pixel_ratio_);

  if (!size_dips_.IsEmpty())
    UpdateSize();
}

void ScenicWindow::OnViewProperties(
    const fuchsia::ui::gfx::ViewProperties& properties) {
  float width = properties.bounding_box.max.x - properties.bounding_box.min.x -
                properties.inset_from_min.x - properties.inset_from_max.x;
  float height = properties.bounding_box.max.y - properties.bounding_box.min.y -
                 properties.inset_from_min.y - properties.inset_from_max.y;

  size_dips_.SetSize(width, height);
  if (device_pixel_ratio_ > 0.0)
    UpdateSize();
}

void ScenicWindow::OnViewAttachedChanged(bool is_view_attached) {
  delegate_->OnWindowStateChanged(is_view_attached
                                      ? PlatformWindowState::kNormal
                                      : PlatformWindowState::kMinimized);
}

void ScenicWindow::OnInputEvent(const fuchsia::ui::input::InputEvent& event) {
  if (event.is_focus()) {
    delegate_->OnActivationChanged(event.focus().focused);
  } else {
    // Scenic doesn't care if the input event was handled, so ignore the
    // "handled" status.
    ignore_result(event_dispatcher_.ProcessEvent(event));
  }
}

void ScenicWindow::DispatchEvent(ui::Event* event) {
  if (event->IsLocatedEvent()) {
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    gfx::PointF location = located_event->location_f();
    location.Scale(device_pixel_ratio_);
    located_event->set_location_f(location);
  }
  delegate_->DispatchEvent(event);
}

}  // namespace ui
