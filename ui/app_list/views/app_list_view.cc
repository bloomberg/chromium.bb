// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_view.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/wallpaper/wallpaper_color_profile.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/custom_launcher_page_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/speech_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"

using wallpaper::ColorProfileType;

namespace app_list {

namespace {

// The margin from the edge to the speech UI.
constexpr int kSpeechUIMargin = 12;

// The height of the half app list from the bottom of the screen.
constexpr int kHalfAppListHeight = 561;

// The fraction of app list height that the app list must be released at in
// order to transition to the next state.
constexpr int kAppListThresholdDenominator = 3;

// The velocity the app list must be dragged in order to transition to the next
// state, measured in DIPs/event.
constexpr int kAppListDragVelocityThreshold = 25;

// The scroll offset in order to transition from PEEKING to FULLSCREEN
constexpr int kAppListMinScrollToSwitchStates = 20;

// The DIP distance from the bezel in which a gesture drag end results in a
// closed app list.
constexpr int kAppListBezelMargin = 50;

// The blur radius of the app list background.
constexpr int kAppListBlurRadius = 30;

// The size of app info dialog in fullscreen app list.
constexpr int kAppInfoDialogWidth = 512;
constexpr int kAppInfoDialogHeight = 384;

// The vertical position for the appearing animation of the speech UI.
constexpr float kSpeechUIAppearingPosition = 12;

// The animation duration for app list movement.
constexpr float kAppListAnimationDurationMs = 300;

// This view forwards the focus to the search box widget by providing it as a
// FocusTraversable when a focus search is provided.
class SearchBoxFocusHost : public views::View {
 public:
  explicit SearchBoxFocusHost(views::Widget* search_box_widget)
      : search_box_widget_(search_box_widget) {}

  ~SearchBoxFocusHost() override {}

  views::FocusTraversable* GetFocusTraversable() override {
    return search_box_widget_;
  }

 private:
  views::Widget* search_box_widget_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxFocusHost);
};

// The view for the App List overlay, which appears as a white rounded
// rectangle with the given radius.
class AppListOverlayView : public views::View {
 public:
  explicit AppListOverlayView(int corner_radius)
      : corner_radius_(corner_radius) {
    SetPaintToLayer();
    SetVisible(false);
    layer()->SetOpacity(0.0f);
  }

  ~AppListOverlayView() override {}

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(SK_ColorWHITE);
    canvas->DrawRoundRect(GetContentsBounds(), corner_radius_, flags);
  }

 private:
  const int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(AppListOverlayView);
};

SkColor GetBackgroundShieldColor(const std::vector<SkColor>& prominent_colors) {
  if (prominent_colors.empty())
    return app_list::AppListView::kDefaultBackgroundColor;

  DCHECK_EQ(static_cast<size_t>(ColorProfileType::NUM_OF_COLOR_PROFILES),
            prominent_colors.size());

  const SkColor dark_muted =
      prominent_colors[static_cast<int>(ColorProfileType::DARK_MUTED)];
  if (SK_ColorTRANSPARENT == dark_muted)
    return app_list::AppListView::kDefaultBackgroundColor;
  return color_utils::GetResultingPaintColor(
      SkColorSetA(SK_ColorBLACK, AppListView::kAppListColorDarkenAlpha),
      dark_muted);
}

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kExcludeWindowFromEventHandling, false);

// This targeter prevents routing events to sub-windows, such as
// RenderHostWindow in order to handle events in context of app list.
class AppListEventTargeter : public aura::WindowTargeter {
 public:
  AppListEventTargeter() = default;
  ~AppListEventTargeter() override = default;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    if (window->GetProperty(kExcludeWindowFromEventHandling)) {
      // Allow routing to sub-windows for ET_MOUSE_MOVED event which is used by
      // accessibility to enter the mode of exploration of WebView contents.
      if (event.type() != ui::ET_MOUSE_MOVED)
        return false;
    }
    return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(window, event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListEventTargeter);
};

}  // namespace

// An animation observer to hide the view at the end of the animation.
class HideViewAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  HideViewAnimationObserver() : frame_(NULL), target_(NULL) {}

  ~HideViewAnimationObserver() override {
    if (target_)
      StopObservingImplicitAnimations();
  }

  void SetTarget(views::View* target) {
    if (target_)
      StopObservingImplicitAnimations();
    target_ = target;
  }

  void set_frame(views::BubbleFrameView* frame) { frame_ = frame; }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (target_) {
      target_->SetVisible(false);
      target_ = NULL;

      // Should update the background by invoking SchedulePaint().
      if (frame_)
        frame_->SchedulePaint();
    }
  }

  views::BubbleFrameView* frame_;
  views::View* target_;

  DISALLOW_COPY_AND_ASSIGN(HideViewAnimationObserver);
};

////////////////////////////////////////////////////////////////////////////////
// AppListView:

AppListView::AppListView(AppListViewDelegate* delegate)
    : delegate_(delegate),
      model_(delegate->GetModel()),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()),
      is_background_blur_enabled_(features::IsBackgroundBlurEnabled()),
      display_observer_(this),
      animation_observer_(new HideViewAnimationObserver()),
      app_list_animation_duration_ms_(kAppListAnimationDurationMs) {
  CHECK(delegate);
  delegate_->GetSpeechUI()->AddObserver(this);

  if (is_fullscreen_app_list_enabled_) {
    display_observer_.Add(display::Screen::GetScreen());
    delegate_->AddObserver(this);
  }
}

AppListView::~AppListView() {
  delegate_->GetSpeechUI()->RemoveObserver(this);
  if (is_fullscreen_app_list_enabled_)
    delegate_->RemoveObserver(this);

  animation_observer_.reset();
  // Remove child views first to ensure no remaining dependencies on delegate_.
  RemoveAllChildViews(true);
}

// static
void AppListView::ExcludeWindowFromEventHandling(aura::Window* window) {
  DCHECK(window);
  window->SetProperty(kExcludeWindowFromEventHandling, true);
}

void AppListView::Initialize(gfx::NativeView parent,
                             int initial_apps_page,
                             bool is_tablet_mode,
                             bool is_side_shelf) {
  base::Time start_time = base::Time::Now();
  is_tablet_mode_ = is_tablet_mode;
  is_side_shelf_ = is_side_shelf;
  InitContents(parent, initial_apps_page);
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  set_color(kContentsBackgroundColor);
  set_parent_window(parent);

  if (is_fullscreen_app_list_enabled_)
    InitializeFullscreen(parent, initial_apps_page);
  else
    InitializeBubble(parent, initial_apps_page);

  InitChildWidgets();
  AddChildView(overlay_view_);

  if (is_fullscreen_app_list_enabled_)
    SetState(app_list_state_);

  delegate_->ViewInitialized();

  UMA_HISTOGRAM_TIMES("Apps.AppListCreationTime",
                      base::Time::Now() - start_time);
  app_list_main_view_->model()->RecordItemsInFoldersForUMA();
}

void AppListView::SetBubbleArrow(views::BubbleBorder::Arrow arrow) {
  GetBubbleFrameView()->bubble_border()->set_arrow(arrow);
  SizeToContents();  // Recalcuates with new border.
  GetBubbleFrameView()->SchedulePaint();
}

void AppListView::MaybeSetAnchorPoint(const gfx::Point& anchor_point) {
  // if the AppListView is a bubble
  if (!is_fullscreen_app_list_enabled_)
    SetAnchorRect(gfx::Rect(anchor_point, gfx::Size()));
}

void AppListView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  app_list_main_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppListView::ShowWhenReady() {
  app_list_main_view_->ShowAppListWhenReady();
}

void AppListView::UpdateBounds() {
  // if the AppListView is a bubble
  if (!is_fullscreen_app_list_enabled_)
    SizeToContents();
}

void AppListView::SetAppListOverlayVisible(bool visible) {
  DCHECK(overlay_view_);

  // Display the overlay immediately so we can begin the animation.
  overlay_view_->SetVisible(true);

  ui::ScopedLayerAnimationSettings settings(
      overlay_view_->layer()->GetAnimator());
  settings.SetTweenType(gfx::Tween::LINEAR);

  // If we're dismissing the overlay, hide the view at the end of the animation.
  if (!visible) {
    // Since only one animation is visible at a time, it's safe to re-use
    // animation_observer_ here.
    animation_observer_->set_frame(NULL);
    animation_observer_->SetTarget(overlay_view_);
    settings.AddObserver(animation_observer_.get());
  }

  const float kOverlayFadeInMilliseconds = 125;
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kOverlayFadeInMilliseconds));

  const float kOverlayOpacity = 0.75f;
  overlay_view_->layer()->SetOpacity(visible ? kOverlayOpacity : 0.0f);
  // Create the illusion that the search box is hidden behind the app list
  // overlay mask by setting its opacity to the same value, and disabling it.
  {
    ui::ScopedLayerAnimationSettings settings(
        search_box_widget_->GetLayer()->GetAnimator());
    const float kSearchBoxWidgetOpacity = 0.5f;
    search_box_widget_->GetLayer()->SetOpacity(visible ? kSearchBoxWidgetOpacity
                                                       : 1.0f);
    search_box_view_->SetEnabled(!visible);
    if (!visible)
      search_box_view_->search_box()->RequestFocus();
  }
}

gfx::Size AppListView::CalculatePreferredSize() const {
  return app_list_main_view_->GetPreferredSize();
}

void AppListView::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
  if (!next_paint_callback_.is_null()) {
    next_paint_callback_.Run();
    next_paint_callback_.Reset();
  }
}

const char* AppListView::GetClassName() const {
  return "AppListView";
}

bool AppListView::ShouldHandleSystemCommands() const {
  return true;
}

bool AppListView::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
  // While on the start page, don't descend into the custom launcher page. Since
  // the only valid action is to open it.
  ContentsView* contents_view = app_list_main_view_->contents_view();
  if (contents_view->custom_page_view() &&
      contents_view->GetActiveState() == AppListModel::STATE_START)
    return !contents_view->custom_page_view()
                ->GetCollapsedLauncherPageBounds()
                .Contains(location);

  return views::BubbleDialogDelegateView::
      ShouldDescendIntoChildForEventHandling(child, location);
}

class AppListView::FullscreenWidgetObserver : views::WidgetObserver {
 public:
  explicit FullscreenWidgetObserver(app_list::AppListView* view)
      : widget_observer_(this) {
    view_ = view;
    widget_observer_.Add(view_->GetWidget());
  }
  ~FullscreenWidgetObserver() override {}

  // Overridden from WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override {
    if (view_->app_list_state() != AppListView::CLOSED)
      view_->SetState(AppListView::CLOSED);
    widget_observer_.Remove(view_->GetWidget());
  }

 private:
  app_list::AppListView* view_;
  ScopedObserver<views::Widget, WidgetObserver> widget_observer_;
  DISALLOW_COPY_AND_ASSIGN(FullscreenWidgetObserver);
};

void AppListView::InitContents(gfx::NativeView parent, int initial_apps_page) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/440224 and
  // crbug.com/441028 are fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "440224, 441028 AppListView::InitContents"));

  if (is_fullscreen_app_list_enabled_) {
    // The shield view that colors/blurs the background of the app list and
    // makes it transparent.
    app_list_background_shield_ = new views::View;
    app_list_background_shield_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    app_list_background_shield_->layer()->SetOpacity(
        is_background_blur_enabled_ ? kAppListOpacityWithBlur
                                    : kAppListOpacity);
    SetBackgroundShieldColor();
    if (is_background_blur_enabled_) {
      app_list_background_shield_->layer()->SetBackgroundBlur(
          kAppListBlurRadius);
    }
    AddChildView(app_list_background_shield_);
  }
  app_list_main_view_ = new AppListMainView(delegate_, this);
  AddChildView(app_list_main_view_);
  app_list_main_view_->SetPaintToLayer();
  app_list_main_view_->layer()->SetFillsBoundsOpaquely(false);
  app_list_main_view_->layer()->SetMasksToBounds(true);
  // This will be added to the |search_box_widget_| after the app list widget is
  // initialized.
  search_box_view_ = new SearchBoxView(app_list_main_view_, delegate_, this);
  search_box_view_->SetPaintToLayer();
  search_box_view_->layer()->SetFillsBoundsOpaquely(false);
  search_box_view_->layer()->SetMasksToBounds(true);

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/440224 and
  // crbug.com/441028 are fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "440224, 441028 AppListView::InitContents1"));

  app_list_main_view_->Init(
      parent, is_fullscreen_app_list_enabled_ ? 0 : initial_apps_page,
      search_box_view_);

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/440224 and
  // crbug.com/441028 are fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "440224, 441028 AppListView::InitContents2"));

  // Speech recognition is available only when the start page exists.
  if (delegate_ && delegate_->IsSpeechRecognitionEnabled()) {
    speech_view_ = new SpeechView(delegate_);
    speech_view_->SetVisible(false);
    speech_view_->SetPaintToLayer();
    speech_view_->layer()->SetFillsBoundsOpaquely(false);
    speech_view_->layer()->SetOpacity(0.0f);
    AddChildView(speech_view_);
  }
}

void AppListView::InitChildWidgets() {
  DCHECK(search_box_view_);

  // Create the search box widget.
  views::Widget::InitParams search_box_widget_params(
      views::Widget::InitParams::TYPE_CONTROL);
  search_box_widget_params.parent = GetWidget()->GetNativeView();
  search_box_widget_params.opacity =
      views::Widget::InitParams::TRANSLUCENT_WINDOW;
  search_box_widget_params.name = "SearchBoxView";

  // Create a widget for the SearchBoxView to live in. This allows the
  // SearchBoxView to be on top of the custom launcher page's WebContents
  // (otherwise the search box events will be captured by the WebContents).
  search_box_widget_ = new views::Widget;
  search_box_widget_->Init(search_box_widget_params);
  search_box_widget_->SetContentsView(search_box_view_);

  // The search box will not naturally receive focus by itself (because it is in
  // a separate widget). Create this SearchBoxFocusHost in the main widget to
  // forward the focus search into to the search box.
  search_box_focus_host_ = new SearchBoxFocusHost(search_box_widget_);
  AddChildView(search_box_focus_host_);
  search_box_widget_->SetFocusTraversableParentView(search_box_focus_host_);
  search_box_widget_->SetFocusTraversableParent(
      GetWidget()->GetFocusTraversable());

  app_list_main_view_->contents_view()->Layout();
}

void AppListView::InitializeFullscreen(gfx::NativeView parent,
                                       int initial_apps_page) {
  const display::Display display_nearest_view = GetDisplayNearestView();
  const gfx::Rect display_work_area_bounds = display_nearest_view.work_area();

  // Set the widget height to the shelf height to replace the shelf background
  // on show animation with no flicker. In shelf mode we set the bounds to the
  // top of the screen because the widget does not animate.
  const int overlay_view_bounds_y = is_side_shelf_
                                        ? display_work_area_bounds.y()
                                        : display_work_area_bounds.bottom();
  gfx::Rect app_list_overlay_view_bounds(
      display_nearest_view.bounds().x(), overlay_view_bounds_y,
      display_nearest_view.bounds().width(),
      display_nearest_view.bounds().height());

  fullscreen_widget_ = new views::Widget;
  views::Widget::InitParams app_list_overlay_view_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);

  app_list_overlay_view_params.name = "AppList";
  app_list_overlay_view_params.parent = parent;
  app_list_overlay_view_params.delegate = this;
  app_list_overlay_view_params.opacity =
      views::Widget::InitParams::TRANSLUCENT_WINDOW;
  app_list_overlay_view_params.layer_type = ui::LAYER_SOLID_COLOR;
  fullscreen_widget_->Init(app_list_overlay_view_params);
  fullscreen_widget_->GetNativeWindow()->SetEventTargeter(
      base::MakeUnique<AppListEventTargeter>());

  // Set native view's bounds directly to avoid screen position controller
  // setting bounds in the display where the widget has the largest
  // intersection. Also, we should not set native view's bounds in screen
  // coordinates as it causes crash in DesktopScreenPositionClient::SetBounds()
  // when '--mash' flag is enabled for desktop build (See crbug.com/757573).
  gfx::NativeView native_view = fullscreen_widget_->GetNativeView();
  ::wm::ConvertRectFromScreen(native_view->parent(),
                              &app_list_overlay_view_bounds);
  native_view->SetBounds(app_list_overlay_view_bounds);

  overlay_view_ = new AppListOverlayView(0 /* no corners */);

  widget_observer_ = std::unique_ptr<FullscreenWidgetObserver>(
      new FullscreenWidgetObserver(this));
}

void AppListView::InitializeBubble(gfx::NativeView parent,
                                   int initial_apps_page) {
  set_margins(gfx::Insets());
  set_close_on_deactivate(false);
  set_shadow(views::BubbleBorder::NO_ASSETS);

  // This creates the app list widget (Before this, child widgets cannot be
  // created).
  views::BubbleDialogDelegateView::CreateBubble(this);

  SetBubbleArrow(views::BubbleBorder::FLOAT);
  // We can now create the internal widgets.

  const int kOverlayCornerRadius =
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius();
  overlay_view_ = new AppListOverlayView(kOverlayCornerRadius);
  overlay_view_->SetBoundsRect(GetContentsBounds());
}

void AppListView::HandleClickOrTap(ui::LocatedEvent* event) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  // No-op if app list is on fullscreen all apps state and the event location is
  // within apps grid view's bounds.
  if (app_list_state_ == FULLSCREEN_ALL_APPS &&
      GetAppsGridView()->GetBoundsInScreen().Contains(event->location())) {
    return;
  }

  if (!search_box_view_->is_search_box_active()) {
    SetState(CLOSED);
    return;
  }

  search_box_view_->ClearSearch();
  search_box_view_->SetSearchBoxActive(false);
}

void AppListView::StartDrag(const gfx::Point& location) {
  // Convert drag point from widget coordinates to screen coordinates because
  // the widget bounds changes during the dragging.
  initial_drag_point_ = location;
  ConvertPointToScreen(this, &initial_drag_point_);
  initial_window_bounds_ = fullscreen_widget_->GetWindowBoundsInScreen();
  if (app_list_state_ == PEEKING)
    drag_started_from_peeking_ = true;
}

void AppListView::UpdateDrag(const gfx::Point& location) {
  if (initial_drag_point_ == gfx::Point()) {
    // When the app grid view redirects the event to the app list view, we
    // detect this by seeing that StartDrag was not called. This sets up the
    // drag.
    StartDrag(location);
    return;
  }
  // Update the widget bounds based on the initial widget bounds and drag delta.
  gfx::Point location_in_screen_coordinates = location;
  ConvertPointToScreen(this, &location_in_screen_coordinates);
  int new_y_position = location_in_screen_coordinates.y() -
                       initial_drag_point_.y() + initial_window_bounds_.y();

  UpdateYPositionAndOpacity(new_y_position,
                            GetAppListBackgroundOpacityDuringDragging());
}

void AppListView::EndDrag(const gfx::Point& location) {
  // When the SearchBoxView closes the app list, ignore the final event.
  if (app_list_state_ == CLOSED)
    return;

  // Change the app list state based on where the drag ended. If fling velocity
  // was over the threshold, snap to the next state in the direction of the
  // fling.
  if (std::abs(last_fling_velocity_) >= kAppListDragVelocityThreshold) {
    // If the user releases drag with velocity over the threshold, snap to
    // the next state, ignoring the drag release position.

    if (last_fling_velocity_ > 0) {
      switch (app_list_state_) {
        case PEEKING:
        case HALF:
        case FULLSCREEN_SEARCH:
        case FULLSCREEN_ALL_APPS:
          SetState(CLOSED);
          break;
        case CLOSED:
          NOTREACHED();
          break;
      }
    } else {
      switch (app_list_state_) {
        case FULLSCREEN_ALL_APPS:
        case FULLSCREEN_SEARCH:
          SetState(app_list_state_);
          break;
        case HALF:
          SetState(FULLSCREEN_SEARCH);
          break;
        case PEEKING:
          UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                    kSwipe, kMaxPeekingToFullscreen);
          SetState(FULLSCREEN_ALL_APPS);
          break;
        case CLOSED:
          NOTREACHED();
          break;
      }
    }
  } else {
    const int display_height = GetDisplayNearestView().size().height();
    int app_list_y_for_state = 0;
    int app_list_height = 0;
    switch (app_list_state_) {
      case FULLSCREEN_ALL_APPS:
      case FULLSCREEN_SEARCH:
        app_list_y_for_state = 0;
        app_list_height = display_height;
        break;
      case HALF:
        app_list_y_for_state = display_height - kHalfAppListHeight;
        app_list_height = kHalfAppListHeight;
        break;
      case PEEKING:
        app_list_y_for_state = display_height - kPeekingAppListHeight;
        app_list_height = kPeekingAppListHeight;
        break;
      case CLOSED:
        NOTREACHED();
        break;
    }

    const int app_list_threshold =
        app_list_height / kAppListThresholdDenominator;
    gfx::Point location_in_screen_coordinates = location;
    ConvertPointToScreen(this, &location_in_screen_coordinates);
    const int drag_delta =
        initial_drag_point_.y() - location_in_screen_coordinates.y();
    const int location_y_in_current_display =
        location_in_screen_coordinates.y() -
        GetDisplayNearestView().bounds().y();
    // If the drag ended near the bezel, close the app list and return early.
    if (location_y_in_current_display >=
        (display_height - kAppListBezelMargin)) {
      SetState(CLOSED);
      return;
    }
    switch (app_list_state_) {
      case FULLSCREEN_ALL_APPS:
        if (std::abs(drag_delta) > app_list_threshold)
          SetState(is_tablet_mode_ || is_side_shelf_ ? CLOSED : PEEKING);
        else
          SetState(app_list_state_);
        break;
      case FULLSCREEN_SEARCH:
        if (std::abs(drag_delta) > app_list_threshold)
          SetState(CLOSED);
        else
          SetState(app_list_state_);
        break;
      case HALF:
        if (std::abs(drag_delta) > app_list_threshold) {
          SetState(drag_delta > 0 ? FULLSCREEN_SEARCH : CLOSED);
        } else {
          SetState(app_list_state_);
        }
        break;
      case PEEKING:
        if (std::abs(drag_delta) > app_list_threshold) {
          if (drag_delta > 0) {
            SetState(FULLSCREEN_ALL_APPS);
            UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                      kSwipe, kMaxPeekingToFullscreen);
          } else {
            SetState(CLOSED);
          }
        } else {
          SetState(app_list_state_);
        }
        break;
      case CLOSED:
        NOTREACHED();
        break;
    }
  }
  drag_started_from_peeking_ = false;
  DraggingLayout();
  initial_drag_point_ = gfx::Point();
}

void AppListView::RecordStateTransitionForUma(AppListState new_state) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  AppListStateTransitionSource transition =
      GetAppListStateTransitionSource(new_state);
  // kMaxAppListStateTransition denotes a transition we are not interested in
  // recording (ie. PEEKING->PEEKING).
  if (transition == kMaxAppListStateTransition)
    return;

  UMA_HISTOGRAM_ENUMERATION(kAppListStateTransitionSourceHistogram, transition,
                            kMaxAppListStateTransition);
}

display::Display AppListView::GetDisplayNearestView() const {
  return display::Screen::GetScreen()->GetDisplayNearestView(parent_window());
}

AppsGridView* AppListView::GetAppsGridView() const {
  return app_list_main_view_->contents_view()
      ->apps_container_view()
      ->apps_grid_view();
}

AppListStateTransitionSource AppListView::GetAppListStateTransitionSource(
    AppListState target_state) const {
  switch (app_list_state_) {
    case CLOSED:
      // CLOSED->X transitions are not useful for UMA.
      NOTREACHED();
      return kMaxAppListStateTransition;
    case PEEKING:
      switch (target_state) {
        case CLOSED:
          return kPeekingToClosed;
        case HALF:
          return kPeekingToHalf;
        case FULLSCREEN_ALL_APPS:
          return kPeekingToFullscreenAllApps;
        case PEEKING:
          // PEEKING->PEEKING is used when resetting the widget position after a
          // failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
        case FULLSCREEN_SEARCH:
          // PEEKING->FULLSCREEN_SEARCH is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }
    case HALF:
      switch (target_state) {
        case CLOSED:
          return kHalfToClosed;
        case PEEKING:
          return kHalfToPeeking;
        case FULLSCREEN_SEARCH:
          return KHalfToFullscreenSearch;
        case HALF:
          // HALF->HALF is used when resetting the widget position after a
          // failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
        case FULLSCREEN_ALL_APPS:
          // HALF->FULLSCREEN_ALL_APPS is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }

    case FULLSCREEN_ALL_APPS:
      switch (target_state) {
        case CLOSED:
          return kFullscreenAllAppsToClosed;
        case PEEKING:
          return kFullscreenAllAppsToPeeking;
        case FULLSCREEN_SEARCH:
          return kFullscreenAllAppsToFullscreenSearch;
        case HALF:
          // FULLSCREEN_ALL_APPS->HALF is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
        case FULLSCREEN_ALL_APPS:
          // FULLSCREEN_ALL_APPS->FULLSCREEN_ALL_APPS is used when resetting the
          // widget positon after a failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
      }
    case FULLSCREEN_SEARCH:
      switch (target_state) {
        case CLOSED:
          return kFullscreenSearchToClosed;
        case FULLSCREEN_ALL_APPS:
          return kFullscreenSearchToFullscreenAllApps;
        case FULLSCREEN_SEARCH:
          // FULLSCREEN_SEARCH->FULLSCREEN_SEARCH is used when resetting the
          // widget position after a failed state transition. Not useful for
          // UMA.
          return kMaxAppListStateTransition;
        case PEEKING:
          // FULLSCREEN_SEARCH->PEEKING is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
        case HALF:
          // FULLSCREEN_SEARCH->HALF is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }
  }
}

void AppListView::OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                           views::Widget* widget) const {
  if (!params->native_widget) {
    views::ViewsDelegate* views_delegate = views::ViewsDelegate::GetInstance();
    if (views_delegate && !views_delegate->native_widget_factory().is_null()) {
      params->native_widget =
          views_delegate->native_widget_factory().Run(*params, widget);
    }
  }
  // Apply a WM-provided shadow (see ui/wm/core/).
  params->shadow_type = views::Widget::InitParams::SHADOW_TYPE_DROP;
  params->shadow_elevation = wm::ShadowElevation::LARGE;
}

int AppListView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

views::View* AppListView::GetInitiallyFocusedView() {
  return app_list_main_view_->search_box_view()->search_box();
}

bool AppListView::WidgetHasHitTestMask() const {
  return GetBubbleFrameView() != nullptr;
}

void AppListView::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  DCHECK(GetBubbleFrameView());

  mask->addRect(gfx::RectToSkRect(GetBubbleFrameView()->GetContentsBounds()));
}

void AppListView::OnScrollEvent(ui::ScrollEvent* event) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  if (event->type() == ui::ET_SCROLL_FLING_CANCEL)
    return;

  if (!HandleScroll(event))
    return;

  event->SetHandled();
  event->StopPropagation();
}

void AppListView::OnMouseEvent(ui::MouseEvent* event) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      event->SetHandled();
      HandleClickOrTap(event);
      break;
    case ui::ET_MOUSEWHEEL:
      if (HandleScroll(event))
        event->SetHandled();
      break;
    default:
      break;
  }
}

void AppListView::OnGestureEvent(ui::GestureEvent* event) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      SetIsInDrag(false);
      event->SetHandled();
      HandleClickOrTap(event);
      break;
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (is_side_shelf_)
        return;
      // There may be multiple scroll begin events in one drag because the
      // relative location of the finger and widget is almost unchanged and
      // scroll begin event occurs when the relative location changes beyond a
      // threshold. So avoid resetting the initial drag point in drag.
      if (!is_in_drag_)
        StartDrag(event->location());
      SetIsInDrag(true);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (is_side_shelf_)
        return;
      SetIsInDrag(true);
      last_fling_velocity_ = event->details().scroll_y();
      UpdateDrag(event->location());
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      if (!is_in_drag_)
        break;
      if (is_side_shelf_)
        return;
      SetIsInDrag(false);
      EndDrag(event->location());
      event->SetHandled();
      break;
    case ui::ET_MOUSEWHEEL: {
      if (HandleScroll(event))
        event->SetHandled();
      break;
    }
    default:
      break;
  }
}

void AppListView::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(!is_fullscreen_app_list_enabled_);

  BubbleDialogDelegateView::OnWidgetDestroying(widget);
  if (delegate_ && widget == GetWidget())
    delegate_->ViewClosing();
}

void AppListView::OnWidgetVisibilityChanged(views::Widget* widget,
                                            bool visible) {
  DCHECK(!is_fullscreen_app_list_enabled_);

  BubbleDialogDelegateView::OnWidgetVisibilityChanged(widget, visible);

  if (widget != GetWidget())
    return;

  if (!visible)
    app_list_main_view_->ResetForShow();
}

bool AppListView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());

  // If the ContentsView does not handle the back action, then this is the
  // top level, so we close the app list.
  if (!app_list_main_view_->contents_view()->Back()) {
    if (is_fullscreen_app_list_enabled_) {
      SetState(CLOSED);
    } else {
      app_list_main_view_->Close();
      delegate_->Dismiss();
    }
    GetWidget()->Deactivate();
  }

  // Don't let DialogClientView handle the accelerator.
  return true;
}

void AppListView::Layout() {
  const gfx::Rect contents_bounds = GetContentsBounds();

  // Make sure to layout |app_list_main_view_| and |speech_view_| at the
  // center of the widget.
  gfx::Rect centered_bounds = contents_bounds;
  ContentsView* contents_view = app_list_main_view_->contents_view();
  centered_bounds.ClampToCenteredSize(
      gfx::Size(is_fullscreen_app_list_enabled_
                    ? contents_view->GetMaximumContentsSize().width()
                    : contents_view->GetDefaultContentsBounds().width(),
                contents_bounds.height()));

  app_list_main_view_->SetBoundsRect(centered_bounds);

  if (speech_view_) {
    gfx::Rect speech_bounds = centered_bounds;
    int preferred_height = speech_view_->GetPreferredSize().height();
    speech_bounds.Inset(kSpeechUIMargin, kSpeechUIMargin);
    speech_bounds.set_height(
        std::min(speech_bounds.height(), preferred_height));
    speech_bounds.Inset(-speech_view_->GetInsets());
    speech_view_->SetBoundsRect(speech_bounds);
  }

  if (!is_fullscreen_app_list_enabled_)
    return;
  contents_view->Layout();
  app_list_background_shield_->SetBoundsRect(contents_bounds);
}

void AppListView::SchedulePaintInRect(const gfx::Rect& rect) {
  BubbleDialogDelegateView::SchedulePaintInRect(rect);
  if (GetBubbleFrameView())
    GetBubbleFrameView()->SchedulePaint();
}

void AppListView::OnTabletModeChanged(bool started) {
  is_tablet_mode_ = started;
  search_box_view_->OnTabletModeChanged(started);
  model_->SetTabletMode(started);
  if (is_tablet_mode_ && !is_fullscreen()) {
    // Set |app_list_state_| to a tablet mode friendly state.
    SetState(app_list_state_ == PEEKING ? FULLSCREEN_ALL_APPS
                                        : FULLSCREEN_SEARCH);
  }
}

bool AppListView::HandleScroll(const ui::Event* event) {
  if (app_list_state_ != PEEKING)
    return false;

  switch (event->type()) {
    case ui::ET_MOUSEWHEEL:
      SetState(event->AsMouseWheelEvent()->y_offset() < 0 ? FULLSCREEN_ALL_APPS
                                                          : CLOSED);
      if (app_list_state_ == FULLSCREEN_ALL_APPS) {
        UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                  kMousewheelScroll, kMaxPeekingToFullscreen);
      }
      // Return true unconditionally because all mousewheel events are large
      // enough to transition the app list.
      return true;
    case ui::ET_SCROLL:
    case ui::ET_SCROLL_FLING_START: {
      if (fabs(event->AsScrollEvent()->y_offset()) >
          kAppListMinScrollToSwitchStates) {
        SetState(event->AsScrollEvent()->y_offset() < 0 ? FULLSCREEN_ALL_APPS
                                                        : CLOSED);
        if (app_list_state_ == FULLSCREEN_ALL_APPS) {
          UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                    kMousepadScroll, kMaxPeekingToFullscreen);
        }
        return true;
      }
      break;
    }
    default:
      break;
  }
  return false;
}

void AppListView::SetState(AppListState new_state) {
  AppListState new_state_override = new_state;
  if (is_side_shelf_ || is_tablet_mode_) {
    // If side shelf or tablet mode are active, all transitions should be
    // made to the tablet mode/side shelf friendly versions.
    if (new_state == HALF) {
      new_state_override = FULLSCREEN_SEARCH;
    } else if (new_state == PEEKING) {
      // If the old state was already FULLSCREEN_ALL_APPS, then we should close.
      new_state_override =
          app_list_state_ == FULLSCREEN_ALL_APPS ? CLOSED : FULLSCREEN_ALL_APPS;
    }
  }

  switch (new_state_override) {
    case PEEKING: {
      switch (app_list_state_) {
        case HALF:
        case PEEKING:
        case FULLSCREEN_ALL_APPS: {
          app_list_main_view_->contents_view()->SetActiveState(
              AppListModel::STATE_START);
          // Set the apps to first page at STATE_START state.
          PaginationModel* pagination_model = GetAppsPaginationModel();
          if (pagination_model->total_pages() > 0 &&
              pagination_model->selected_page() != 0) {
            pagination_model->SelectPage(0, false /* animate */);
          }
          break;
        }
        case FULLSCREEN_SEARCH:
        case CLOSED:
          NOTREACHED();
          break;
      }
      break;
    }
    case HALF:
      break;
    case FULLSCREEN_ALL_APPS: {
      // Set timer to ignore further scroll events for this transition.
      GetAppsGridView()->StartTimerToIgnoreScrollEvents();

      AppsContainerView* apps_container_view =
          app_list_main_view_->contents_view()->apps_container_view();

      if (apps_container_view->IsInFolderView())
        apps_container_view->app_list_folder_view()->CloseFolderPage();

      app_list_main_view_->contents_view()->SetActiveState(
          AppListModel::STATE_APPS, !is_side_shelf_);
      break;
    }
    case FULLSCREEN_SEARCH:
      break;
    case CLOSED:
      switch (app_list_state_) {
        case CLOSED:
          return;
        case HALF:
        case PEEKING:
        case FULLSCREEN_ALL_APPS:
        case FULLSCREEN_SEARCH:
          delegate_->Dismiss();
          app_list_main_view_->Close();
          break;
      }
  }
  StartAnimationForState(new_state_override);
  RecordStateTransitionForUma(new_state_override);
  model_->SetStateFullscreen(new_state_override);
  app_list_state_ = new_state_override;

  // Updates the visibility of app list items according to the change of
  // |app_list_state_|.
  app_list_main_view_->contents_view()
      ->apps_container_view()
      ->apps_grid_view()
      ->UpdateControlVisibility(app_list_state_, is_in_drag_);
}

void AppListView::StartAnimationForState(AppListState target_state) {
  if (is_side_shelf_)
    return;

  const int display_height = GetDisplayNearestView().size().height();
  int target_state_y = 0;

  switch (target_state) {
    case PEEKING:
      target_state_y = display_height - kPeekingAppListHeight;
      break;
    case HALF:
      target_state_y = display_height - kHalfAppListHeight;
      break;
    case CLOSED:
      // The close animation is handled by the delegate.
      return;
    default:
      break;
  }

  gfx::Rect target_bounds = fullscreen_widget_->GetNativeView()->bounds();
  target_bounds.set_y(target_state_y);

  std::unique_ptr<ui::LayerAnimationElement> bounds_animation_element =
      ui::LayerAnimationElement::CreateBoundsElement(
          target_bounds,
          base::TimeDelta::FromMilliseconds(app_list_animation_duration_ms_));

  bounds_animation_element->set_tween_type(gfx::Tween::EASE_OUT);

  ui::LayerAnimator* animator = fullscreen_widget_->GetLayer()->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animator->StopAnimating();
  animator->ScheduleAnimation(
      new ui::LayerAnimationSequence(std::move(bounds_animation_element)));
}

void AppListView::StartCloseAnimation(base::TimeDelta animation_duration) {
  DCHECK(is_fullscreen_app_list_enabled_);
  if (is_side_shelf_ || !is_fullscreen_app_list_enabled_)
    return;

  app_list_main_view_->contents_view()->FadeOutOnClose(animation_duration);
}

void AppListView::SetStateFromSearchBoxView(bool search_box_is_empty) {
  switch (app_list_state_) {
    case PEEKING:
      if (!search_box_is_empty)
        SetState(HALF);
      break;
    case HALF:
      if (search_box_is_empty)
        SetState(PEEKING);
      break;
    case FULLSCREEN_SEARCH:
      if (search_box_is_empty) {
        SetState(FULLSCREEN_ALL_APPS);
        app_list_main_view()->contents_view()->SetActiveState(
            AppListModel::State::STATE_APPS);
      }
      break;
    case FULLSCREEN_ALL_APPS:
      if (!search_box_is_empty)
        SetState(FULLSCREEN_SEARCH);
      break;
    case CLOSED:
      NOTREACHED();
      break;
  }
}

void AppListView::UpdateYPositionAndOpacity(int y_position_in_screen,
                                            float background_opacity) {
  SetIsInDrag(true);
  background_opacity_ = background_opacity;
  gfx::Rect new_widget_bounds = fullscreen_widget_->GetWindowBoundsInScreen();
  app_list_y_position_in_screen_ =
      std::max(y_position_in_screen, GetDisplayNearestView().bounds().y());
  new_widget_bounds.set_y(app_list_y_position_in_screen_);
  gfx::NativeView native_view = fullscreen_widget_->GetNativeView();
  ::wm::ConvertRectFromScreen(native_view->parent(), &new_widget_bounds);
  native_view->SetBounds(new_widget_bounds);

  DraggingLayout();
}

PaginationModel* AppListView::GetAppsPaginationModel() const {
  return GetAppsGridView()->pagination_model();
}

gfx::Rect AppListView::GetAppInfoDialogBounds() const {
  if (!is_fullscreen_app_list_enabled_)
    return GetBoundsInScreen();
  gfx::Rect app_info_bounds(GetDisplayNearestView().bounds());
  app_info_bounds.ClampToCenteredSize(
      gfx::Size(kAppInfoDialogWidth, kAppInfoDialogHeight));
  return app_info_bounds;
}

void AppListView::SetIsInDrag(bool is_in_drag) {
  if (is_in_drag == is_in_drag_)
    return;

  is_in_drag_ = is_in_drag;
  app_list_main_view_->contents_view()
      ->apps_container_view()
      ->apps_grid_view()
      ->UpdateControlVisibility(app_list_state_, is_in_drag_);
}

int AppListView::GetWorkAreaBottom() {
  return fullscreen_widget_->GetWorkAreaBoundsInScreen().bottom();
}

void AppListView::DraggingLayout() {
  float shield_opacity =
      is_background_blur_enabled_ ? kAppListOpacityWithBlur : kAppListOpacity;
  app_list_background_shield_->layer()->SetOpacity(
      is_in_drag_ ? background_opacity_ : shield_opacity);

  // Updates the opacity of the items in the app list.
  search_box_view_->UpdateOpacity();
  GetAppsGridView()->UpdateOpacity();

  Layout();
}

void AppListView::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
  if (!speech_view_)
    return;

  bool will_appear = (new_state == SPEECH_RECOGNITION_RECOGNIZING ||
                      new_state == SPEECH_RECOGNITION_IN_SPEECH ||
                      new_state == SPEECH_RECOGNITION_NETWORK_ERROR);
  // No change for this class.
  if (speech_view_->visible() == will_appear)
    return;

  if (will_appear)
    speech_view_->Reset();

  animation_observer_->set_frame(GetBubbleFrameView());
  gfx::Transform speech_transform;
  speech_transform.Translate(0, SkFloatToMScalar(kSpeechUIAppearingPosition));
  if (will_appear)
    speech_view_->layer()->SetTransform(speech_transform);

  {
    ui::ScopedLayerAnimationSettings main_settings(
        app_list_main_view_->layer()->GetAnimator());
    if (will_appear) {
      animation_observer_->SetTarget(app_list_main_view_);
      main_settings.AddObserver(animation_observer_.get());
    }
    app_list_main_view_->layer()->SetOpacity(will_appear ? 0.0f : 1.0f);
  }

  {
    ui::ScopedLayerAnimationSettings search_box_settings(
        search_box_widget_->GetLayer()->GetAnimator());
    search_box_widget_->GetLayer()->SetOpacity(will_appear ? 0.0f : 1.0f);
  }

  {
    ui::ScopedLayerAnimationSettings speech_settings(
        speech_view_->layer()->GetAnimator());
    if (!will_appear) {
      animation_observer_->SetTarget(speech_view_);
      speech_settings.AddObserver(animation_observer_.get());
    }

    speech_view_->layer()->SetOpacity(will_appear ? 1.0f : 0.0f);
    if (will_appear)
      speech_view_->layer()->SetTransform(gfx::Transform());
    else
      speech_view_->layer()->SetTransform(speech_transform);
  }

  // Prevent the search box from receiving events when hidden.
  search_box_view_->SetEnabled(!will_appear);

  if (will_appear) {
    speech_view_->SetVisible(true);
  } else {
    app_list_main_view_->SetVisible(true);

    // Refocus the search box. However, if the app list widget does not
    // have focus, it means another window has already taken focus, and we
    // *must not* focus the search box (or we would steal focus back into
    // the app list).
    if (GetWidget()->IsActive())
      search_box_view_->search_box()->RequestFocus();
  }
}

void AppListView::OnDisplayMetricsChanged(const display::Display& display,
                                          uint32_t changed_metrics) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  // Set the |fullscreen_widget_| size to fit the new display metrics.
  gfx::Size size = GetDisplayNearestView().size();
  fullscreen_widget_->SetSize(size);

  // Update the |fullscreen_widget_| bounds to accomodate the new work
  // area.
  SetState(app_list_state_);
}

float AppListView::GetAppListBackgroundOpacityDuringDragging() {
  float top_of_applist = fullscreen_widget_->GetWindowBoundsInScreen().y();
  float dragging_height = std::max((GetWorkAreaBottom() - top_of_applist), 0.f);
  float coefficient =
      std::min(dragging_height / (kNumOfShelfSize * kShelfSize), 1.0f);
  float shield_opacity =
      is_background_blur_enabled_ ? kAppListOpacityWithBlur : kAppListOpacity;
  return coefficient * shield_opacity;
}

void AppListView::GetWallpaperProminentColors(std::vector<SkColor>* colors) {
  delegate_->GetWallpaperProminentColors(colors);
}

void AppListView::OnWallpaperColorsChanged() {
  SetBackgroundShieldColor();
}

void AppListView::SetBackgroundShieldColor() {
  // There is a chance when AppListView::OnWallpaperColorsChanged is called from
  // AppListViewDelegate, the |app_list_background_shield_| is not initialized.
  if (!is_fullscreen_app_list_enabled_ || !app_list_background_shield_)
    return;

  std::vector<SkColor> prominent_colors;
  GetWallpaperProminentColors(&prominent_colors);
  app_list_background_shield_->layer()->SetColor(
      GetBackgroundShieldColor(prominent_colors));
}

}  // namespace app_list
