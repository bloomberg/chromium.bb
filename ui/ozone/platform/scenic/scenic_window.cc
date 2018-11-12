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
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

ScenicWindow::ScenicWindow(ScenicWindowManager* window_manager,
                           PlatformWindowDelegate* delegate,
                           zx::eventpair view_token)
    : manager_(window_manager),
      delegate_(delegate),
      window_id_(manager_->AddWindow(this)),
      event_dispatcher_(this),
      view_listener_binding_(this),
      scenic_session_(manager_->GetScenic()),
      parent_node_(&scenic_session_),
      node_(&scenic_session_),
      shape_node_(&scenic_session_),
      material_(&scenic_session_),
      input_listener_binding_(this) {
  scenic_session_.set_error_handler(
      fit::bind_member(this, &ScenicWindow::OnScenicError));
  scenic_session_.set_event_handler(
      fit::bind_member(this, &ScenicWindow::OnScenicEvents));

  // Import parent node and create event pair to pass to the ViewManager.
  zx::eventpair parent_export_token;
  parent_node_.BindAsRequest(&parent_export_token);

  // Setup entity node for the window.
  parent_node_.AddChild(node_);
  node_.AddChild(shape_node_);
  shape_node_.SetMaterial(material_);

  // Subscribe to metrics events from the parent node. These events are used to
  // get the device pixel ratio for the screen.
  parent_node_.SetEventMask(fuchsia::ui::gfx::kMetricsEventMask);

  // Create the view.
  manager_->GetViewManager()->CreateView2(
      view_.NewRequest(), std::move(view_token),
      view_listener_binding_.NewBinding(), std::move(parent_export_token),
      "Chromium");
  view_.set_error_handler(fit::bind_member(this, &ScenicWindow::OnViewError));
  view_listener_binding_.set_error_handler(
      fit::bind_member(this, &ScenicWindow::OnViewError));

  // Setup ViewsV1 input event listener.
  // TODO(crbug.com/881591): Remove this when ViewsV1 deprecation is complete.
  fuchsia::sys::ServiceProviderPtr view_service_provider;
  view_->GetServiceProvider(view_service_provider.NewRequest());
  view_service_provider->ConnectToService(
      fuchsia::ui::input::InputConnection::Name_,
      input_connection_.NewRequest().TakeChannel());
  input_connection_->SetEventListener(input_listener_binding_.NewBinding());

  // Call Present() to ensure that the scenic session commands are processed,
  // which is necessary to receive metrics event from Scenic.
  scenic_session_.Present(
      /*presentation_time=*/0, [](fuchsia::images::PresentationInfo info) {});

  delegate_->OnAcceleratedWidgetAvailable(window_id_);
}

ScenicWindow::~ScenicWindow() {
  manager_->RemoveWindow(window_id_, this);
}

void ScenicWindow::SetTexture(const scenic::Image& image) {
  material_.SetTexture(image);
}

void ScenicWindow::SetTexture(uint32_t image_id) {
  material_.SetTexture(image_id);
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

void ScenicWindow::Show() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Hide() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Close() {
  NOTIMPLEMENTED();
}

void ScenicWindow::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

void ScenicWindow::SetCapture() {
  NOTIMPLEMENTED();
}

void ScenicWindow::ReleaseCapture() {
  NOTIMPLEMENTED();
}

bool ScenicWindow::HasCapture() const {
  NOTIMPLEMENTED();
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
  return PLATFORM_WINDOW_STATE_NORMAL;
}

void ScenicWindow::SetCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void ScenicWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void ScenicWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

PlatformImeController* ScenicWindow::GetPlatformImeController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void ScenicWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Rect ScenicWindow::GetRestoredBoundsInPixels() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void ScenicWindow::UpdateSize() {
  gfx::SizeF scaled = ScaleSize(size_dips_, device_pixel_ratio_);
  size_pixels_ = gfx::Size(ceilf(scaled.width()), ceilf(scaled.height()));
  gfx::Rect size_rect(size_pixels_);

  // Update this window's Screen's dimensions to match the new size.
  ScenicScreen* screen = manager_->screen();
  if (screen)
    screen->OnWindowBoundsChanged(window_id_, size_rect);

  // Set node shape to rectangle that matches size of the view.
  scenic::Rectangle rect(&scenic_session_, size_dips_.width(),
                         size_dips_.height());
  shape_node_.SetShape(rect);

  // Translate the node by half of the view dimensions to put it in the center
  // of the view.
  shape_node_.SetTranslation(size_dips_.width() / 2.0,
                             size_dips_.height() / 2.0, 0.f);

  // This is necessary when using vulkan because ImagePipes are presented
  // separately and we need to make sure our sizes change is committed.
  scenic_session_.Present(
      /*presentation_time=*/0, [](fuchsia::images::PresentationInfo info) {});

  delegate_->OnBoundsChanged(size_rect);
}

void ScenicWindow::OnPropertiesChanged(
    fuchsia::ui::viewsv1::ViewProperties properties,
    OnPropertiesChangedCallback callback) {
  if (properties.view_layout) {
    size_dips_.SetSize(properties.view_layout->size.width,
                       properties.view_layout->size.height);
    if (device_pixel_ratio_ > 0.0)
      UpdateSize();
  }

  callback();
}

void ScenicWindow::OnScenicError() {
  LOG(ERROR) << "scenic::Session failed.";
  delegate_->OnClosed();
}

void ScenicWindow::OnScenicEvents(
    fidl::VectorPtr<fuchsia::ui::scenic::Event> events) {
  for (const auto& event : *events) {
    if (event.is_gfx()) {
      if (!event.gfx().is_metrics())
        continue;

      auto& metrics = event.gfx().metrics();
      if (metrics.node_id != parent_node_.id())
        continue;

      device_pixel_ratio_ =
          std::max(metrics.metrics.scale_x, metrics.metrics.scale_y);

      ScenicScreen* screen = manager_->screen();
      if (screen)
        screen->OnWindowMetrics(window_id_, device_pixel_ratio_);

      if (!size_dips_.IsEmpty())
        UpdateSize();
    } else if (event.is_input()) {
      auto& input_event = event.input();
      if (input_event.is_focus()) {
        delegate_->OnActivationChanged(input_event.focus().focused);
      } else {
        // Scenic doesn't care if the input event was handled, so ignore the
        // "handled" status.
        ignore_result(event_dispatcher_.ProcessEvent(input_event));
      }
    }
  }
}

void ScenicWindow::OnEvent(fuchsia::ui::input::InputEvent event,
                           OnEventCallback callback) {
  bool result = false;

  if (event.is_focus()) {
    delegate_->OnActivationChanged(event.focus().focused);
    result = true;
  } else {
    result = event_dispatcher_.ProcessEvent(event);
  }

  callback(result);
}

void ScenicWindow::OnViewError() {
  VLOG(1) << "viewsv1::View connection was closed.";
  delegate_->OnClosed();
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
