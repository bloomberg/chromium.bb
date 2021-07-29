// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/video_recording_watcher.h"

#include <memory>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor_lookup.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Recording is performed at a rate of 30 FPS. Any non-pressed/-released mouse
// events that are too frequent will be throttled. We use the frame duration as
// the minimum delay between any two successive such events that we use to
// update the cursor overlay.
constexpr base::TimeDelta kCursorEventsThrottleDelay =
    base::TimeDelta::FromHz(30);

// Window resizes can be done on many intermediate steps. This delay is used to
// throttle these resize events so that we send the final size of the window to
// the recording service when it stabilizes.
constexpr base::TimeDelta kWindowSizeChangeThrottleDelay =
    base::TimeDelta::FromMilliseconds(250);

// Returns true if |window_1| and |window_2| are both windows that belong to
// the same Desk. Note that it will return false for windows that don't belong
// to any desk (such as always-on-top windows or PIPs).
bool AreWindowsOnSameDesk(aura::Window* window_1, aura::Window* window_2) {
  auto* container_1 = desks_util::GetDeskContainerForContext(window_1);
  auto* container_2 = desks_util::GetDeskContainerForContext(window_2);
  return container_1 && container_2 && container_1 == container_2;
}

// Gets the mouse cursor location in the coordinates of the given |window|. Use
// this if a mouse event is not available.
gfx::PointF GetCursorLocationInWindow(aura::Window* window) {
  gfx::PointF cursor_point(
      display::Screen::GetScreen()->GetCursorScreenPoint());
  wm::ConvertPointFromScreen(window, &cursor_point);
  return cursor_point;
}

// Gets the location of the given mouse |event| in the coordinates of the given
// |window|.
gfx::PointF GetEventLocationInWindow(aura::Window* window,
                                     const ui::MouseEvent& event) {
  aura::Window* target = static_cast<aura::Window*>(event.target());
  gfx::PointF location = event.location_f();
  if (target != window)
    aura::Window::ConvertPointToTarget(target, window, &location);
  return location;
}

// Returns the cursor overlay bounds as defined by the documentation of the
// FrameSinkVideoCaptureOverlay. The bounds should be relative within the bounds
// of the recorded frame sink (i.e. in the range [0.f, 1.f) for both cursor
// origin and size).
gfx::RectF GetCursorOverlayBounds(
    const aura::Window* recorded_window,
    const gfx::PointF& location_in_recorded_window,
    const gfx::Point& cursor_hotspot,
    float cursor_image_scale_factor,
    const SkBitmap& cursor_bitmap) {
  DCHECK(recorded_window);
  DCHECK_GT(cursor_image_scale_factor, 0);

  // The video size, and the resolution constraints will be matching the size of
  // the recorded window (whether a root or a non-root window). Hence, the
  // bounds of the cursor overlay should be relative to that size.
  const auto window_size = recorded_window->bounds().size();
  if (window_size.IsEmpty())
    return gfx::RectF();

  const gfx::PointF cursor_hotspot_dip =
      gfx::ConvertPointToDips(cursor_hotspot, cursor_image_scale_factor);
  const gfx::SizeF cursor_size_dip = gfx::ConvertSizeToDips(
      gfx::SizeF(cursor_bitmap.width(), cursor_bitmap.height()),
      cursor_image_scale_factor);
  gfx::RectF cursor_relative_bounds(
      location_in_recorded_window - cursor_hotspot_dip.OffsetFromOrigin(),
      cursor_size_dip);
  cursor_relative_bounds.Scale(1.f / window_size.width(),
                               1.f / window_size.height());
  return cursor_relative_bounds;
}

}  // namespace

// -----------------------------------------------------------------------------
// RecordedWindowRootObserver:

// Defines an observer to observe the hierarchy changes of the root window under
// which the recorded window resides. This is only constructed when performing a
// window recording type.
class RecordedWindowRootObserver : public aura::WindowObserver {
 public:
  RecordedWindowRootObserver(aura::Window* root, VideoRecordingWatcher* owner)
      : root_(root), owner_(owner) {
    DCHECK(root_);
    DCHECK(owner_);
    DCHECK(root_->IsRootWindow());
    DCHECK_EQ(owner_->recording_source_, CaptureModeSource::kWindow);

    root_->AddObserver(this);
  }
  RecordedWindowRootObserver(const RecordedWindowRootObserver&) = delete;
  RecordedWindowRootObserver& operator=(const RecordedWindowRootObserver&) =
      delete;
  ~RecordedWindowRootObserver() override { root_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override {
    DCHECK_EQ(params.receiver, root_);
    owner_->OnRootHierarchyChanged(params.target);
  }

  void OnWindowDestroying(aura::Window* window) override {
    // We should never get here, as the recorded window gets moved to a
    // different display before the root of another is destroyed. So this root
    // observer should have been destroyed already.
    NOTREACHED();
  }

 private:
  aura::Window* const root_;
  VideoRecordingWatcher* const owner_;
};

// -----------------------------------------------------------------------------
//  VideoRecordingWatcher:

VideoRecordingWatcher::VideoRecordingWatcher(
    CaptureModeController* controller,
    aura::Window* window_being_recorded,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCaptureOverlay>
        cursor_capture_overlay)
    : controller_(controller),
      cursor_manager_(Shell::Get()->cursor_manager()),
      window_being_recorded_(window_being_recorded),
      current_root_(window_being_recorded->GetRootWindow()),
      recording_source_(controller_->source()),
      cursor_capture_overlay_remote_(std::move(cursor_capture_overlay)) {
  DCHECK(controller_);
  DCHECK(window_being_recorded_);
  DCHECK(current_root_);
  DCHECK(controller_->is_recording_in_progress());

  if (!window_being_recorded_->IsRootWindow()) {
    DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);
    non_root_window_capture_request_ =
        window_being_recorded_->MakeWindowCapturable();
    root_observer_ =
        std::make_unique<RecordedWindowRootObserver>(current_root_, this);
    Shell::Get()->activation_client()->AddObserver(this);
  } else {
    // We only need to observe the changes in the state of the software-
    // composited cursor when recording a root window (i.e. fullscreen or
    // partial region capture), since the software cursor is in the layer
    // subtree of the root window, and will be captured by the frame sink video
    // capturer automatically without the need for the cursor overlay. In this
    // case we need to avoid producing a video with two overlapping cursors.
    // When recording a window however, the software cursor is not in its layer
    // subtree, and has to always be captured using the cursor overlay.
    auto* cursor_window_controller =
        Shell::Get()->window_tree_host_manager()->cursor_window_controller();
    // Note that the software cursor might have already been enabled prior to
    // the recording starting.
    force_cursor_overlay_hidden_ =
        cursor_window_controller->is_cursor_compositing_enabled();
    cursor_window_controller->AddObserver(this);
  }

  if (recording_source_ == CaptureModeSource::kRegion)
    partial_region_bounds_ = controller_->user_capture_region();

  window_being_recorded_->AddObserver(this);
  TabletModeController::Get()->AddObserver(this);

  // Note the following:
  // 1- We add |this| as a pre-target handler of the |window_being_recorded_| as
  //    opposed to |Env|. This ensures that we only get mouse events when the
  //    window being recorded is the target. This is more efficient since we
  //    won't get any event when the curosr is in a different display, or
  //    targeting a different window.
  // 2- We use the |kAccessibility| priority to ensure that we get these events
  //    before other pre-target handlers can consume them (e.g. when opening a
  //    capture mode session to take a screenshot while recording a video).
  window_being_recorded_->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kAccessibility);
}

VideoRecordingWatcher::~VideoRecordingWatcher() {
  DCHECK(window_being_recorded_);

  window_being_recorded_->RemovePreTargetHandler(this);
  TabletModeController::Get()->RemoveObserver(this);
  if (recording_source_ == CaptureModeSource::kWindow) {
    Shell::Get()->activation_client()->RemoveObserver(this);
  } else {
    Shell::Get()
        ->window_tree_host_manager()
        ->cursor_window_controller()
        ->RemoveObserver(this);
  }
  window_being_recorded_->RemoveObserver(this);
}

void VideoRecordingWatcher::OnWindowParentChanged(aura::Window* window,
                                                  aura::Window* parent) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  UpdateLayerStackingAndDimmers();
}

void VideoRecordingWatcher::OnWindowVisibilityChanged(aura::Window* window,
                                                      bool visible) {
  if (window == window_being_recorded_)
    UpdateShouldPaintLayer();
}

void VideoRecordingWatcher::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK(controller_->is_recording_in_progress());

  if (recording_source_ != CaptureModeSource::kWindow)
    return;

  // We care only about size changes, since the location of the window won't
  // affect the recorded video frames of it, however, the size of the window
  // affects the size of the frames.
  if (old_bounds.size() == new_bounds.size())
    return;

  window_size_change_throttle_timer_.Start(
      FROM_HERE, kWindowSizeChangeThrottleDelay, this,
      &VideoRecordingWatcher::OnWindowSizeChangeThrottleTimerFiring);
}

void VideoRecordingWatcher::OnWindowOpacitySet(
    aura::Window* window,
    ui::PropertyChangeReason reason) {
  if (window == window_being_recorded_)
    UpdateShouldPaintLayer();
}

void VideoRecordingWatcher::OnWindowStackingChanged(aura::Window* window) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  UpdateLayerStackingAndDimmers();
}

void VideoRecordingWatcher::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());

  // EndVideoRecording() destroys |this|. No need to remove observer here, since
  // it will be done in the destructor.
  controller_->EndVideoRecording(EndRecordingReason::kDisplayOrWindowClosing);
}

void VideoRecordingWatcher::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, window_being_recorded_);

  // We should never get here, since OnWindowDestroying() calls
  // EndVideoRecording() which deletes us.
  NOTREACHED();
}

void VideoRecordingWatcher::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  DCHECK_EQ(window, window_being_recorded_);
  DCHECK(controller_->is_recording_in_progress());
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  root_observer_.reset();
  current_root_ = new_root;

  if (!new_root) {
    // EndVideoRecording() destroys |this|.
    controller_->EndVideoRecording(EndRecordingReason::kDisplayOrWindowClosing);
    return;
  }

  root_observer_ =
      std::make_unique<RecordedWindowRootObserver>(current_root_, this);
  controller_->OnRecordedWindowChangingRoot(window_being_recorded_, new_root);
}

void VideoRecordingWatcher::OnPaintLayer(const ui::PaintContext& context) {
  if (!should_paint_layer_)
    return;

  DCHECK_NE(recording_source_, CaptureModeSource::kFullscreen);

  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Canvas* canvas = recorder.canvas();
  auto* color_provider = AshColorProvider::Get();
  const SkColor dimming_color = color_provider->GetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield40);
  canvas->DrawColor(dimming_color);

  // We don't draw a region border around the recorded window. We just paint the
  // above shield as a backdrop.
  if (recording_source_ == CaptureModeSource::kWindow)
    return;

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  gfx::Rect region = gfx::ScaleToEnclosingRect(partial_region_bounds_, dsf);
  region.Inset(-capture_mode::kCaptureRegionBorderStrokePx,
               -capture_mode::kCaptureRegionBorderStrokePx);
  canvas->FillRect(region, SK_ColorTRANSPARENT, SkBlendMode::kClear);

  // Draw the region border.
  cc::PaintFlags border_flags;
  border_flags.setColor(capture_mode::kRegionBorderColor);
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setStrokeWidth(capture_mode::kCaptureRegionBorderStrokePx);
  canvas->DrawRect(gfx::RectF(region), border_flags);
}

void VideoRecordingWatcher::OnWindowActivated(ActivationReason reason,
                                              aura::Window* gained_active,
                                              aura::Window* lost_active) {
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  UpdateLayerStackingAndDimmers();
}

void VideoRecordingWatcher::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (recording_source_ == CaptureModeSource::kFullscreen)
    return;

  if (!(metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
                   DISPLAY_METRIC_DEVICE_SCALE_FACTOR))) {
    return;
  }

  const int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(current_root_).id();
  if (display_id != display.id())
    return;

  const auto& root_bounds = current_root_->bounds();
  controller_->PushNewRootSizeToRecordingService(root_bounds.size());

  DCHECK(layer());
  layer()->SetBounds(root_bounds);
}

void VideoRecordingWatcher::OnDimmedWindowDestroying(
    aura::Window* dimmed_window) {
  dimmers_.erase(dimmed_window);
}

void VideoRecordingWatcher::OnDimmedWindowParentChanged(
    aura::Window* dimmed_window) {
  // If the dimmed window moves to another display or a different desk, we no
  // longer dim it.
  if (dimmed_window->GetRootWindow() != current_root_ ||
      !AreWindowsOnSameDesk(dimmed_window, window_being_recorded_)) {
    dimmers_.erase(dimmed_window);
  }
}

void VideoRecordingWatcher::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSEWHEEL:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      return;

    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
      // Pressed/released events are important, so we handle them immediately.
      UpdateCursorOverlayNow(
          GetEventLocationInWindow(window_being_recorded_, *event));
      return;

    default:
      UpdateOrThrottleCursorOverlay(
          GetEventLocationInWindow(window_being_recorded_, *event));
  }
}

void VideoRecordingWatcher::OnTabletModeStarted() {
  UpdateCursorOverlayNow(gfx::PointF());
}

void VideoRecordingWatcher::OnTabletModeEnded() {
  UpdateCursorOverlayNow(GetCursorLocationInWindow(window_being_recorded_));
}

void VideoRecordingWatcher::OnCursorCompositingStateChanged(bool enabled) {
  DCHECK_NE(recording_source_, CaptureModeSource::kWindow);
  force_cursor_overlay_hidden_ = enabled;
  UpdateCursorOverlayNow(
      force_cursor_overlay_hidden_
          ? gfx::PointF()
          : GetCursorLocationInWindow(window_being_recorded_));
}

bool VideoRecordingWatcher::IsWindowDimmedForTesting(
    aura::Window* window) const {
  return dimmers_.contains(window);
}

void VideoRecordingWatcher::BindCursorOverlayForTesting(
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCaptureOverlay> overlay) {
  cursor_capture_overlay_remote_.reset();
  cursor_capture_overlay_remote_.Bind(std::move(overlay));
}

void VideoRecordingWatcher::FlushCursorOverlayForTesting() {
  cursor_capture_overlay_remote_.FlushForTesting();
}

void VideoRecordingWatcher::SendThrottledWindowSizeChangedNowForTesting() {
  window_size_change_throttle_timer_.FireNow();
}

void VideoRecordingWatcher::SetLayer(std::unique_ptr<ui::Layer> layer) {
  if (layer) {
    layer->set_delegate(this);
    layer->SetName("Recording Shield");
  }
  LayerOwner::SetLayer(std::move(layer));
  UpdateShouldPaintLayer();
  UpdateLayerStackingAndDimmers();
}

void VideoRecordingWatcher::OnRootHierarchyChanged(aura::Window* target) {
  DCHECK(controller_->is_recording_in_progress());
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  if (target != window_being_recorded_ && !dimmers_.contains(target) &&
      CanIncludeWindowInMruList(target)) {
    UpdateLayerStackingAndDimmers();
  }
}

bool VideoRecordingWatcher::CalculateShouldPaintLayer() const {
  if (recording_source_ == CaptureModeSource::kFullscreen)
    return false;

  if (recording_source_ == CaptureModeSource::kRegion)
    return true;

  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);
  return window_being_recorded_->TargetVisibility() &&
         window_being_recorded_->layer()->GetTargetVisibility() &&
         window_being_recorded_->layer()->GetTargetOpacity() > 0;
}

void VideoRecordingWatcher::UpdateShouldPaintLayer() {
  const bool new_value = CalculateShouldPaintLayer();
  if (new_value == should_paint_layer_)
    return;

  should_paint_layer_ = new_value;
  if (!should_paint_layer_) {
    // If we're not painting the shield, we don't need the individual dimmers
    // either.
    dimmers_.clear();
  }

  if (layer())
    layer()->SchedulePaint(layer()->bounds());
}

void VideoRecordingWatcher::UpdateLayerStackingAndDimmers() {
  if (!layer())
    return;

  DCHECK_NE(recording_source_, CaptureModeSource::kFullscreen);

  const bool is_recording_window =
      recording_source_ == CaptureModeSource::kWindow;

  ui::Layer* new_parent =
      is_recording_window
          ? window_being_recorded_->layer()->parent()
          : current_root_->GetChildById(kShellWindowId_OverlayContainer)
                ->layer();
  ui::Layer* old_parent = layer()->parent();
  DCHECK(new_parent || is_recording_window);

  if (!new_parent && old_parent) {
    // If the window gets removed from the hierarchy, we remove the shield layer
    // too, as well as any dimming of windows we have.
    old_parent->Remove(layer());
    dimmers_.clear();
    return;
  }

  if (new_parent != old_parent) {
    // We get here the first time we parent the shield layer under the overlay
    // container when we're recording a partial region, or when recording a
    // window, and it gets moved to another display, or moved to a different
    // desk.
    new_parent->Add(layer());
    layer()->SetBounds(current_root_->bounds());
  }

  // When recording a partial region, the shield layer is stacked at the top of
  // everything in the overlay container.
  if (!is_recording_window) {
    new_parent->StackAtTop(layer());
    return;
  }

  // However, when recording a window, we stack the shield layer below the
  // recorded window's layer. This takes care of dimming any windows below the
  // recorded window in the z-order.
  new_parent->StackBelow(layer(), window_being_recorded_->layer());

  // If the shield is not painted, all the individual dimmers should be removed.
  if (!should_paint_layer_) {
    dimmers_.clear();
    return;
  }

  // For windows that are above the recorded window in the z-order and on the
  // same display, they're dimmed separately.
  const SkColor dimming_color = AshColorProvider::Get()->GetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield40);
  // We use |kAllDesks| here for the following reasons:
  // 1- A dimmed window can move out from the desk where the window being
  //    recorded is (either by keyboard shortcut or drag and drop in overview).
  // 2- The recorded window itself can move out from the active desk to an
  //    inactive desk that has other windows.
  // In (1), we want to remove the dimmers of those windows that moved out.
  // In (2), we want to remove the dimmers of the window on the active desk, and
  // create ones for the windows in the inactive desk (if any of them is above
  // the recorded window).
  const auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          DesksMruType::kAllDesks);
  bool did_find_recorded_window = false;
  // Note that the order of |mru_windows| are from top-most first.
  for (auto* window : mru_windows) {
    if (window == window_being_recorded_) {
      did_find_recorded_window = true;
      continue;
    }

    // No need to dim windows that are below the window being recorded in
    // z-order, or those on other displays, or other desks.
    if (did_find_recorded_window || window->GetRootWindow() != current_root_ ||
        !AreWindowsOnSameDesk(window, window_being_recorded_)) {
      dimmers_.erase(window);
      continue;
    }

    auto& dimmer = dimmers_[window];
    if (!dimmer) {
      dimmer = std::make_unique<WindowDimmer>(window, /*animate=*/false, this);
      dimmer->SetDimColor(dimming_color);
      dimmer->window()->Show();
    }
  }
}

gfx::NativeCursor VideoRecordingWatcher::GetCurrentCursor() const {
  const auto cursor = cursor_manager_->GetCursor();
  // See the documentation in cursor_type.mojom. |kNull| is treated exactly as
  // |kPointer|.
  return (cursor.type() == ui::mojom::CursorType::kNull)
             ? gfx::NativeCursor(ui::mojom::CursorType::kPointer)
             : cursor;
}

void VideoRecordingWatcher::UpdateOrThrottleCursorOverlay(
    const gfx::PointF& location) {
  if (cursor_events_throttle_timer_.IsRunning()) {
    throttled_cursor_location_ = location;
    return;
  }

  UpdateCursorOverlayNow(location);
  cursor_events_throttle_timer_.Start(
      FROM_HERE, kCursorEventsThrottleDelay, this,
      &VideoRecordingWatcher::OnCursorThrottleTimerFiring);
}

void VideoRecordingWatcher::UpdateCursorOverlayNow(
    const gfx::PointF& location) {
  // Cancel any pending throttled event.
  cursor_events_throttle_timer_.Stop();
  throttled_cursor_location_.reset();

  if (!cursor_capture_overlay_remote_)
    return;

  if (force_cursor_overlay_hidden_ ||
      TabletModeController::Get()->InTabletMode()) {
    HideCursorOverlay();
    return;
  }

  const gfx::RectF window_local_bounds(
      gfx::SizeF(window_being_recorded_->bounds().size()));
  if (!window_local_bounds.Contains(location)) {
    HideCursorOverlay();
    return;
  }

  const gfx::NativeCursor cursor = GetCurrentCursor();
  DCHECK_NE(cursor.type(), ui::mojom::CursorType::kNull);

  const float cursor_image_scale_factor = cursor.image_scale_factor();
  const SkBitmap cursor_image = ui::GetCursorBitmap(cursor);
  const gfx::RectF cursor_overlay_bounds = GetCursorOverlayBounds(
      window_being_recorded_, location, ui::GetCursorHotspot(cursor),
      cursor_image_scale_factor, cursor_image);

  if (cursor != last_cursor_) {
    if (cursor_image.drawsNothing()) {
      last_cursor_ = gfx::NativeCursor();
      HideCursorOverlay();
      return;
    }

    last_cursor_ = cursor;
    last_cursor_overlay_bounds_ = cursor_overlay_bounds;
    cursor_capture_overlay_remote_->SetImageAndBounds(
        cursor_image, last_cursor_overlay_bounds_);
    return;
  }

  if (last_cursor_overlay_bounds_ == cursor_overlay_bounds)
    return;

  last_cursor_overlay_bounds_ = cursor_overlay_bounds;
  cursor_capture_overlay_remote_->SetBounds(last_cursor_overlay_bounds_);
}

void VideoRecordingWatcher::HideCursorOverlay() {
  DCHECK(cursor_capture_overlay_remote_);

  // No need to rehide if already hidden.
  if (last_cursor_overlay_bounds_ == gfx::RectF())
    return;

  last_cursor_overlay_bounds_ = gfx::RectF();
  cursor_capture_overlay_remote_->SetBounds(last_cursor_overlay_bounds_);
}

void VideoRecordingWatcher::OnCursorThrottleTimerFiring() {
  if (throttled_cursor_location_)
    UpdateCursorOverlayNow(*throttled_cursor_location_);
}

void VideoRecordingWatcher::OnWindowSizeChangeThrottleTimerFiring() {
  DCHECK(controller_->is_recording_in_progress());
  DCHECK_EQ(recording_source_, CaptureModeSource::kWindow);

  controller_->OnRecordedWindowSizeChanged(
      window_being_recorded_->bounds().size());
}

}  // namespace ash
