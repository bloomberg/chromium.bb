// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_session.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/capture_window_observer.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/magnifier/magnifier_glass.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_dimmer.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "cc/paint/paint_flags.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The visual radius of the drag affordance circles which are shown while
// resizing a drag region.
constexpr int kAffordanceCircleRadiusDp = 4;

// The hit radius of the drag affordance circles touch events.
constexpr int kAffordanceCircleTouchHitRadiusDp = 16;

// Capture region magnifier parameters.
constexpr MagnifierGlass::Params kMagnifierParams{
    /*scale=*/2.f,
    /*radius=*/60,
    /*border_size=*/2,
    /*border_outline_thickness=*/0,
    /*border_color=*/SK_ColorWHITE,
    /*border_outline_color=*/SK_ColorTRANSPARENT,
    /*bottom_shadow=*/
    gfx::ShadowValue(gfx::Vector2d(0, 1),
                     2,
                     SkColorSetARGB(0x4C, 0x00, 0x00, 0x00)),
    /*top_shadow=*/
    gfx::ShadowValue(gfx::Vector2d(0, 1),
                     3,
                     SkColorSetARGB(0x26, 0x00, 0x00, 0x00))};

constexpr int kSizeLabelBorderRadius = 4;

constexpr int kSizeLabelHorizontalPadding = 8;

// Blue300 at 30%.
constexpr SkColor kCaptureRegionColor = SkColorSetA(gfx::kGoogleBlue300, 77);

// Values for the shadows of the capture region components.
constexpr int kRegionAffordanceCircleShadow2Blur = 6;
constexpr gfx::ShadowValue kRegionOutlineShadow(gfx::Vector2d(0, 0),
                                                2,
                                                SkColorSetARGB(41, 0, 0, 0));
constexpr gfx::ShadowValue kRegionAffordanceCircleShadow1(
    gfx::Vector2d(0, 1),
    2,
    SkColorSetARGB(76, 0, 0, 0));
constexpr gfx::ShadowValue kRegionAffordanceCircleShadow2(
    gfx::Vector2d(0, 2),
    kRegionAffordanceCircleShadow2Blur,
    SkColorSetARGB(38, 0, 0, 0));

// Values of the focus ring draw around the region or affordance circles.
constexpr int kFocusRingStrokeWidthDp = 2;
constexpr int kFocusRingSpacingDp = 2;

// When updating the capture region, request a repaint on the region and inset
// such that the border, affordance circles and affordance circle shadows are
// all repainted as well.
constexpr int kDamageInsetDp = capture_mode::kCaptureRegionBorderStrokePx +
                               kAffordanceCircleRadiusDp +
                               kRegionAffordanceCircleShadow2Blur;

// The minimum padding on each side of the capture region. If the capture button
// cannot be placed in the center of the capture region and maintain this
// padding, it will be placed below or above the capture region.
constexpr int kCaptureRegionMinimumPaddingDp = 16;

// Animation parameters needed when countdown starts.
// The animation duration that the label fades out and scales down before count
// down starts.
constexpr base::TimeDelta kCaptureLabelAnimationDuration =
    base::TimeDelta::FromMilliseconds(267);
// The animation duration that the capture bar fades out before count down
// starts.
constexpr base::TimeDelta kCaptureBarFadeOutDuration =
    base::TimeDelta::FromMilliseconds(167);
// The animation duration that the fullscreen shield fades out before count down
// starts.
constexpr base::TimeDelta kCaptureShieldFadeOutDuration =
    base::TimeDelta::FromMilliseconds(333);
// If there is no text message was showing when count down starts, the label
// widget will shrink down from 120% -> 100% and fade in.
constexpr float kLabelScaleUpOnCountdown = 1.2;

// Animation parameters for capture bar overlapping the user capture region.
// The default animation duration for opacity changes to the capture bar.
constexpr base::TimeDelta kCaptureBarOpacityChangeDuration =
    base::TimeDelta::FromMilliseconds(100);
// The animation duration for showing the capture bar on mouse/touch release.
constexpr base::TimeDelta kCaptureBarOnReleaseOpacityChangeDuration =
    base::TimeDelta::FromMilliseconds(167);
// When the capture bar and user capture region overlap and the mouse is not
// hovering over the capture bar, drop the opacity to this value to make the
// region easier to see.
constexpr float kCaptureBarOverlapOpacity = 0.1;

// If the user is using keyboard only and they are on the selecting region
// phase, they can create default region which is centered and sized to this
// value times the root window's width and height.
constexpr float kRegionDefaultRatio = 0.24f;

// Mouse cursor warping is disabled when the capture source is a custom region.
// Sets the mouse warp status to |enable| and return the original value.
bool SetMouseWarpEnabled(bool enable) {
  auto* mouse_cursor_filter = Shell::Get()->mouse_cursor_filter();
  const bool old_value = mouse_cursor_filter->mouse_warp_enabled();
  mouse_cursor_filter->set_mouse_warp_enabled(enable);
  return old_value;
}

// Gets the overlay container inside |root|.
aura::Window* GetParentContainer(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());
  return root->GetChildById(kShellWindowId_MenuContainer);
}

// Returns the smallest rect that contains all of |points|.
gfx::Rect GetRectEnclosingPoints(const std::vector<gfx::Point>& points) {
  DCHECK_GE(points.size(), 2u);

  int x = INT_MAX;
  int y = INT_MAX;
  int right = INT_MIN;
  int bottom = INT_MIN;
  for (const gfx::Point& point : points) {
    x = std::min(point.x(), x);
    y = std::min(point.y(), y);
    right = std::max(point.x(), right);
    bottom = std::max(point.y(), bottom);
  }
  return gfx::Rect(x, y, right - x, bottom - y);
}

// Returns the widget init params needed to create a widget associated with a
// capture session.
views::Widget::InitParams CreateWidgetParams(aura::Window* parent,
                                             const gfx::Rect& bounds,
                                             const std::string& name) {
  // Use a popup widget to get transient properties, such as not needing to
  // click on the widget first to get capture before receiving events.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent;
  params.bounds = bounds;
  params.name = name;
  return params;
}

// Gets the root window associated |location_in_screen| if given, otherwise gets
// the root window associated with the CursorManager.
aura::Window* GetPreferredRootWindow(
    base::Optional<gfx::Point> location_in_screen = base::nullopt) {
  int64_t display_id =
      (location_in_screen
           ? display::Screen::GetScreen()->GetDisplayNearestPoint(
                 *location_in_screen)
           : Shell::Get()->cursor_manager()->GetDisplay())
          .id();

  // The Display object returned by CursorManager::GetDisplay may be stale, but
  // will have the correct id.
  DCHECK_NE(display::kInvalidDisplayId, display_id);
  return Shell::GetRootWindowForDisplayId(display_id);
}

// In fullscreen or window capture mode, the mouse will change to a camera
// image icon if we're capturing image, or a video record image icon if we're
// capturing video.
ui::Cursor GetCursorForFullscreenOrWindowCapture(bool capture_image) {
  ui::Cursor cursor(ui::mojom::CursorType::kCustom);
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          GetPreferredRootWindow());
  const float device_scale_factor = display.device_scale_factor();
  // TODO: Adjust the icon color after spec is updated.
  const gfx::ImageSkia icon = gfx::CreateVectorIcon(
      capture_image ? kCaptureModeImageIcon : kCaptureModeVideoIcon,
      SK_ColorBLACK);
  SkBitmap bitmap = *icon.bitmap();
  gfx::Point hotspot(bitmap.width() / 2, bitmap.height() / 2);
  ui::ScaleAndRotateCursorBitmapAndHotpoint(
      device_scale_factor, display.panel_rotation(), &bitmap, &hotspot);
  auto* cursor_factory = ui::CursorFactory::GetInstance();
  ui::PlatformCursor platform_cursor =
      cursor_factory->CreateImageCursor(cursor.type(), bitmap, hotspot);
  cursor.SetPlatformCursor(platform_cursor);
  cursor.set_custom_bitmap(bitmap);
  cursor.set_custom_hotspot(hotspot);
  cursor_factory->UnrefImageCursor(platform_cursor);

  return cursor;
}

// Returns the expected cursor type for |position| in region capture.
ui::mojom::CursorType GetCursorTypeForFineTunePosition(
    FineTunePosition position) {
  switch (position) {
    case FineTunePosition::kTopLeft:
      return ui::mojom::CursorType::kNorthWestResize;
    case FineTunePosition::kBottomRight:
      return ui::mojom::CursorType::kSouthEastResize;
    case FineTunePosition::kTopCenter:
    case FineTunePosition::kBottomCenter:
      return ui::mojom::CursorType::kNorthSouthResize;
    case FineTunePosition::kTopRight:
      return ui::mojom::CursorType::kNorthEastResize;
    case FineTunePosition::kBottomLeft:
      return ui::mojom::CursorType::kSouthWestResize;
    case FineTunePosition::kLeftCenter:
    case FineTunePosition::kRightCenter:
      return ui::mojom::CursorType::kEastWestResize;
    case FineTunePosition::kCenter:
      return ui::mojom::CursorType::kMove;
    default:
      return ui::mojom::CursorType::kCell;
  }
}

// Gets the amount of change that should happen to a region given |event_flags|.
int GetArrowKeyPressChange(int event_flags) {
  if ((event_flags & ui::EF_SHIFT_DOWN) != 0)
    return capture_mode::kShiftArrowKeyboardRegionChangeDp;
  if ((event_flags & ui::EF_CONTROL_DOWN) != 0)
    return capture_mode::kCtrlArrowKeyboardRegionChangeDp;
  return capture_mode::kArrowKeyboardRegionChangeDp;
}

// Clips |out_bounds| to fit |rect|. Similar to
// gfx::Rect::AdjustToFit() but does not shift the output rect to maintain the
// rect size.
void ClipRectToFit(gfx::Rect* out_bounds, const gfx::Rect& rect) {
  out_bounds->SetByBounds(std::max(rect.x(), out_bounds->x()),
                          std::max(rect.y(), out_bounds->y()),
                          std::min(rect.right(), out_bounds->right()),
                          std::min(rect.bottom(), out_bounds->bottom()));
}

// Returns the appropriate |message_id| for a chromevox alert.
// |for_toggle_alert| helps differentiate between the session start and when a
// user toggles the source.
int GetMessageIdForCaptureSource(CaptureModeSource source,
                                 bool for_toggle_alert) {
  switch (source) {
    case CaptureModeSource::kFullscreen:
      return for_toggle_alert
                 ? IDS_ASH_SCREEN_CAPTURE_ALERT_SELECT_SOURCE_FULLSCREEN
                 : IDS_ASH_SCREEN_CAPTURE_SOURCE_FULLSCREEN;
    case CaptureModeSource::kRegion:
      return for_toggle_alert
                 ? IDS_ASH_SCREEN_CAPTURE_ALERT_SELECT_SOURCE_REGION
                 : IDS_ASH_SCREEN_CAPTURE_SOURCE_PARTIAL;
    default:
      return for_toggle_alert
                 ? IDS_ASH_SCREEN_CAPTURE_ALERT_SELECT_SOURCE_WINDOW
                 : IDS_ASH_SCREEN_CAPTURE_SOURCE_WINDOW;
  }
}

void UpdateAutoclickMenuBoundsIfNeeded() {
  Shell::Get()->accessibility_controller()->UpdateAutoclickMenuBoundsIfNeeded();
}

}  // namespace

// -----------------------------------------------------------------------------
// CaptureModeSession::CursorSetter:

class CaptureModeSession::CursorSetter {
 public:
  CursorSetter()
      : cursor_manager_(Shell::Get()->cursor_manager()),
        original_cursor_(cursor_manager_->GetCursor()),
        original_cursor_visible_(cursor_manager_->IsCursorVisible()),
        original_cursor_locked_(cursor_manager_->IsCursorLocked()),
        current_orientation_(GetCurrentScreenOrientation()) {}

  CursorSetter(const CursorSetter&) = delete;
  CursorSetter& operator=(const CursorSetter&) = delete;

  ~CursorSetter() { ResetCursor(); }

  // Note that this will always make the cursor visible if it is not |kNone|.
  void UpdateCursor(const ui::Cursor& cursor) {
    if (original_cursor_locked_)
      return;

    if (in_cursor_update_)
      return;

    base::AutoReset<bool> auto_reset_in_cursor_update(&in_cursor_update_, true);
    const ui::mojom::CursorType current_cursor_type =
        cursor_manager_->GetCursor().type();
    const ui::mojom::CursorType new_cursor_type = cursor.type();
    const CaptureModeType capture_type = CaptureModeController::Get()->type();
    const float device_scale_factor =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(GetPreferredRootWindow())
            .device_scale_factor();

    // For custom cursors, update the cursor if we need to change between image
    // capture and video capture, if the device scale factor changes, or if the
    // screen orientation changes.
    const OrientationLockType orientation = GetCurrentScreenOrientation();
    const bool is_cursor_changed =
        current_cursor_type != new_cursor_type ||
        (current_cursor_type == ui::mojom::CursorType::kCustom &&
         (custom_cursor_capture_type_ != capture_type ||
          custom_cursor_device_scale_factor_ != device_scale_factor ||
          current_orientation_ != orientation));
    const bool is_cursor_visibility_changed =
        cursor_manager_->IsCursorVisible() !=
        (new_cursor_type != ui::mojom::CursorType::kNone);
    if (new_cursor_type == ui::mojom::CursorType::kCustom) {
      custom_cursor_capture_type_ = capture_type;
      custom_cursor_device_scale_factor_ = device_scale_factor;
    }

    current_orientation_ = orientation;

    if (!is_cursor_changed && !is_cursor_visibility_changed)
      return;

    if (cursor_manager_->IsCursorLocked())
      cursor_manager_->UnlockCursor();
    if (new_cursor_type == ui::mojom::CursorType::kNone) {
      cursor_manager_->HideCursor();
    } else {
      cursor_manager_->SetCursor(cursor);
      cursor_manager_->ShowCursor();
    }
    cursor_manager_->LockCursor();
    was_cursor_reset_to_original_ = false;
  }

  // Resets to its original cursor.
  void ResetCursor() {
    // Only unlock the cursor if it wasn't locked before.
    if (original_cursor_locked_)
      return;

    // Only reset cursor if it hasn't been reset before.
    if (was_cursor_reset_to_original_)
      return;

    if (cursor_manager_->IsCursorLocked())
      cursor_manager_->UnlockCursor();
    cursor_manager_->SetCursor(original_cursor_);
    if (original_cursor_visible_)
      cursor_manager_->ShowCursor();
    else
      cursor_manager_->HideCursor();
    was_cursor_reset_to_original_ = true;
  }

  bool IsCursorVisible() const { return cursor_manager_->IsCursorVisible(); }

  void HideCursor() {
    if (original_cursor_locked_ || !IsCursorVisible())
      return;

    if (cursor_manager_->IsCursorLocked())
      cursor_manager_->UnlockCursor();
    cursor_manager_->HideCursor();
    cursor_manager_->LockCursor();
    was_cursor_reset_to_original_ = false;
  }

  bool IsUsingCustomCursor(CaptureModeType type) const {
    return cursor_manager_->GetCursor().type() ==
               ui::mojom::CursorType::kCustom &&
           custom_cursor_capture_type_ == type;
  }

 private:
  wm::CursorManager* const cursor_manager_;
  const gfx::NativeCursor original_cursor_;
  const bool original_cursor_visible_;

  // If the original cursor is already locked, don't make any changes to it.
  const bool original_cursor_locked_;

  // The current custom cursor type. kImage if we're using image capture icon as
  // the mouse cursor, and kVideo if we're using video record icon as the mouse
  // cursor.
  CaptureModeType custom_cursor_capture_type_ = CaptureModeType::kImage;

  // Records the current device scale factor. If the DSF changes, we will need
  // to update the cursor if we're using a custom cursor.
  float custom_cursor_device_scale_factor_ = 1.f;

  // Records the current screen orientation. If screen orientation changes, we
  // will need to update the cursor if we're using custom cursor.
  OrientationLockType current_orientation_;

  // True if the cursor has reset back to its original cursor. It's to prevent
  // Reset() from setting the cursor to |original_cursor_| more than once.
  bool was_cursor_reset_to_original_ = true;

  // True if the cursor is currently being updated. This is to prevent
  // UpdateCursor() is called nestly more than once and the mouse is locked
  // multiple times.
  bool in_cursor_update_ = false;
};

// -----------------------------------------------------------------------------
// CaptureModeSession::ScopedA11yOverrideWindowSetter:

// Scoped class that sets the capture mode bar widget window as the window for
// accessibility focus for the duration of a capture mode session. Clears the
// accessibility focus window when destructed.
class CaptureModeSession::ScopedA11yOverrideWindowSetter
    : public aura::WindowObserver {
 public:
  explicit ScopedA11yOverrideWindowSetter(aura::Window* a11y_focus_window) {
    SetA11yOverrideWindow(a11y_focus_window);
  }
  ScopedA11yOverrideWindowSetter(const ScopedA11yOverrideWindowSetter&) =
      delete;
  ScopedA11yOverrideWindowSetter& operator=(
      const ScopedA11yOverrideWindowSetter&) = delete;
  ~ScopedA11yOverrideWindowSetter() override { SetA11yOverrideWindow(nullptr); }

 private:
  // Sets a window as the a11y override window. Accessiblity features will check
  // for a a11y override window to focus before getting the window with actual
  // focus.
  void SetA11yOverrideWindow(aura::Window* a11y_override_window) {
    Shell::Get()->accessibility_controller()->SetA11yOverrideWindow(
        a11y_override_window);
  }
};

// -----------------------------------------------------------------------------
// CaptureModeSession:

CaptureModeSession::CaptureModeSession(CaptureModeController* controller)
    : controller_(controller),
      current_root_(GetPreferredRootWindow()),
      magnifier_glass_(kMagnifierParams),
      cursor_setter_(std::make_unique<CursorSetter>()),
      focus_cycler_(std::make_unique<CaptureModeSessionFocusCycler>(this)) {}

CaptureModeSession::~CaptureModeSession() = default;

void CaptureModeSession::Initialize() {
  // A context menu may have input capture when entering a session. Remove
  // capture from it, otherwise subsequent mouse events will cause it to close,
  // and then we won't be able to take a screenshot of the menu. Store it so we
  // can return capture to it when exiting the session.
  // Note that some windows gets destroyed when they lose the capture (e.g. a
  // window created for capturing events while drag-drop in progress), so we
  // need to account for that.
  auto* capture_client = aura::client::GetCaptureClient(current_root_);
  input_capture_window_ = capture_client->GetCaptureWindow();
  if (input_capture_window_) {
    aura::WindowTracker tracker({input_capture_window_});
    capture_client->ReleaseCapture(input_capture_window_);
    if (tracker.windows().empty())
      input_capture_window_ = nullptr;
    else
      input_capture_window_->AddObserver(this);
  }

  SetLayer(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->set_delegate(this);
  auto* parent = GetParentContainer(current_root_);
  parent->layer()->Add(layer());
  layer()->SetBounds(parent->bounds());

  // The last region selected could have been on a larger display. Ensure that
  // the region is not larger than the current display.
  ClampCaptureRegionToRootWindowSize();

  capture_mode_bar_widget_->Init(
      CreateWidgetParams(parent, CaptureModeBarView::GetBounds(current_root_),
                         "CaptureModeBarWidget"));
  capture_mode_bar_view_ = capture_mode_bar_widget_->SetContentsView(
      std::make_unique<CaptureModeBarView>());
  capture_mode_bar_widget_->Show();

  scoped_a11y_overrider_ = std::make_unique<ScopedA11yOverrideWindowSetter>(
      capture_mode_bar_widget_->GetNativeWindow());

  // Advance focus once if spoken feedback is on so that the capture bar takes
  // spoken feedback focus.
  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled())
    focus_cycler_->AdvanceFocus(/*reverse=*/false);

  UpdateCaptureLabelWidget();
  RefreshStackingOrder(parent);

  UpdateCursor(display::Screen::GetScreen()->GetCursorScreenPoint(),
               /*is_touch=*/false);
  if (controller_->source() == CaptureModeSource::kWindow)
    capture_window_observer_ = std::make_unique<CaptureWindowObserver>(this);

  UpdateRootWindowDimmers();

  TabletModeController::Get()->AddObserver(this);
  current_root_->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  // Our event handling code assumes the capture bar widget has been initialized
  // already. So we start handling events after everything has been setup.
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);

  capture_mode_util::TriggerAccessibilityAlert(l10n_util::GetStringFUTF8(
      IDS_ASH_SCREEN_CAPTURE_ALERT_OPEN,
      l10n_util::GetStringUTF16(GetMessageIdForCaptureSource(
          controller_->source(), /*for_toggle_alert=*/false)),
      l10n_util::GetStringUTF16(
          controller_->type() == CaptureModeType::kImage
              ? IDS_ASH_SCREEN_CAPTURE_TYPE_SCREENSHOT
              : IDS_ASH_SCREEN_CAPTURE_TYPE_SCREEN_RECORDING)));
  UpdateAutoclickMenuBoundsIfNeeded();
}

void CaptureModeSession::Shutdown() {
  is_shutting_down_ = true;

  aura::Env::GetInstance()->RemovePreTargetHandler(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  current_root_->RemoveObserver(this);
  TabletModeController::Get()->RemoveObserver(this);
  if (input_capture_window_) {
    input_capture_window_->RemoveObserver(this);
    aura::client::GetCaptureClient(current_root_)
        ->SetCapture(input_capture_window_);
  }

  // This may happen if we hit esc while dragging.
  if (old_mouse_warp_status_)
    SetMouseWarpEnabled(*old_mouse_warp_status_);

  // Close these widgets immediately to avoid having them show up in the
  // captured screenshots or video.
  if (capture_label_widget_)
    capture_label_widget_->CloseNow();
  if (dimensions_label_widget_)
    dimensions_label_widget_->CloseNow();
  if (capture_mode_settings_widget_)
    capture_mode_settings_widget_->CloseNow();
  DCHECK(capture_mode_bar_widget_);
  capture_mode_bar_widget_->CloseNow();

  if (a11y_alert_on_session_exit_) {
    capture_mode_util::TriggerAccessibilityAlert(
        IDS_ASH_SCREEN_CAPTURE_ALERT_CLOSE);
  }
  UpdateAutoclickMenuBoundsIfNeeded();
}

aura::Window* CaptureModeSession::GetSelectedWindow() const {
  return capture_window_observer_ ? capture_window_observer_->window()
                                  : nullptr;
}

void CaptureModeSession::OnCaptureSourceChanged(CaptureModeSource new_source) {
  capture_source_changed_ = true;

  if (new_source == CaptureModeSource::kWindow)
    capture_window_observer_ = std::make_unique<CaptureWindowObserver>(this);
  else
    capture_window_observer_.reset();

  if (new_source == CaptureModeSource::kRegion)
    num_capture_region_adjusted_ = 0;

  capture_mode_bar_view_->OnCaptureSourceChanged(new_source);
  UpdateDimensionsLabelWidget(/*is_resizing=*/false);
  layer()->SchedulePaint(layer()->bounds());
  UpdateCaptureLabelWidget();
  UpdateCursor(display::Screen::GetScreen()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  if (focus_cycler_->RegionGroupFocused())
    focus_cycler_->ClearFocus();

  capture_mode_util::TriggerAccessibilityAlert(
      GetMessageIdForCaptureSource(new_source, /*for_toggle_alert=*/true));
}

void CaptureModeSession::OnCaptureTypeChanged(CaptureModeType new_type) {
  capture_mode_bar_view_->OnCaptureTypeChanged(new_type);
  UpdateCaptureLabelWidget();
  UpdateCursor(display::Screen::GetScreen()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  capture_mode_util::TriggerAccessibilityAlert(
      new_type == CaptureModeType::kImage
          ? IDS_ASH_SCREEN_CAPTURE_ALERT_SELECT_TYPE_IMAGE
          : IDS_ASH_SCREEN_CAPTURE_ALERT_SELECT_TYPE_VIDEO);
}

void CaptureModeSession::SetSettingsMenuShown(bool shown) {
  capture_mode_bar_view_->SetSettingsMenuShown(shown);

  if (!shown) {
    capture_mode_settings_widget_.reset();
    capture_mode_settings_view_ = nullptr;
    return;
  }

  if (!capture_mode_settings_widget_) {
    auto* parent = GetParentContainer(current_root_);
    capture_mode_settings_widget_ = std::make_unique<views::Widget>();
    capture_mode_settings_widget_->Init(CreateWidgetParams(
        parent, CaptureModeSettingsView::GetBounds(capture_mode_bar_view_),
        "CaptureModeSettingsWidget"));
    capture_mode_settings_view_ =
        capture_mode_settings_widget_->SetContentsView(
            std::make_unique<CaptureModeSettingsView>());
    parent->layer()->StackAtTop(capture_mode_settings_widget_->GetLayer());
    focus_cycler_->OnSettingsMenuWidgetCreated();
    capture_mode_settings_widget_->Show();
  }
}

void CaptureModeSession::OnMicrophoneChanged(bool microphone_enabled) {
  DCHECK(capture_mode_settings_view_);
  capture_mode_settings_view_->OnMicrophoneChanged(microphone_enabled);
}

void CaptureModeSession::ReportSessionHistograms() {
  if (controller_->source() == CaptureModeSource::kRegion)
    RecordNumberOfCaptureRegionAdjustments(num_capture_region_adjusted_);
  num_capture_region_adjusted_ = 0;

  RecordCaptureModeSwitchesFromInitialMode(capture_source_changed_);
  RecordCaptureModeConfiguration(controller_->type(), controller_->source());
}

void CaptureModeSession::StartCountDown(
    base::OnceClosure countdown_finished_callback) {
  DCHECK(capture_label_widget_);

  CaptureLabelView* label_view =
      static_cast<CaptureLabelView*>(capture_label_widget_->GetContentsView());
  label_view->StartCountDown(std::move(countdown_finished_callback));
  UpdateCaptureLabelWidgetBounds(/*animate=*/true);

  UpdateCursor(display::Screen::GetScreen()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  // Fade out the capture bar.
  ui::Layer* capture_bar_layer = capture_mode_bar_widget_->GetLayer();
  ui::ScopedLayerAnimationSettings capture_bar_settings(
      capture_bar_layer->GetAnimator());
  capture_bar_settings.SetTransitionDuration(kCaptureBarFadeOutDuration);
  capture_bar_settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  capture_bar_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  capture_bar_layer->SetOpacity(0.f);

  // Do a repaint to hide the affordance circles.
  RepaintRegion();

  // Fade out the shield if it's recording fullscreen.
  if (controller_->source() == CaptureModeSource::kFullscreen) {
    ui::ScopedLayerAnimationSettings shield_settings(layer()->GetAnimator());
    shield_settings.SetTransitionDuration(kCaptureShieldFadeOutDuration);
    shield_settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    shield_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer()->SetOpacity(0.f);
  }
}

void CaptureModeSession::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());

  auto* color_provider = AshColorProvider::Get();
  const SkColor dimming_color = color_provider->GetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield40);
  recorder.canvas()->DrawColor(dimming_color);

  PaintCaptureRegion(recorder.canvas());
}

void CaptureModeSession::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::ET_KEY_PRESSED)
    return;

  ui::KeyboardCode key_code = event->key_code();
  switch (key_code) {
    case ui::VKEY_ESCAPE: {
      event->StopPropagation();

      if (capture_mode_settings_widget_)
        SetSettingsMenuShown(false);
      else if (focus_cycler_->HasFocus())
        focus_cycler_->ClearFocus();
      else
        controller_->Stop();  // |this| is destroyed here.

      return;
    }

    case ui::VKEY_RETURN: {
      event->StopPropagation();
      if (!IsInCountDownAnimation())
        controller_->PerformCapture();  // |this| is destroyed here.
      return;
    }

    case ui::VKEY_SPACE: {
      event->StopPropagation();
      event->SetHandled();

      if (focus_cycler_->OnSpacePressed())
        return;

      // Create a default region if we are in region mode and there is no
      // existing region.
      if (controller_->source() == CaptureModeSource::kRegion &&
          controller_->user_capture_region().IsEmpty()) {
        SelectDefaultRegion();
      }
      return;
    }

    case ui::VKEY_TAB: {
      event->StopPropagation();
      event->SetHandled();
      focus_cycler_->AdvanceFocus(/*reverse=*/event->IsShiftDown());
      return;
    }

    case ui::VKEY_UP:
    case ui::VKEY_DOWN: {
      event->StopPropagation();
      event->SetHandled();
      UpdateRegionVertically(/*up=*/key_code == ui::VKEY_UP, event->flags());
      return;
    }

    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT: {
      event->StopPropagation();
      event->SetHandled();
      UpdateRegionHorizontally(/*left=*/key_code == ui::VKEY_LEFT,
                               event->flags());
      return;
    }

    default:
      return;
  }
}

void CaptureModeSession::OnMouseEvent(ui::MouseEvent* event) {
  OnLocatedEvent(event, /*is_touch=*/false);
}

void CaptureModeSession::OnTouchEvent(ui::TouchEvent* event) {
  OnLocatedEvent(event, /*is_touch=*/true);
}

void CaptureModeSession::OnTabletModeStarted() {
  UpdateCaptureLabelWidget();
  UpdateCursor(display::Screen::GetScreen()->GetCursorScreenPoint(),
               /*is_touch=*/false);
}

void CaptureModeSession::OnTabletModeEnded() {
  UpdateCaptureLabelWidget();
  UpdateCursor(display::Screen::GetScreen()->GetCursorScreenPoint(),
               /*is_touch=*/false);
}

void CaptureModeSession::OnWindowDestroying(aura::Window* window) {
  if (window == input_capture_window_) {
    input_capture_window_->RemoveObserver(this);
    input_capture_window_ = nullptr;
    return;
  }
  DCHECK_EQ(current_root_, window);
  MaybeChangeRoot(Shell::GetPrimaryRootWindow());
}

void CaptureModeSession::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!(metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
                   DISPLAY_METRIC_DEVICE_SCALE_FACTOR))) {
    return;
  }

  EndSelection(/*is_event_on_capture_bar_or_menu=*/false,
               /*region_intersects_capture_bar=*/false);

  UpdateCursor(display::Screen::GetScreen()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  // Ensure the region still fits the root window after display changes.
  ClampCaptureRegionToRootWindowSize();

  // Update the bounds of all created widgets and repaint the entire layer.
  auto* parent = GetParentContainer(current_root_);
  DCHECK_EQ(parent->layer(), layer()->parent());
  layer()->SetBounds(parent->bounds());

  DCHECK(capture_mode_bar_widget_);
  capture_mode_bar_widget_->SetBounds(
      CaptureModeBarView::GetBounds(current_root_));
  if (capture_label_widget_)
    UpdateCaptureLabelWidget();
  layer()->SchedulePaint(layer()->bounds());
}

gfx::Rect CaptureModeSession::GetSelectedWindowBounds() const {
  auto* window = GetSelectedWindow();
  return window ? window->bounds() : gfx::Rect();
}

void CaptureModeSession::RefreshStackingOrder(aura::Window* parent_container) {
  DCHECK(parent_container);
  auto* capture_mode_bar_layer = capture_mode_bar_widget_->GetLayer();
  auto* overlay_layer = layer();
  auto* parent_container_layer = parent_container->layer();

  parent_container_layer->StackAtTop(overlay_layer);
  parent_container_layer->StackAtTop(capture_label_widget_->GetLayer());
  parent_container_layer->StackAtTop(capture_mode_bar_layer);
}

void CaptureModeSession::PaintCaptureRegion(gfx::Canvas* canvas) {
  gfx::Rect region;
  bool adjustable_region = false;

  switch (controller_->source()) {
    case CaptureModeSource::kFullscreen:
      region = current_root_->bounds();
      break;

    case CaptureModeSource::kWindow:
      region = GetSelectedWindowBounds();
      break;

    case CaptureModeSource::kRegion:
      region = controller_->user_capture_region();
      adjustable_region = true;
      break;
  }

  if (region.IsEmpty())
    return;

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  region = gfx::ScaleToEnclosingRect(region, dsf);

  if (!adjustable_region) {
    canvas->FillRect(region, SK_ColorTRANSPARENT, SkBlendMode::kClear);
    canvas->FillRect(region, kCaptureRegionColor);
    return;
  }

  region.Inset(-capture_mode::kCaptureRegionBorderStrokePx,
               -capture_mode::kCaptureRegionBorderStrokePx);
  canvas->FillRect(region, SK_ColorTRANSPARENT, SkBlendMode::kClear);

  // Draw the region border.
  cc::PaintFlags border_flags;
  border_flags.setColor(capture_mode::kRegionBorderColor);
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setStrokeWidth(capture_mode::kCaptureRegionBorderStrokePx);
  border_flags.setLooper(gfx::CreateShadowDrawLooper({kRegionOutlineShadow}));
  canvas->DrawRect(gfx::RectF(region), border_flags);

  // Draws the focus ring if the region or one of the affordance circles
  // currently has focus.
  auto maybe_draw_focus_ring = [&canvas, &region,
                                &dsf](FineTunePosition position) {
    if (position == FineTunePosition::kNone)
      return;

    cc::PaintFlags focus_ring_flags;
    focus_ring_flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kFocusRingColor));
    focus_ring_flags.setStyle(cc::PaintFlags::kStroke_Style);
    focus_ring_flags.setStrokeWidth(kFocusRingStrokeWidthDp);

    if (position == FineTunePosition::kCenter) {
      gfx::RectF focus_rect(region);
      focus_rect.Inset(
          gfx::InsetsF(-kFocusRingSpacingDp - kFocusRingStrokeWidthDp / 2));
      canvas->DrawRect(focus_rect, focus_ring_flags);
      return;
    }

    const float radius =
        dsf * (kAffordanceCircleRadiusDp + kFocusRingSpacingDp +
               kFocusRingStrokeWidthDp / 2);
    canvas->DrawCircle(
        capture_mode_util::GetLocationForFineTunePosition(region, position),
        radius, focus_ring_flags);
  };

  const FineTunePosition focused_fine_tune_position =
      focus_cycler_->GetFocusedFineTunePosition();
  if (is_selecting_region_ || fine_tune_position_ != FineTunePosition::kNone) {
    maybe_draw_focus_ring(focused_fine_tune_position);
    return;
  }

  if (IsInCountDownAnimation())
    return;

  // Draw the drag affordance circles.
  cc::PaintFlags circle_flags;
  circle_flags.setColor(capture_mode::kRegionBorderColor);
  circle_flags.setStyle(cc::PaintFlags::kFill_Style);
  circle_flags.setAntiAlias(true);
  circle_flags.setLooper(gfx::CreateShadowDrawLooper(
      {kRegionAffordanceCircleShadow1, kRegionAffordanceCircleShadow2}));

  auto draw_circle = [&canvas, &circle_flags,
                      &dsf](const gfx::Point& location) {
    canvas->DrawCircle(location, dsf * kAffordanceCircleRadiusDp, circle_flags);
  };

  draw_circle(region.origin());
  draw_circle(region.top_center());
  draw_circle(region.top_right());
  draw_circle(region.right_center());
  draw_circle(region.bottom_right());
  draw_circle(region.bottom_center());
  draw_circle(region.bottom_left());
  draw_circle(region.left_center());

  maybe_draw_focus_ring(focused_fine_tune_position);
}

void CaptureModeSession::OnLocatedEvent(ui::LocatedEvent* event,
                                        bool is_touch) {
  // If we're currently in countdown animation, don't further handle any
  // located events. However we should stop the event propagation here to
  // prevent other event handlers from handling this event.
  if (IsInCountDownAnimation()) {
    event->StopPropagation();
    return;
  }

  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
    return;

  gfx::Point screen_location = event->location();
  aura::Window* event_target = static_cast<aura::Window*>(event->target());
  wm::ConvertPointToScreen(event_target, &screen_location);

  // For fullscreen/window mode, change the root window as soon as we detect the
  // cursor on a new display. For region mode, wait until the user taps down to
  // try to select a new region on the new display.
  const CaptureModeSource source = controller_->source();
  const bool is_press_event = event->type() == ui::ET_MOUSE_PRESSED ||
                              event->type() == ui::ET_TOUCH_PRESSED;

  // Clear keyboard focus on presses.
  if (is_press_event && focus_cycler_->HasFocus())
    focus_cycler_->ClearFocus();

  const bool can_change_root =
      source != CaptureModeSource::kRegion ||
      (source == CaptureModeSource::kRegion && is_press_event);
  if (can_change_root)
    MaybeChangeRoot(GetPreferredRootWindow(screen_location));

  // The root may have switched while pressing the mouse down. Move the capture
  // bar to the current display if that is the case and make sure it is stacked
  // at the top. The dimensions label and capture button have been moved and
  // stacked on tap down so manually stack at top instead of calling
  // RefreshStackingOrder.
  const bool is_release_event = event->type() == ui::ET_MOUSE_RELEASED ||
                                event->type() == ui::ET_TOUCH_RELEASED;
  if (is_release_event && source == CaptureModeSource::kRegion &&
      current_root_ !=
          capture_mode_bar_widget_->GetNativeWindow()->GetRootWindow()) {
    capture_mode_bar_widget_->SetBounds(
        CaptureModeBarView::GetBounds(current_root_));
    auto* parent = GetParentContainer(current_root_);
    parent->StackChildAtTop(capture_mode_bar_widget_->GetNativeWindow());
  }

  const bool is_event_on_settings_menu =
      IsEventInSettingsMenuBounds(screen_location);

  // Hide the settings menu if the user presses anywhere outside of the menu.
  // Skip if the event is on the settings button, since the button will handle
  // toggling the menu separately.
  if (is_press_event && !is_event_on_settings_menu &&
      !capture_mode_bar_view_->settings_button()->GetBoundsInScreen().Contains(
          screen_location)) {
    SetSettingsMenuShown(/*shown=*/false);
  }

  // Let the capture button handle any events it can handle first.
  if (ShouldCaptureLabelHandleEvent(event_target)) {
    UpdateCursor(screen_location, is_touch);
    return;
  }

  const bool is_event_on_capture_bar =
      capture_mode_bar_widget_->GetWindowBoundsInScreen().Contains(
          screen_location);
  const bool is_event_on_capture_bar_or_menu =
      is_event_on_capture_bar || is_event_on_settings_menu;

  const CaptureModeSource capture_source = controller_->source();
  const bool is_capture_fullscreen =
      capture_source == CaptureModeSource::kFullscreen;
  const bool is_capture_window = capture_source == CaptureModeSource::kWindow;
  if (is_capture_fullscreen || is_capture_window) {
    // Do not handle any event located on the capture mode bar or settings menu.
    if (is_event_on_capture_bar_or_menu) {
      UpdateCursor(screen_location, is_touch);
      return;
    }

    event->SetHandled();
    event->StopPropagation();

    switch (event->type()) {
      case ui::ET_MOUSE_MOVED:
      case ui::ET_TOUCH_PRESSED:
      case ui::ET_TOUCH_MOVED: {
        if (is_capture_window) {
          // Make sure the capture label widget will not get picked up by the
          // get topmost window algorithm otherwise a crash will happen since
          // the snapshot code tries snap a deleted window.
          std::set<aura::Window*> ignore_windows;
          if (capture_label_widget_)
            ignore_windows.insert(capture_label_widget_->GetNativeWindow());

          capture_window_observer_->UpdateSelectedWindowAtPosition(
              screen_location, ignore_windows);
        }
        UpdateCursor(screen_location, is_touch);
        break;
      }
      case ui::ET_MOUSE_RELEASED:
      case ui::ET_TOUCH_RELEASED:
        if (is_capture_fullscreen || (is_capture_window && GetSelectedWindow()))
          controller_->PerformCapture();
        break;
      default:
        break;
    }
    return;
  }

  DCHECK_EQ(CaptureModeSource::kRegion, capture_source);
  DCHECK(cursor_setter_);
  // Allow events that are located on the capture mode bar or settings menu to
  // pass through so we can click the buttons.
  if (!is_event_on_capture_bar &&
      !(capture_mode_settings_widget_ &&
        capture_mode_settings_widget_->GetWindowBoundsInScreen().Contains(
            screen_location))) {
    event->SetHandled();
    event->StopPropagation();
  }

  // OnLocatedEventPressed() and OnLocatedEventDragged used root locations since
  // CaptureModeController::user_capture_region() is stored in root
  // coordinates..
  // TODO(sammiequon): Update CaptureModeController::user_capture_region() to
  // store screen coordinates.
  gfx::Point location_in_root = event->location();
  aura::Window::ConvertPointToTarget(event_target, current_root_,
                                     &location_in_root);

  const bool region_intersects_capture_bar =
      capture_mode_bar_widget_->GetWindowBoundsInScreen().Intersects(
          controller_->user_capture_region());

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
      old_mouse_warp_status_ = SetMouseWarpEnabled(false);
      OnLocatedEventPressed(location_in_root, is_touch,
                            is_event_on_capture_bar_or_menu);
      break;
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_TOUCH_MOVED:
      OnLocatedEventDragged(location_in_root);
      break;
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_TOUCH_RELEASED:
      // Reenable mouse warping.
      if (old_mouse_warp_status_)
        SetMouseWarpEnabled(*old_mouse_warp_status_);
      old_mouse_warp_status_.reset();
      OnLocatedEventReleased(is_event_on_capture_bar_or_menu,
                             region_intersects_capture_bar);
      break;
    case ui::ET_MOUSE_MOVED:
      if (region_intersects_capture_bar) {
        if (capture_mode_settings_widget_ && !is_event_on_capture_bar_or_menu)
          SetSettingsMenuShown(/*shown=*/false);

        UpdateCaptureBarWidgetOpacity(
            is_event_on_capture_bar_or_menu ? 1.f : kCaptureBarOverlapOpacity,
            /*on_release=*/false);
      }
      break;
    default:
      break;
  }
  UpdateCursor(screen_location, is_touch);
}

FineTunePosition CaptureModeSession::GetFineTunePosition(
    const gfx::Point& location_in_root,
    bool is_touch) const {
  // In the case of overlapping affordances, prioritize the bottomm right
  // corner, then the rest of the corners, then the edges.
  static const std::vector<FineTunePosition> drag_positions = {
      FineTunePosition::kBottomRight,  FineTunePosition::kBottomLeft,
      FineTunePosition::kTopLeft,      FineTunePosition::kTopRight,
      FineTunePosition::kBottomCenter, FineTunePosition::kLeftCenter,
      FineTunePosition::kTopCenter,    FineTunePosition::kRightCenter};

  const int hit_radius =
      is_touch ? kAffordanceCircleTouchHitRadiusDp : kAffordanceCircleRadiusDp;
  const int hit_radius_squared = hit_radius * hit_radius;
  for (FineTunePosition position : drag_positions) {
    const gfx::Point position_location =
        capture_mode_util::GetLocationForFineTunePosition(
            controller_->user_capture_region(), position);
    // If |location_in_root| is within |hit_radius| of |position_location| for
    // both x and y, then |position| is the current pressed down affordance.
    if ((position_location - location_in_root).LengthSquared() <=
        hit_radius_squared) {
      return position;
    }
  }

  if (controller_->user_capture_region().Contains(location_in_root))
    return FineTunePosition::kCenter;

  return FineTunePosition::kNone;
}

void CaptureModeSession::OnLocatedEventPressed(
    const gfx::Point& location_in_root,
    bool is_touch,
    bool is_event_on_capture_bar_or_menu) {
  initial_location_in_root_ = location_in_root;
  previous_location_in_root_ = location_in_root;

  // Use cursor compositing instead of the platform cursor when dragging to
  // ensure the cursor is aligned with the region.
  is_drag_in_progress_ = true;
  Shell::Get()->UpdateCursorCompositingEnabled();

  if (!is_event_on_capture_bar_or_menu)
    UpdateCaptureBarWidgetOpacity(0.f, /*on_release=*/false);

  if (is_selecting_region_)
    return;

  fine_tune_position_ = GetFineTunePosition(location_in_root, is_touch);

  if (fine_tune_position_ == FineTunePosition::kNone &&
      !is_event_on_capture_bar_or_menu) {
    // If the point is outside the capture region and not on the capture bar or
    // settings menu, restart to the select phase.
    is_selecting_region_ = true;
    UpdateCaptureRegion(gfx::Rect(), /*is_resizing=*/true, /*by_user=*/true);
    num_capture_region_adjusted_ = 0;
    return;
  }

  // In order to hide the drag affordance circles on click, we need to repaint
  // the capture region.
  if (fine_tune_position_ != FineTunePosition::kNone) {
    ++num_capture_region_adjusted_;
    RepaintRegion();
  }

  if (fine_tune_position_ != FineTunePosition::kCenter &&
      fine_tune_position_ != FineTunePosition::kNone) {
    anchor_points_ = GetAnchorPointsForPosition(fine_tune_position_);
    const gfx::Point position_location =
        capture_mode_util::GetLocationForFineTunePosition(
            controller_->user_capture_region(), fine_tune_position_);
    MaybeShowMagnifierGlassAtPoint(position_location);
  }
}

void CaptureModeSession::OnLocatedEventDragged(
    const gfx::Point& location_in_root) {
  const gfx::Point previous_location_in_root = previous_location_in_root_;
  previous_location_in_root_ = location_in_root;

  // For the select phase, the select region is the rectangle formed by the
  // press location and the current location.
  if (is_selecting_region_) {
    UpdateCaptureRegion(
        GetRectEnclosingPoints({initial_location_in_root_, location_in_root}),
        /*is_resizing=*/true, /*by_user=*/true);
    return;
  }

  if (fine_tune_position_ == FineTunePosition::kNone)
    return;

  // For a reposition, offset the old select region by the difference between
  // the current location and the previous location, but do not let the select
  // region go offscreen.
  if (fine_tune_position_ == FineTunePosition::kCenter) {
    gfx::Rect new_capture_region = controller_->user_capture_region();
    new_capture_region.Offset(location_in_root - previous_location_in_root);
    new_capture_region.AdjustToFit(current_root_->bounds());
    UpdateCaptureRegion(new_capture_region, /*is_resizing=*/false,
                        /*by_user=*/true);
    return;
  }

  // The new region is defined by the rectangle which encloses the anchor
  // point(s) and |resizing_point|, which is based off of |location_in_root| but
  // prevents edge drags from resizing the region in the non-desired direction.
  std::vector<gfx::Point> points = anchor_points_;
  DCHECK(!points.empty());
  gfx::Point resizing_point = location_in_root;

  // For edge dragging, there will be two anchor points with the same primary
  // axis value. Setting |resizing_point|'s secondary axis value to match either
  // one of the anchor points secondary axis value will ensure that for the
  // duration of a drag, GetRectEnclosingPoints will return a rect whose
  // secondary dimension does not change.
  if (fine_tune_position_ == FineTunePosition::kLeftCenter ||
      fine_tune_position_ == FineTunePosition::kRightCenter) {
    resizing_point.set_y(points.front().y());
  } else if (fine_tune_position_ == FineTunePosition::kTopCenter ||
             fine_tune_position_ == FineTunePosition::kBottomCenter) {
    resizing_point.set_x(points.front().x());
  }
  points.push_back(resizing_point);
  UpdateCaptureRegion(GetRectEnclosingPoints(points), /*is_resizing=*/true,
                      /*by_user=*/true);
  MaybeShowMagnifierGlassAtPoint(location_in_root);
}

void CaptureModeSession::OnLocatedEventReleased(
    bool is_event_on_capture_bar_or_menu,
    bool region_intersects_capture_bar) {
  EndSelection(is_event_on_capture_bar_or_menu, region_intersects_capture_bar);

  // Do a repaint to show the affordance circles.
  RepaintRegion();

  if (!is_selecting_region_)
    return;

  // After first release event, we advance to the next phase.
  is_selecting_region_ = false;
  UpdateCaptureLabelWidget();
}

void CaptureModeSession::UpdateCaptureRegion(
    const gfx::Rect& new_capture_region,
    bool is_resizing,
    bool by_user) {
  const gfx::Rect old_capture_region = controller_->user_capture_region();
  if (old_capture_region == new_capture_region)
    return;

  // Calculate the region that has been damaged and repaint the layer. Add some
  // extra padding to make sure the border and affordance circles are also
  // repainted.
  gfx::Rect damage_region = old_capture_region;
  damage_region.Union(new_capture_region);
  damage_region.Inset(gfx::Insets(-kDamageInsetDp));
  layer()->SchedulePaint(damage_region);

  controller_->SetUserCaptureRegion(new_capture_region, by_user);
  UpdateDimensionsLabelWidget(is_resizing);
  UpdateCaptureLabelWidget();
}

void CaptureModeSession::UpdateDimensionsLabelWidget(bool is_resizing) {
  const bool should_not_show =
      !is_resizing || controller_->source() != CaptureModeSource::kRegion ||
      controller_->user_capture_region().IsEmpty();
  if (should_not_show) {
    dimensions_label_widget_.reset();
    return;
  }

  if (!dimensions_label_widget_) {
    auto* parent = GetParentContainer(current_root_);
    dimensions_label_widget_ = std::make_unique<views::Widget>();
    dimensions_label_widget_->Init(
        CreateWidgetParams(parent, gfx::Rect(), "CaptureModeDimensionsLabel"));

    auto size_label = std::make_unique<views::Label>();
    auto* color_provider = AshColorProvider::Get();
    size_label->SetEnabledColor(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    size_label->SetBackground(views::CreateRoundedRectBackground(
        color_provider->GetBaseLayerColor(
            AshColorProvider::BaseLayerType::kTransparent80),
        kSizeLabelBorderRadius));
    size_label->SetAutoColorReadabilityEnabled(false);
    dimensions_label_widget_->SetContentsView(std::move(size_label));

    dimensions_label_widget_->Show();

    // When moving to a new display, the dimensions label gets created/moved
    // onto the new display on press, while the capture bar gets moved on
    // release. In this case, we do not have to stack the dimensions label.
    if (parent == capture_mode_bar_widget_->GetNativeWindow()->parent()) {
      parent->StackChildBelow(dimensions_label_widget_->GetNativeWindow(),
                              capture_mode_bar_widget_->GetNativeWindow());
    }
  }

  views::Label* size_label =
      static_cast<views::Label*>(dimensions_label_widget_->GetContentsView());

  const gfx::Rect capture_region = controller_->user_capture_region();
  size_label->SetText(base::UTF8ToUTF16(base::StringPrintf(
      "%d x %d", capture_region.width(), capture_region.height())));

  UpdateDimensionsLabelBounds();
}

void CaptureModeSession::UpdateDimensionsLabelBounds() {
  DCHECK(dimensions_label_widget_ &&
         dimensions_label_widget_->GetContentsView());

  gfx::Rect bounds(
      dimensions_label_widget_->GetContentsView()->GetPreferredSize());
  const gfx::Rect capture_region = controller_->user_capture_region();
  gfx::Rect screen_region = current_root_->bounds();

  bounds.set_width(bounds.width() + 2 * kSizeLabelHorizontalPadding);
  bounds.set_x(capture_region.CenterPoint().x() - bounds.width() / 2);
  bounds.set_y(capture_region.bottom() + kSizeLabelYDistanceFromRegionDp);

  // The dimension label should always be within the screen and at the bottom of
  // the capture region. If it does not fit below the bottom edge fo the region,
  // move it above the bottom edge into the capture region.
  screen_region.Inset(0, 0, 0, kSizeLabelYDistanceFromRegionDp);
  bounds.AdjustToFit(screen_region);

  wm::ConvertRectToScreen(current_root_, &bounds);
  dimensions_label_widget_->SetBounds(bounds);
}

void CaptureModeSession::MaybeShowMagnifierGlassAtPoint(
    const gfx::Point& location_in_root) {
  if (!capture_mode_util::IsCornerFineTunePosition(fine_tune_position_))
    return;
  magnifier_glass_.ShowFor(current_root_, location_in_root);
}

void CaptureModeSession::CloseMagnifierGlass() {
  magnifier_glass_.Close();
}

std::vector<gfx::Point> CaptureModeSession::GetAnchorPointsForPosition(
    FineTunePosition position) {
  std::vector<gfx::Point> anchor_points;
  // For a vertex, the anchor point is the opposite vertex on the rectangle
  // (ex. bottom left vertex -> top right vertex anchor point). For an edge, the
  // anchor points are the two vertices of the opposite edge (ex. bottom edge ->
  // top left and top right anchor points).
  const gfx::Rect rect = controller_->user_capture_region();
  switch (position) {
    case FineTunePosition::kNone:
    case FineTunePosition::kCenter:
      break;
    case FineTunePosition::kTopLeft:
      anchor_points.push_back(rect.bottom_right());
      break;
    case FineTunePosition::kTopCenter:
      anchor_points.push_back(rect.bottom_left());
      anchor_points.push_back(rect.bottom_right());
      break;
    case FineTunePosition::kTopRight:
      anchor_points.push_back(rect.bottom_left());
      break;
    case FineTunePosition::kLeftCenter:
      anchor_points.push_back(rect.top_right());
      anchor_points.push_back(rect.bottom_right());
      break;
    case FineTunePosition::kRightCenter:
      anchor_points.push_back(rect.origin());
      anchor_points.push_back(rect.bottom_left());
      break;
    case FineTunePosition::kBottomLeft:
      anchor_points.push_back(rect.top_right());
      break;
    case FineTunePosition::kBottomCenter:
      anchor_points.push_back(rect.origin());
      anchor_points.push_back(rect.top_right());
      break;
    case FineTunePosition::kBottomRight:
      anchor_points.push_back(rect.origin());
      break;
  }
  DCHECK(!anchor_points.empty());
  DCHECK_LE(anchor_points.size(), 2u);
  return anchor_points;
}

void CaptureModeSession::UpdateCaptureLabelWidget() {
  if (!capture_label_widget_) {
    capture_label_widget_ = std::make_unique<views::Widget>();
    auto* parent = GetParentContainer(current_root_);
    capture_label_widget_->Init(
        CreateWidgetParams(parent, gfx::Rect(), "CaptureLabel"));
    capture_label_widget_->SetContentsView(
        std::make_unique<CaptureLabelView>(this));
    capture_label_widget_->Show();
  }

  CaptureLabelView* label_view =
      static_cast<CaptureLabelView*>(capture_label_widget_->GetContentsView());
  label_view->UpdateIconAndText();
  UpdateCaptureLabelWidgetBounds(/*animate=*/false);

  focus_cycler_->OnCaptureLabelWidgetUpdated();
}

void CaptureModeSession::UpdateCaptureLabelWidgetBounds(bool animate) {
  DCHECK(capture_label_widget_);

  const gfx::Rect bounds = CalculateCaptureLabelWidgetBounds();
  const gfx::Rect old_bounds =
      capture_label_widget_->GetNativeWindow()->GetBoundsInScreen();
  if (old_bounds == bounds)
    return;

  if (!animate) {
    capture_label_widget_->SetBounds(bounds);
    return;
  }

  ui::Layer* layer = capture_label_widget_->GetLayer();
  if (!old_bounds.IsEmpty()) {
    // This happens if there is a label or a label button showing when count
    // down starts. In this case we'll do a bounds change animation.
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(kCaptureLabelAnimationDuration);
    capture_label_widget_->SetBounds(bounds);
  } else {
    // This happens when no text message was showing when count down starts, in
    // this case we'll do a fade in + shrinking down animation.
    capture_label_widget_->SetBounds(bounds);
    const gfx::Point center_point = bounds.CenterPoint();
    layer->SetTransform(
        gfx::GetScaleTransform(gfx::Point(center_point.x() - bounds.x(),
                                          center_point.y() - bounds.y()),
                               kLabelScaleUpOnCountdown));
    layer->SetOpacity(0.f);

    // Fade in.
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(kCaptureLabelAnimationDuration);
    settings.SetTweenType(gfx::Tween::LINEAR);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer->SetOpacity(1.f);

    // Scale down from 120% -> 100%.
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    layer->SetTransform(gfx::Transform());
  }
}

gfx::Rect CaptureModeSession::CalculateCaptureLabelWidgetBounds() {
  DCHECK(capture_label_widget_);
  CaptureLabelView* label_view =
      static_cast<CaptureLabelView*>(capture_label_widget_->GetContentsView());

  const gfx::Size preferred_size = label_view->GetPreferredSize();

  // Calculates the bounds for when the capture label is not placed in the
  // middle of the screen.
  auto calculate_bounds = [&preferred_size](const gfx::Rect& capture_bounds,
                                            aura::Window* root) {
    // The capture_bounds must be at least the size of |preferred_size| plus
    // some padding for the capture label to be centered inside it.
    gfx::Rect label_bounds(capture_bounds);
    gfx::Size capture_bounds_min_size = preferred_size;
    capture_bounds_min_size.Enlarge(kCaptureRegionMinimumPaddingDp,
                                    kCaptureRegionMinimumPaddingDp);
    if (label_bounds.width() > capture_bounds_min_size.width() &&
        label_bounds.height() > capture_bounds_min_size.height()) {
      label_bounds.ClampToCenteredSize(preferred_size);
    } else {
      // The capture_bounds is too small for the capture label to be inside it.
      // Align |label_bounds| so that its horizontal centerpoint aligns with the
      // capture_bounds centerpoint.
      label_bounds.set_size(preferred_size);
      label_bounds.set_x(capture_bounds.CenterPoint().x() -
                         preferred_size.width() / 2);

      // Try to put the capture label slightly below the capture_bounds. If it
      // does not fully fit in the root window bounds, place the capture label
      // slightly above.
      const int under_capture_bounds_label_y =
          capture_bounds.bottom() + kCaptureButtonDistanceFromRegionDp;
      if (under_capture_bounds_label_y + preferred_size.height() <
          root->bounds().bottom()) {
        label_bounds.set_y(under_capture_bounds_label_y);
      } else {
        label_bounds.set_y(capture_bounds.y() -
                           kCaptureButtonDistanceFromRegionDp -
                           preferred_size.height());
      }
    }
    return label_bounds;
  };

  gfx::Rect bounds(current_root_->bounds());
  const gfx::Rect capture_region = controller_->user_capture_region();
  const gfx::Rect window_bounds = GetSelectedWindowBounds();
  const CaptureModeSource source = controller_->source();

  // For fullscreen mode, the capture label is placed in the middle of the
  // screen. For region capture mode, if it's in select phase, the capture label
  // is also placed in the middle of the screen, and if it's in fine tune phase,
  // the capture label is ideally placed in the middle of the capture region. If
  // it cannot fit, then it will be placed slightly above or below the capture
  // region. For window capture mode, it is the same as the region capture mode
  // fine tune phase logic, in that it will first try to place the label in the
  // middle of the selected window bounds, otherwise it will be placed slightly
  // above or below the selected window.
  if (source == CaptureModeSource::kRegion && !is_selecting_region_ &&
      !capture_region.IsEmpty()) {
    if (label_view->IsInCountDownAnimation()) {
      // If countdown starts, calculate the bounds based on the old capture
      // label's position, otherwise, since the countdown label bounds is
      // smaller than the label bounds and may fit into the capture region even
      // if the old capture label doesn't fit thus was place outside of the
      // capture region, it's possible that we see the countdown label animates
      // to inside of the capture region from outside of the capture region.
      bounds = capture_label_widget_->GetNativeWindow()->bounds();
      bounds.ClampToCenteredSize(preferred_size);
    } else {
      bounds = calculate_bounds(capture_region, current_root_);
    }
  } else if (source == CaptureModeSource::kWindow && !window_bounds.IsEmpty()) {
    bounds = calculate_bounds(window_bounds, current_root_);
  } else {
    bounds.ClampToCenteredSize(preferred_size);
  }
  // User capture bounds are in root window coordinates so convert them here.
  wm::ConvertRectToScreen(current_root_, &bounds);
  return bounds;
}

bool CaptureModeSession::ShouldCaptureLabelHandleEvent(
    aura::Window* event_target) {
  if (!capture_label_widget_ ||
      capture_label_widget_->GetNativeWindow() != event_target) {
    return false;
  }

  CaptureLabelView* label_view =
      static_cast<CaptureLabelView*>(capture_label_widget_->GetContentsView());
  return label_view->ShouldHandleEvent();
}

void CaptureModeSession::MaybeChangeRoot(aura::Window* new_root) {
  DCHECK(new_root->IsRootWindow());

  if (new_root == current_root_)
    return;

  current_root_->RemoveObserver(this);
  new_root->AddObserver(this);

  auto* new_parent = GetParentContainer(new_root);
  new_parent->layer()->Add(layer());
  layer()->SetBounds(new_parent->bounds());

  current_root_ = new_root;

  // Update the bounds of the widgets after setting the new root. For region
  // capture, the capture bar will move at a later time, when the mouse is
  // released.
  if (controller_->source() != CaptureModeSource::kRegion) {
    capture_mode_bar_widget_->SetBounds(
        CaptureModeBarView::GetBounds(current_root_));
  }

  // Because we use custom cursors for region and full screen capture, we need
  // to update the cursor in case the display DSF changes.
  UpdateCursor(display::Screen::GetScreen()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  // The following call to UpdateCaptureRegion will update the capture label
  // bounds, moving it onto the correct display, but will early return if the
  // region is already empty.
  if (controller_->user_capture_region().IsEmpty())
    UpdateCaptureLabelWidgetBounds(/*animate=*/false);

  // Start with a new region when we switch displays.
  is_selecting_region_ = true;
  UpdateCaptureRegion(gfx::Rect(), /*is_resizing=*/false, /*by_user=*/false);

  UpdateRootWindowDimmers();
}

void CaptureModeSession::UpdateRootWindowDimmers() {
  root_window_dimmers_.clear();

  // Add dimmers for all root windows except |current_root_| if needed.
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    if (root_window == current_root_)
      continue;

    auto dimmer = std::make_unique<WindowDimmer>(root_window);
    dimmer->window()->Show();
    root_window_dimmers_.emplace(std::move(dimmer));
  }
}

bool CaptureModeSession::IsInCountDownAnimation() const {
  CaptureLabelView* label_view =
      static_cast<CaptureLabelView*>(capture_label_widget_->GetContentsView());
  return label_view->IsInCountDownAnimation();
}

void CaptureModeSession::UpdateCursor(const gfx::Point& location_in_screen,
                                      bool is_touch) {
  // Hide mouse cursor in tablet mode.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  if (tablet_mode_controller->InTabletMode() &&
      !tablet_mode_controller->IsInDevTabletMode()) {
    cursor_setter_->HideCursor();
    return;
  }

  if (IsInCountDownAnimation()) {
    cursor_setter_->UpdateCursor(ui::mojom::CursorType::kPointer);
    return;
  }

  // If the current mouse is on capture bar or settings menu, use the pointer
  // mouse cursor.
  const bool is_event_on_capture_bar_or_menu =
      capture_mode_bar_widget_->GetWindowBoundsInScreen().Contains(
          location_in_screen) ||
      IsEventInSettingsMenuBounds(location_in_screen);
  if (is_event_on_capture_bar_or_menu) {
    cursor_setter_->UpdateCursor(ui::mojom::CursorType::kPointer);
    return;
  }

  // If the current mouse event is on capture label button, and capture label
  // button can handle the event, show the hand mouse cursor.
  const bool is_event_on_capture_button =
      capture_label_widget_->GetWindowBoundsInScreen().Contains(
          location_in_screen) &&
      static_cast<CaptureLabelView*>(capture_label_widget_->GetContentsView())
          ->ShouldHandleEvent();
  if (is_event_on_capture_button) {
    cursor_setter_->UpdateCursor(ui::mojom::CursorType::kHand);
    return;
  }

  const CaptureModeSource source = controller_->source();
  if (source == CaptureModeSource::kWindow && !GetSelectedWindow()) {
    // If we're in window capture mode and there is no select window at the
    // moment, we should use the original mouse.
    cursor_setter_->ResetCursor();
    return;
  }

  if (source == CaptureModeSource::kFullscreen ||
      source == CaptureModeSource::kWindow) {
    // For fullscreen and other window capture cases, we should either use
    // image capture icon or screen record icon as the mouse icon.
    cursor_setter_->UpdateCursor(GetCursorForFullscreenOrWindowCapture(
        controller_->type() == CaptureModeType::kImage));
    return;
  }

  DCHECK_EQ(source, CaptureModeSource::kRegion);
  if (fine_tune_position_ != FineTunePosition::kNone) {
    // We're in fine tuning process.
    if (capture_mode_util::IsCornerFineTunePosition(fine_tune_position_)) {
      cursor_setter_->HideCursor();
    } else {
      cursor_setter_->UpdateCursor(
          GetCursorTypeForFineTunePosition(fine_tune_position_));
    }
  } else {
    // Otherwise update the cursor depending on the current cursor location.
    cursor_setter_->UpdateCursor(GetCursorTypeForFineTunePosition(
        GetFineTunePosition(location_in_screen, is_touch)));
  }
}

bool CaptureModeSession::IsUsingCustomCursor(CaptureModeType type) const {
  return cursor_setter_->IsUsingCustomCursor(type);
}

void CaptureModeSession::UpdateCaptureBarWidgetOpacity(float opacity,
                                                       bool on_release) {
  DCHECK(capture_mode_bar_view_);
  DCHECK(capture_mode_bar_widget_->GetLayer());

  ui::Layer* capture_bar_layer = capture_mode_bar_widget_->GetLayer();
  if (capture_bar_layer->GetTargetOpacity() == opacity)
    return;

  ui::ScopedLayerAnimationSettings capture_bar_settings(
      capture_bar_layer->GetAnimator());
  capture_bar_settings.SetTransitionDuration(
      on_release ? kCaptureBarOnReleaseOpacityChangeDuration
                 : kCaptureBarOpacityChangeDuration);
  capture_bar_settings.SetTweenType(on_release ? gfx::Tween::FAST_OUT_SLOW_IN
                                               : gfx::Tween::LINEAR);
  capture_bar_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  capture_bar_layer->SetOpacity(opacity);
}

void CaptureModeSession::ClampCaptureRegionToRootWindowSize() {
  gfx::Rect new_capture_region = controller_->user_capture_region();
  new_capture_region.AdjustToFit(current_root_->bounds());
  controller_->SetUserCaptureRegion(new_capture_region, /*by_user=*/false);
}

void CaptureModeSession::EndSelection(bool is_event_on_capture_bar_or_menu,
                                      bool region_intersects_capture_bar) {
  fine_tune_position_ = FineTunePosition::kNone;
  anchor_points_.clear();

  is_drag_in_progress_ = false;
  Shell::Get()->UpdateCursorCompositingEnabled();

  // TODO(richui): Update this for tablet mode.
  UpdateCaptureBarWidgetOpacity(
      region_intersects_capture_bar && !is_event_on_capture_bar_or_menu
          ? kCaptureBarOverlapOpacity
          : 1.f,
      /*on_release=*/true);

  UpdateDimensionsLabelWidget(/*is_resizing=*/false);
  CloseMagnifierGlass();
}

void CaptureModeSession::RepaintRegion() {
  gfx::Rect damage_region = controller_->user_capture_region();
  damage_region.Inset(gfx::Insets(-kDamageInsetDp));
  layer()->SchedulePaint(damage_region);
}

void CaptureModeSession::SelectDefaultRegion() {
  is_selecting_region_ = false;

  // Default is centered in the root, and its width and height are
  // |kRegionDefaultRatio| size of the root.
  gfx::Rect default_capture_region = current_root_->bounds();
  default_capture_region.ClampToCenteredSize(gfx::ScaleToCeiledSize(
      default_capture_region.size(), kRegionDefaultRatio));
  UpdateCaptureRegion(default_capture_region, /*is_resizing=*/false,
                      /*by_user=*/true);
}

void CaptureModeSession::UpdateRegionHorizontally(bool left, int event_flags) {
  const FineTunePosition focused_fine_tune_position =
      focus_cycler_->GetFocusedFineTunePosition();
  if (focused_fine_tune_position == FineTunePosition::kNone ||
      focused_fine_tune_position == FineTunePosition::kTopCenter ||
      focused_fine_tune_position == FineTunePosition::kBottomCenter) {
    return;
  }

  const int change = GetArrowKeyPressChange(event_flags);
  gfx::Rect new_capture_region = controller_->user_capture_region();

  if (focused_fine_tune_position == FineTunePosition::kCenter) {
    new_capture_region.Offset(left ? -change : change, 0);
    new_capture_region.AdjustToFit(current_root_->bounds());
  } else {
    const gfx::Point location =
        capture_mode_util::GetLocationForFineTunePosition(
            new_capture_region, focused_fine_tune_position);
    // If an affordance circle on the left side of the capture region is
    // focused, left presses will enlarge the existing region and right presses
    // will shrink the existing region. If it is on the right side, right
    // presses will enlarge and left presses will shrink.
    const bool affordance_on_left = location.x() == new_capture_region.x();
    const bool shrink = affordance_on_left ^ left;

    if (shrink && new_capture_region.width() < change)
      return;

    const int inset = shrink ? change : -change;
    gfx::Insets insets(0, affordance_on_left ? inset : 0, 0,
                       affordance_on_left ? 0 : inset);
    new_capture_region.Inset(insets);
    ClipRectToFit(&new_capture_region, current_root_->bounds());
  }

  UpdateCaptureRegion(new_capture_region, /*is_resizing=*/false,
                      /*by_user=*/true);
}

void CaptureModeSession::UpdateRegionVertically(bool up, int event_flags) {
  const FineTunePosition focused_fine_tune_position =
      focus_cycler_->GetFocusedFineTunePosition();
  if (focused_fine_tune_position == FineTunePosition::kNone ||
      focused_fine_tune_position == FineTunePosition::kLeftCenter ||
      focused_fine_tune_position == FineTunePosition::kRightCenter) {
    return;
  }

  const int change = GetArrowKeyPressChange(event_flags);
  gfx::Rect new_capture_region = controller_->user_capture_region();

  // TODO(sammiequon): The below is similar to UpdateRegionHorizontally() except
  // we are acting on the y-axis. Investigate if we can remove the duplication.
  if (focused_fine_tune_position == FineTunePosition::kCenter) {
    new_capture_region.Offset(0, up ? -change : change);
    new_capture_region.AdjustToFit(current_root_->bounds());
  } else {
    const gfx::Point location =
        capture_mode_util::GetLocationForFineTunePosition(
            new_capture_region, focused_fine_tune_position);
    // If an affordance circle on the top side of the capture region is
    // focused, up presses will enlarge the existing region and down presses
    // will shrink the existing region. If it is on the bottom side, down
    // presses will enlarge and up presses will shrink.
    const bool affordance_on_top = location.y() == new_capture_region.y();
    const bool shrink = affordance_on_top ^ up;

    if (shrink && new_capture_region.height() < change)
      return;

    const int inset = shrink ? change : -change;
    gfx::Insets insets(affordance_on_top ? inset : 0, 0,
                       affordance_on_top ? 0 : inset, 0);
    new_capture_region.Inset(insets);

    ClipRectToFit(&new_capture_region, current_root_->bounds());
  }

  UpdateCaptureRegion(new_capture_region, /*is_resizing=*/false,
                      /*by_user=*/true);
}

bool CaptureModeSession::IsEventInSettingsMenuBounds(
    const gfx::Point& location_in_screen) {
  if (!capture_mode_settings_widget_)
    return false;

  gfx::Rect settings_menu_bounds(
      capture_mode_settings_widget_->GetWindowBoundsInScreen());
  settings_menu_bounds.Inset(
      0, 0, 0, -capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu);

  return settings_menu_bounds.Contains(location_in_screen);
}

}  // namespace ash
