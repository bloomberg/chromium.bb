// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/platform_window/fuchsia/scenic_window_delegate.h"

namespace ui {

namespace {

// Converts Scenic's rect-based representation of insets to gfx::Insets.
// Returns zero-width insets if |inset_from_min| and |inset_from_max| are
// uninitialized (indicating that no insets were provided from Scenic).
gfx::Insets ConvertInsets(
    float device_pixel_ratio,
    const fuchsia::ui::gfx::ViewProperties& view_properties) {
  return gfx::Insets(device_pixel_ratio * view_properties.inset_from_min.y,
                     device_pixel_ratio * view_properties.inset_from_min.x,
                     device_pixel_ratio * view_properties.inset_from_max.y,
                     device_pixel_ratio * view_properties.inset_from_max.x);
}

}  // namespace

ScenicWindow::ScenicWindow(ScenicWindowManager* window_manager,
                           PlatformWindowDelegate* delegate,
                           PlatformWindowInitProperties properties)
    : manager_(window_manager),
      delegate_(delegate),
      scenic_window_delegate_(properties.scenic_window_delegate),
      window_id_(manager_->AddWindow(this)),
      view_ref_(std::move(properties.view_ref_pair.view_ref)),
      event_dispatcher_(this),
      scenic_session_(manager_->GetScenic()),
      safe_presenter_(&scenic_session_),
      view_(&scenic_session_,
            std::move(std::move(properties.view_token)),
            std::move(properties.view_ref_pair.control_ref),
            CloneViewRef(),
            "chromium window"),
      node_(&scenic_session_),
      input_node_(&scenic_session_),
      render_node_(&scenic_session_),
      bounds_(properties.bounds) {
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

  node_.AddChild(render_node_);

  delegate_->OnAcceleratedWidgetAvailable(window_id_);

  if (properties.enable_keyboard) {
    is_virtual_keyboard_enabled_ = properties.enable_virtual_keyboard;
    keyboard_service_ = base::ComponentContextForProcess()
                            ->svc()
                            ->Connect<fuchsia::ui::input3::Keyboard>();
    keyboard_service_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "input3.Keyboard service disconnected.";
    });
    keyboard_client_ = std::make_unique<KeyboardClient>(keyboard_service_.get(),
                                                        CloneViewRef(), this);
  } else {
    DCHECK(!properties.enable_virtual_keyboard);
  }
}

ScenicWindow::~ScenicWindow() {
  manager_->RemoveWindow(window_id_, this);
}

fuchsia::ui::views::ViewRef ScenicWindow::CloneViewRef() {
  fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref_.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}

gfx::Rect ScenicWindow::GetBounds() const {
  return bounds_;
}

void ScenicWindow::SetBounds(const gfx::Rect& bounds) {
  // This path should only be reached in tests.
  bounds_ = bounds;
}

void ScenicWindow::SetTitle(const std::u16string& title) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::Show(bool inactive) {
  if (is_visible_)
    return;

  is_visible_ = true;

  UpdateRootNodeVisibility();

  // Call Present2() to ensure that the scenic session commands are processed,
  // which is necessary to receive metrics event from Scenic.
  safe_presenter_.QueuePresent();
}

void ScenicWindow::Hide() {
  if (!is_visible_)
    return;

  is_visible_ = false;

  UpdateRootNodeVisibility();
}

void ScenicWindow::Close() {
  Hide();
  delegate_->OnClosed();
}

bool ScenicWindow::IsVisible() const {
  return is_visible_;
}

void ScenicWindow::PrepareForShutdown() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::SetCapture() {
  // TODO(crbug.com/1231516): Use Scenic capture APIs.
  NOTIMPLEMENTED_LOG_ONCE();
  has_capture_ = true;
}

void ScenicWindow::ReleaseCapture() {
  // TODO(crbug.com/1231516): Use Scenic capture APIs.
  NOTIMPLEMENTED_LOG_ONCE();
  has_capture_ = false;
}

bool ScenicWindow::HasCapture() const {
  // TODO(crbug.com/1231516): Use Scenic capture APIs.
  NOTIMPLEMENTED_LOG_ONCE();
  return has_capture_;
}

void ScenicWindow::ToggleFullscreen() {
  NOTIMPLEMENTED_LOG_ONCE();
  is_fullscreen_ = !is_fullscreen_;
}

void ScenicWindow::Maximize() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::Minimize() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::Restore() {
  NOTIMPLEMENTED_LOG_ONCE();
}

PlatformWindowState ScenicWindow::GetPlatformWindowState() const {
  NOTIMPLEMENTED_LOG_ONCE();
  if (is_fullscreen_)
    return PlatformWindowState::kFullScreen;
  if (!is_view_attached_)
    return PlatformWindowState::kMinimized;
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

void ScenicWindow::SetCursor(scoped_refptr<PlatformCursor> cursor) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
}

gfx::Rect ScenicWindow::GetRestoredBoundsInPixels() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

void ScenicWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                  const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ScenicWindow::SizeConstraintsChanged() {
  NOTIMPLEMENTED_LOG_ONCE();
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

  safe_presenter_.QueuePresent();
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

void ScenicWindow::OnViewAttachedChanged(bool is_view_attached) {
  PlatformWindowState old_state = GetPlatformWindowState();
  is_view_attached_ = is_view_attached;
  PlatformWindowState new_state = GetPlatformWindowState();
  if (old_state != new_state) {
    delegate_->OnWindowStateChanged(old_state, new_state);
  }
}

void ScenicWindow::OnViewMetrics(const fuchsia::ui::gfx::Metrics& metrics) {
  device_pixel_ratio_ = std::max(metrics.scale_x, metrics.scale_y);
  if (scenic_window_delegate_)
    scenic_window_delegate_->OnScenicPixelScale(this, device_pixel_ratio_);

  if (view_properties_)
    UpdateSize();
}

void ScenicWindow::OnViewProperties(
    const fuchsia::ui::gfx::ViewProperties& properties) {
  view_properties_ = properties;
  if (device_pixel_ratio_ > 0.0)
    UpdateSize();
}

void ScenicWindow::OnInputEvent(const fuchsia::ui::input::InputEvent& event) {
  if (event.is_focus()) {
    delegate_->OnActivationChanged(event.focus().focused);
  } else {
    // Scenic doesn't care if the input event was handled, so ignore the
    // "handled" status.
    std::ignore = event_dispatcher_.ProcessEvent(event);
  }
}

void ScenicWindow::UpdateSize() {
  DCHECK_GT(device_pixel_ratio_, 0.0);
  DCHECK(view_properties_);

  const float width = view_properties_->bounding_box.max.x -
                      view_properties_->bounding_box.min.x;
  const float height = view_properties_->bounding_box.max.y -
                       view_properties_->bounding_box.min.y;

  bounds_ = gfx::Rect(ceilf(width * device_pixel_ratio_),
                      ceilf(height * device_pixel_ratio_));

  // Update the root node to be shown, or hidden, based on the View state.
  // If the root node is not visible then skip resizing content, etc.
  if (!UpdateRootNodeVisibility())
    return;

  // Translate the node by half of the view dimensions to put it in the center
  // of the view.
  node_.SetTranslation(width / 2.0, height / 2.0, 0.f);

  // Scale the render node so that surface rect can always be 1x1.
  render_node_.SetScale(width, height, 1.f);

  // Resize input node to cover the whole surface.
  scenic::Rectangle window_rect(&scenic_session_, width, height);
  input_node_.SetShape(window_rect);

  // This is necessary when using vulkan because ImagePipes are presented
  // separately and we need to make sure our sizes change is committed.
  safe_presenter_.QueuePresent();

  PlatformWindowDelegate::BoundsChange bounds;
  bounds.bounds = bounds_;
  bounds.system_ui_overlap =
      ConvertInsets(device_pixel_ratio_, *view_properties_);
  delegate_->OnBoundsChanged(bounds);
}

bool ScenicWindow::UpdateRootNodeVisibility() {
  bool should_show_root_node = is_visible_ && !is_zero_sized();
  if (should_show_root_node != is_root_node_shown_) {
    is_root_node_shown_ = should_show_root_node;
    if (should_show_root_node)
      view_.AddChild(node_);
    else
      node_.Detach();
  }
  return is_root_node_shown_;
}

}  // namespace ui
