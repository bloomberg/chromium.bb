// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_view.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/app_list/model/app_list_model.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_util.h"
#include "ui/app_list/assistant_interaction_model.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
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
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"

using ash::ColorProfileType;

namespace app_list {

namespace {

// The height of the half app list from the bottom of the screen.
constexpr int kHalfAppListHeight = 561;

// The fraction of app list height that the app list must be released at in
// order to transition to the next state.
constexpr int kAppListThresholdDenominator = 3;

// The velocity the app list must be dragged in order to transition to the next
// state, measured in DIPs/event.
constexpr int kAppListDragVelocityThreshold = 6;

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

// The animation duration for app list movement.
constexpr float kAppListAnimationDurationTestMs = 0;
constexpr float kAppListAnimationDurationMs = 200;
constexpr float kAppListAnimationDurationFromFullscreenMs = 250;

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

class StateAnimationMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  StateAnimationMetricsReporter() = default;
  ~StateAnimationMetricsReporter() override = default;

  void Report(int value) override {
    UMA_HISTOGRAM_PERCENTAGE("Apps.StateTransition.AnimationSmoothness", value);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StateAnimationMetricsReporter);
};

// An animation observer to decide whether to ignore scroll events.
class ScrollAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit ScrollAnimationObserver(base::WeakPtr<AppListView> view)
      : view_(view) {}
  ~ScrollAnimationObserver() override = default;

 private:
  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (view_)
      view_->SetIsIgnoringScrollEvents(false);
  }

  base::WeakPtr<AppListView> view_;

  DISALLOW_COPY_AND_ASSIGN(ScrollAnimationObserver);
};

}  // namespace

// An animation observer to hide the view at the end of the animation.
class HideViewAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  HideViewAnimationObserver() : target_(NULL) {}

  ~HideViewAnimationObserver() override {
    if (target_)
      StopObservingImplicitAnimations();
  }

  void SetTarget(views::View* target) {
    if (target_)
      StopObservingImplicitAnimations();
    target_ = target;
  }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (target_) {
      target_->SetVisible(false);
      target_ = NULL;
    }
  }

  views::View* target_;

  DISALLOW_COPY_AND_ASSIGN(HideViewAnimationObserver);
};

////////////////////////////////////////////////////////////////////////////////
// AppListView:

AppListView::AppListView(AppListViewDelegate* delegate)
    : delegate_(delegate),
      model_(delegate->GetModel()),
      search_model_(delegate->GetSearchModel()),
      short_animations_for_testing_(false),
      is_background_blur_enabled_(features::IsBackgroundBlurEnabled()),
      display_observer_(this),
      animation_observer_(new HideViewAnimationObserver()),
      previous_arrow_key_traversal_enabled_(
          views::FocusManager::arrow_key_traversal_enabled()),
      state_animation_metrics_reporter_(
          std::make_unique<StateAnimationMetricsReporter>()),
      weak_ptr_factory_(this) {
  CHECK(delegate);

  display_observer_.Add(display::Screen::GetScreen());
  delegate_->AddObserver(this);
  // Enable arrow key in FocusManager. Arrow left/right and up/down triggers
  // the same focus movement as tab/shift+tab.
  views::FocusManager::set_arrow_key_traversal_enabled(true);

  scroll_animation_observer_.reset(
      new ScrollAnimationObserver(weak_ptr_factory_.GetWeakPtr()));
}

AppListView::~AppListView() {
  delegate_->RemoveObserver(this);

  animation_observer_.reset();
  // Remove child views first to ensure no remaining dependencies on delegate_.
  RemoveAllChildViews(true);
  views::FocusManager::set_arrow_key_traversal_enabled(
      previous_arrow_key_traversal_enabled_);
}

// static
void AppListView::ExcludeWindowFromEventHandling(aura::Window* window) {
  DCHECK(window);
  window->SetProperty(kExcludeWindowFromEventHandling, true);
}

void AppListView::Initialize(const InitParams& params) {
  base::Time start_time = base::Time::Now();
  is_tablet_mode_ = params.is_tablet_mode;
  is_side_shelf_ = params.is_side_shelf;
  assistant_interaction_model_ = params.assistant_interaction_model;
  InitContents(params.initial_apps_page);
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_NONE));
  parent_window_ = params.parent;

  InitializeFullscreen(params.parent, params.parent_container_id);

  InitChildWidgets();
  AddChildView(overlay_view_);

  SetState(app_list_state_);

  UMA_HISTOGRAM_TIMES(kAppListCreationTimeHistogram,
                      base::Time::Now() - start_time);
  RecordFolderMetrics();
}

void AppListView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  app_list_main_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppListView::ShowWhenReady() {
  app_list_main_view_->ShowAppListWhenReady();
}

void AppListView::Dismiss() {
  app_list_main_view_->Close();
  delegate_->Dismiss();
  SetState(AppListViewState::CLOSED);
  GetWidget()->Deactivate();
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
  views::WidgetDelegateView::OnPaint(canvas);
  if (!next_paint_callback_.is_null()) {
    next_paint_callback_.Run();
    next_paint_callback_.Reset();
  }
}

const char* AppListView::GetClassName() const {
  return "AppListView";
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
    if (view_->app_list_state() != AppListViewState::CLOSED)
      view_->SetState(AppListViewState::CLOSED);
    widget_observer_.Remove(view_->GetWidget());
  }

 private:
  app_list::AppListView* view_;
  ScopedObserver<views::Widget, WidgetObserver> widget_observer_;
  DISALLOW_COPY_AND_ASSIGN(FullscreenWidgetObserver);
};

void AppListView::InitContents(int initial_apps_page) {
  // The shield view that colors/blurs the background of the app list and
  // makes it transparent.
  app_list_background_shield_ = new views::View;
  app_list_background_shield_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  app_list_background_shield_->layer()->SetOpacity(
      is_background_blur_enabled_ ? kAppListOpacityWithBlur : kAppListOpacity);
  SetBackgroundShieldColor();
  if (is_background_blur_enabled_) {
    app_list_background_shield_->layer()->SetBackgroundBlur(kAppListBlurRadius);
  }
  AddChildView(app_list_background_shield_);
  app_list_main_view_ = new AppListMainView(delegate_, this);
  AddChildView(app_list_main_view_);
  app_list_main_view_->SetPaintToLayer();
  app_list_main_view_->layer()->SetFillsBoundsOpaquely(false);
  app_list_main_view_->layer()->SetMasksToBounds(true);
  // This will be added to the |search_box_widget_| after the app list widget is
  // initialized.
  search_box_view_ = new SearchBoxView(app_list_main_view_, delegate_, this);
  search_box_view_->Init();

  app_list_main_view_->Init(0, search_box_view_);
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
  search_box_widget_params.delegate = search_box_view_;

  // Create a widget for the SearchBoxView to live in. This allows the
  // SearchBoxView to be on top of the custom launcher page's WebContents
  // (otherwise the search box events will be captured by the WebContents).
  search_box_widget_ = new views::Widget;
  search_box_widget_->Init(search_box_widget_params);

  // Assign an accessibility role to the native window of search box widget, so
  // that hitting search+right could move ChromeVox focus across search box to
  // other elements in app list view.
  search_box_widget_->GetNativeWindow()->SetProperty(
      ui::kAXRoleOverride,
      static_cast<ax::mojom::Role>(ax::mojom::Role::kGroup));

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
                                       int parent_container_id) {
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

  // The app list container fills the screen, so convert to local coordinates.
  gfx::Rect local_bounds = app_list_overlay_view_bounds;
  local_bounds -= display_nearest_view.bounds().OffsetFromOrigin();

  fullscreen_widget_ = new views::Widget;
  views::Widget::InitParams app_list_overlay_view_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);

  app_list_overlay_view_params.name = "AppList";
  if (parent) {
    app_list_overlay_view_params.parent = parent;
  } else {
    // Under mash, the app list is owned by the browser process, which cannot
    // directly access the ash window container hierarchy to set |parent|.
    // TODO(jamescook): Remove this when app_list moves into //ash as |parent|
    // will not be null.
    app_list_overlay_view_params
        .mus_properties[ui::mojom::WindowManager::kContainerId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(parent_container_id);
    app_list_overlay_view_params
        .mus_properties[ui::mojom::WindowManager::kDisplayId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(display_nearest_view.id());
    app_list_overlay_view_params.bounds = local_bounds;
  }
  app_list_overlay_view_params.delegate = this;
  app_list_overlay_view_params.opacity =
      views::Widget::InitParams::TRANSLUCENT_WINDOW;
  app_list_overlay_view_params.layer_type = ui::LAYER_SOLID_COLOR;
  fullscreen_widget_->Init(app_list_overlay_view_params);
  fullscreen_widget_->GetNativeWindow()->SetEventTargeter(
      std::make_unique<AppListEventTargeter>());

  // The widget's initial position will be off the bottom of the display.
  // Set native view's bounds directly to avoid screen position controller
  // setting bounds in the display where the widget has the largest
  // intersection.
  // TODO(mash): Redesign this animation to position the widget to cover the
  // entire screen, then animate the layer up into position. crbug.com/768437
  fullscreen_widget_->GetNativeView()->SetBounds(local_bounds);

  overlay_view_ = new AppListOverlayView(0 /* no corners */);

  widget_observer_ = std::unique_ptr<FullscreenWidgetObserver>(
      new FullscreenWidgetObserver(this));
}

void AppListView::HandleClickOrTap(ui::LocatedEvent* event) {
  // Clear focus if the located event is not handled by any child view.
  GetFocusManager()->ClearFocus();

  // No-op if app list is on fullscreen all apps state and the event location is
  // near an app.
  if (app_list_state_ == AppListViewState::FULLSCREEN_ALL_APPS &&
      GetRootAppsGridView()->IsEventNearAppIcon(*event)) {
    return;
  }

  if (GetAppsContainerView()->IsInFolderView()) {
    // Close the folder if it is opened.
    GetAppsContainerView()->app_list_folder_view()->CloseFolderPage();
    return;
  }

  // No-op if the event was a right-click or two-finger tap.
  if ((event->IsGestureEvent() &&
       event->AsGestureEvent()->type() == ui::ET_GESTURE_TWO_FINGER_TAP) ||
      (event->IsMouseEvent() &&
       event->AsMouseEvent()->IsOnlyRightMouseButton())) {
    return;
  }

  if (!search_box_view_->is_search_box_active()) {
    Dismiss();
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
  if (app_list_state_ == AppListViewState::PEEKING)
    drag_started_from_peeking_ = true;
}

void AppListView::UpdateDrag(const gfx::Point& location) {
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
  if (app_list_state_ == AppListViewState::CLOSED)
    return;

  // Change the app list state based on where the drag ended. If fling velocity
  // was over the threshold, snap to the next state in the direction of the
  // fling.
  if (std::abs(last_fling_velocity_) >= kAppListDragVelocityThreshold) {
    // If the user releases drag with velocity over the threshold, snap to
    // the next state, ignoring the drag release position.

    if (last_fling_velocity_ > 0) {
      switch (app_list_state_) {
        case AppListViewState::PEEKING:
        case AppListViewState::HALF:
        case AppListViewState::FULLSCREEN_SEARCH:
        case AppListViewState::FULLSCREEN_ALL_APPS:
          Dismiss();
          break;
        case AppListViewState::CLOSED:
          NOTREACHED();
          break;
      }
    } else {
      switch (app_list_state_) {
        case AppListViewState::FULLSCREEN_ALL_APPS:
        case AppListViewState::FULLSCREEN_SEARCH:
          SetState(app_list_state_);
          break;
        case AppListViewState::HALF:
          SetState(AppListViewState::FULLSCREEN_SEARCH);
          break;
        case AppListViewState::PEEKING:
          UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                    kSwipe, kMaxPeekingToFullscreen);
          SetState(AppListViewState::FULLSCREEN_ALL_APPS);
          break;
        case AppListViewState::CLOSED:
          NOTREACHED();
          break;
      }
    }
  } else {
    const int display_height = GetDisplayNearestView().size().height();
    int app_list_y_for_state = 0;
    int app_list_height = 0;
    switch (app_list_state_) {
      case AppListViewState::FULLSCREEN_ALL_APPS:
      case AppListViewState::FULLSCREEN_SEARCH:
        app_list_y_for_state = 0;
        app_list_height = display_height;
        break;
      case AppListViewState::HALF:
        app_list_y_for_state = display_height - kHalfAppListHeight;
        app_list_height = kHalfAppListHeight;
        break;
      case AppListViewState::PEEKING:
        app_list_y_for_state = display_height - kPeekingAppListHeight;
        app_list_height = kPeekingAppListHeight;
        break;
      case AppListViewState::CLOSED:
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
      Dismiss();
      return;
    }
    switch (app_list_state_) {
      case AppListViewState::FULLSCREEN_ALL_APPS:
        if (drag_delta < -app_list_threshold) {
          if (is_tablet_mode_ || is_side_shelf_)
            Dismiss();
          else
            SetState(AppListViewState::PEEKING);
        } else {
          SetState(app_list_state_);
        }
        break;
      case AppListViewState::FULLSCREEN_SEARCH:
        if (drag_delta < -app_list_threshold)
          Dismiss();
        else
          SetState(app_list_state_);
        break;
      case AppListViewState::HALF:
        if (drag_delta > app_list_threshold)
          SetState(AppListViewState::FULLSCREEN_SEARCH);
        else if (drag_delta < -app_list_threshold)
          Dismiss();
        else
          SetState(app_list_state_);
        break;
      case AppListViewState::PEEKING:
        if (drag_delta > app_list_threshold) {
          SetState(AppListViewState::FULLSCREEN_ALL_APPS);
          UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                    kSwipe, kMaxPeekingToFullscreen);
        } else if (drag_delta < -app_list_threshold) {
          Dismiss();
        } else {
          SetState(app_list_state_);
        }
        break;
      case AppListViewState::CLOSED:
        NOTREACHED();
        break;
    }
  }
  drag_started_from_peeking_ = false;
  DraggingLayout();
  initial_drag_point_ = gfx::Point();
}

void AppListView::SetChildViewsForStateTransition(
    AppListViewState target_state) {
  if (target_state != AppListViewState::PEEKING &&
      target_state != AppListViewState::FULLSCREEN_ALL_APPS)
    return;

  if (GetAppsContainerView()->IsInFolderView())
    GetAppsContainerView()->ResetForShowApps();

  if (target_state == AppListViewState::PEEKING) {
    app_list_main_view_->contents_view()->SetActiveState(
        ash::AppListState::kStateStart);
    // Set the apps to first page at STATE_START state.
    PaginationModel* pagination_model = GetAppsPaginationModel();
    if (pagination_model->total_pages() > 0 &&
        pagination_model->selected_page() != 0) {
      pagination_model->SelectPage(0, false /* animate */);
    }
  } else {
    app_list_main_view_->contents_view()->SetActiveState(
        ash::AppListState::kStateApps, !is_side_shelf_);
  }
}

void AppListView::ConvertAppListStateToFullscreenEquivalent(
    AppListViewState* target_state) {
  if (!(is_side_shelf_ || is_tablet_mode_))
    return;

  // If side shelf or tablet mode are active, all transitions should be
  // made to the tablet mode/side shelf friendly versions.
  if (*target_state == AppListViewState::HALF) {
    *target_state = AppListViewState::FULLSCREEN_SEARCH;
  } else if (*target_state == AppListViewState::PEEKING) {
    // FULLSCREEN_ALL_APPS->PEEKING in tablet/side shelf mode should close
    // instead of going to PEEKING.
    *target_state = app_list_state_ == AppListViewState::FULLSCREEN_ALL_APPS
                        ? AppListViewState::CLOSED
                        : AppListViewState::FULLSCREEN_ALL_APPS;
  }
}

void AppListView::RecordStateTransitionForUma(AppListViewState new_state) {
  AppListStateTransitionSource transition =
      GetAppListStateTransitionSource(new_state);
  // kMaxAppListStateTransition denotes a transition we are not interested in
  // recording (ie. PEEKING->PEEKING).
  if (transition == kMaxAppListStateTransition)
    return;

  UMA_HISTOGRAM_ENUMERATION(kAppListStateTransitionSourceHistogram, transition,
                            kMaxAppListStateTransition);

  switch (transition) {
    case kPeekingToFullscreenAllApps:
    case KHalfToFullscreenSearch:
      base::RecordAction(base::UserMetricsAction("AppList_PeekingToFull"));
      break;

    case kFullscreenAllAppsToPeeking:
      base::RecordAction(base::UserMetricsAction("AppList_FullToPeeking"));
      break;

    default:
      break;
  }
}

void AppListView::MaybeCreateAccessibilityEvent(AppListViewState new_state) {
  if (new_state != AppListViewState::PEEKING &&
      new_state != AppListViewState::FULLSCREEN_ALL_APPS)
    return;

  DCHECK(state_announcement_ == base::string16());

  if (new_state == AppListViewState::PEEKING) {
    state_announcement_ = l10n_util::GetStringUTF16(
        IDS_APP_LIST_SUGGESTED_APPS_ACCESSIBILITY_ANNOUNCEMENT);
  } else {
    state_announcement_ = l10n_util::GetStringUTF16(
        IDS_APP_LIST_ALL_APPS_ACCESSIBILITY_ANNOUNCEMENT);
  }
  NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  state_announcement_ = base::string16();
}

display::Display AppListView::GetDisplayNearestView() const {
  return display::Screen::GetScreen()->GetDisplayNearestView(parent_window_);
}

AppsContainerView* AppListView::GetAppsContainerView() {
  return app_list_main_view_->contents_view()->GetAppsContainerView();
}

AppsGridView* AppListView::GetRootAppsGridView() {
  return GetAppsContainerView()->apps_grid_view();
}

AppsGridView* AppListView::GetFolderAppsGridView() {
  return GetAppsContainerView()->app_list_folder_view()->items_grid_view();
}

AppListStateTransitionSource AppListView::GetAppListStateTransitionSource(
    AppListViewState target_state) const {
  switch (app_list_state_) {
    case AppListViewState::CLOSED:
      // CLOSED->X transitions are not useful for UMA.
      return kMaxAppListStateTransition;
    case AppListViewState::PEEKING:
      switch (target_state) {
        case AppListViewState::CLOSED:
          return kPeekingToClosed;
        case AppListViewState::HALF:
          return kPeekingToHalf;
        case AppListViewState::FULLSCREEN_ALL_APPS:
          return kPeekingToFullscreenAllApps;
        case AppListViewState::PEEKING:
          // PEEKING->PEEKING is used when resetting the widget position after a
          // failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
        case AppListViewState::FULLSCREEN_SEARCH:
          // PEEKING->FULLSCREEN_SEARCH is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }
    case AppListViewState::HALF:
      switch (target_state) {
        case AppListViewState::CLOSED:
          return kHalfToClosed;
        case AppListViewState::PEEKING:
          return kHalfToPeeking;
        case AppListViewState::FULLSCREEN_SEARCH:
          return KHalfToFullscreenSearch;
        case AppListViewState::HALF:
          // HALF->HALF is used when resetting the widget position after a
          // failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
        case AppListViewState::FULLSCREEN_ALL_APPS:
          // HALF->FULLSCREEN_ALL_APPS is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }

    case AppListViewState::FULLSCREEN_ALL_APPS:
      switch (target_state) {
        case AppListViewState::CLOSED:
          return kFullscreenAllAppsToClosed;
        case AppListViewState::PEEKING:
          return kFullscreenAllAppsToPeeking;
        case AppListViewState::FULLSCREEN_SEARCH:
          return kFullscreenAllAppsToFullscreenSearch;
        case AppListViewState::HALF:
          // FULLSCREEN_ALL_APPS->HALF is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
        case AppListViewState::FULLSCREEN_ALL_APPS:
          // FULLSCREEN_ALL_APPS->FULLSCREEN_ALL_APPS is used when resetting the
          // widget positon after a failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
      }
    case AppListViewState::FULLSCREEN_SEARCH:
      switch (target_state) {
        case AppListViewState::CLOSED:
          return kFullscreenSearchToClosed;
        case AppListViewState::FULLSCREEN_ALL_APPS:
          return kFullscreenSearchToFullscreenAllApps;
        case AppListViewState::FULLSCREEN_SEARCH:
          // FULLSCREEN_SEARCH->FULLSCREEN_SEARCH is used when resetting the
          // widget position after a failed state transition. Not useful for
          // UMA.
          return kMaxAppListStateTransition;
        case AppListViewState::PEEKING:
          // FULLSCREEN_SEARCH->PEEKING is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
        case AppListViewState::HALF:
          // FULLSCREEN_SEARCH->HALF is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }
  }
}

views::View* AppListView::GetInitiallyFocusedView() {
  return app_list_main_view_->search_box_view()->search_box();
}

void AppListView::OnScrollEvent(ui::ScrollEvent* event) {
  if (!HandleScroll(event->y_offset(), event->type()))
    return;

  event->SetHandled();
  event->StopPropagation();
}

void AppListView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      event->SetHandled();
      HandleClickOrTap(event);
      break;
    case ui::ET_MOUSEWHEEL:
      if (HandleScroll(event->AsMouseWheelEvent()->offset().y(),
                       ui::ET_MOUSEWHEEL))
        event->SetHandled();
      break;
    default:
      break;
  }
}

void AppListView::OnGestureEvent(ui::GestureEvent* event) {
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
      if (HandleScroll(event->AsMouseWheelEvent()->offset().y(),
                       ui::ET_MOUSEWHEEL))
        event->SetHandled();
      break;
    }
    default:
      break;
  }
}

ax::mojom::Role AppListView::GetAccessibleWindowRole() const {
  // Default role of root view is ax::mojom::Role::kWindow which traps ChromeVox
  // focus within the root view. Assign ax::mojom::Role::kGroup here to allow
  // the focus to move from elements in app list view to search box.
  return ax::mojom::Role::kGroup;
}

bool AppListView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  switch (accelerator.key_code()) {
    case ui::VKEY_ESCAPE:
    case ui::VKEY_BROWSER_BACK:
      // If the ContentsView does not handle the back action, then this is the
      // top level, so we close the app list.
      if (!app_list_main_view_->contents_view()->Back()) {
        Dismiss();
      }
      break;
    default:
      NOTREACHED();
      return false;
  }

  // Don't let DialogClientView handle the accelerator.
  return true;
}

void AppListView::Layout() {
  const gfx::Rect contents_bounds = GetContentsBounds();

  // Make sure to layout |app_list_main_view_| at the center of the widget.
  gfx::Rect centered_bounds = contents_bounds;
  ContentsView* contents_view = app_list_main_view_->contents_view();
  centered_bounds.ClampToCenteredSize(
      gfx::Size(contents_view->GetMaximumContentsSize().width(),
                contents_bounds.height()));

  app_list_main_view_->SetBoundsRect(centered_bounds);

  contents_view->Layout();
  app_list_background_shield_->SetBoundsRect(contents_bounds);
}

void AppListView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(state_announcement_);
  node_data->role = ax::mojom::Role::kAlert;
}

void AppListView::OnKeyEvent(ui::KeyEvent* event) {
  RedirectKeyEventToSearchBox(event);
}

void AppListView::OnTabletModeChanged(bool started) {
  is_tablet_mode_ = started;
  search_box_view_->OnTabletModeChanged(started);
  search_model_->SetTabletMode(started);
  if (is_tablet_mode_ && !is_fullscreen()) {
    // Set |app_list_state_| to a tablet mode friendly state.
    SetState(app_list_state_ == AppListViewState::PEEKING
                 ? AppListViewState::FULLSCREEN_ALL_APPS
                 : AppListViewState::FULLSCREEN_SEARCH);
  }
}

bool AppListView::HandleScroll(int offset, ui::EventType type) {
  // Ignore 0-offset events to prevent spurious dismissal, see crbug.com/806338
  // The system generates 0-offset ET_SCROLL_FLING_CANCEL events during simple
  // touchpad mouse moves. Those may be passed via mojo APIs and handled here.
  if (offset == 0 || is_in_drag() || is_ignoring_scroll_events_)
    return false;

  if (app_list_state_ != AppListViewState::PEEKING &&
      app_list_state_ != AppListViewState::FULLSCREEN_ALL_APPS)
    return false;

  // Let the Apps grid view handle the event first in FULLSCREEN_ALL_APPS.
  if (app_list_state_ == AppListViewState::FULLSCREEN_ALL_APPS) {
    AppsGridView* apps_grid_view = GetAppsContainerView()->IsInFolderView()
                                       ? GetFolderAppsGridView()
                                       : GetRootAppsGridView();
    if (apps_grid_view->HandleScrollFromAppListView(offset, type))
      return true;
  }

  // If the event is a mousewheel event, the offset is always large enough,
  // otherwise the offset must be larger than the scroll threshold.
  if (type == ui::ET_MOUSEWHEEL ||
      abs(offset) > kAppListMinScrollToSwitchStates) {
    if (offset > 0) {
      Dismiss();
    } else {
      if (app_list_state_ == AppListViewState::FULLSCREEN_ALL_APPS)
        return true;
      SetState(AppListViewState::FULLSCREEN_ALL_APPS);
      const AppListPeekingToFullscreenSource source =
          type == ui::ET_MOUSEWHEEL ? kMousewheelScroll : kMousepadScroll;
      UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram, source,
                                kMaxPeekingToFullscreen);
    }
  }
  return true;
}

void AppListView::SetState(AppListViewState new_state) {
  // Do not allow the state to be changed once it has been set to CLOSED.
  if (app_list_state_ == AppListViewState::CLOSED)
    return;

  AppListViewState new_state_override = new_state;
  ConvertAppListStateToFullscreenEquivalent(&new_state_override);
  MaybeCreateAccessibilityEvent(new_state_override);
  SetChildViewsForStateTransition(new_state_override);
  StartAnimationForState(new_state_override);
  RecordStateTransitionForUma(new_state_override);
  model_->SetStateFullscreen(new_state_override);
  app_list_state_ = new_state_override;
  if (new_state_override == AppListViewState::CLOSED) {
    return;
  }

  // Reset the focus to initially focused view. This should be done before
  // updating visibility of views, because setting focused view invisible
  // automatically moves focus to next focusable view, which potentially
  // causes bugs.
  GetInitiallyFocusedView()->RequestFocus();

  // Updates the visibility of app list items according to the change of
  // |app_list_state_|.
  GetAppsContainerView()->UpdateControlVisibility(app_list_state_, is_in_drag_);
}

void AppListView::StartAnimationForState(AppListViewState target_state) {
  if (is_side_shelf_)
    return;

  const display::Display display = GetDisplayNearestView();
  const int display_height = display.size().height();
  int target_state_y = 0;

  switch (target_state) {
    case AppListViewState::PEEKING:
      target_state_y = display_height - kPeekingAppListHeight;
      break;
    case AppListViewState::HALF:
      target_state_y = std::max(0, display_height - kHalfAppListHeight);
      break;
    case AppListViewState::FULLSCREEN_ALL_APPS:
    case AppListViewState::FULLSCREEN_SEARCH:
      // The ChromeVox panel as well as the Docked Magnifier viewport affect the
      // workarea of the display. We need to account for that when applist is in
      // fullscreen to avoid being shown below them.
      target_state_y = display.work_area().y() - display.bounds().y();
      break;

    case AppListViewState::CLOSED:
      // The close animation is handled by the delegate.
      return;
    default:
      break;
  }

  gfx::Rect target_bounds = fullscreen_widget_->GetNativeView()->bounds();
  const int original_state_y = target_bounds.origin().y();
  target_bounds.set_y(target_state_y);

  int animation_duration;
  // If animating to or from a fullscreen state, animate over 250ms, else
  // animate over 200 ms.
  if (short_animations_for_testing_) {
    animation_duration = kAppListAnimationDurationTestMs;
  } else if (is_fullscreen() ||
             target_state == AppListViewState::FULLSCREEN_ALL_APPS ||
             target_state == AppListViewState::FULLSCREEN_SEARCH) {
    animation_duration = kAppListAnimationDurationFromFullscreenMs;
  } else {
    animation_duration = kAppListAnimationDurationMs;
  }

  if (fullscreen_widget_->GetNativeView()->bounds().y() ==
      display.work_area().bottom()) {
    // If the animation start position is the bottom of the screen activate the
    // fade in animation.
    app_list_main_view_->contents_view()->FadeInOnOpen(
        base::TimeDelta::FromMilliseconds(animation_duration));
  }

  ui::Layer* layer = fullscreen_widget_->GetLayer();
  layer->SetBounds(target_bounds);
  gfx::Transform transform;
  transform.Translate(0, original_state_y - target_state_y);
  layer->SetTransform(transform);

  ui::LayerAnimator* animator = layer->GetAnimator();
  animator->StopAnimating();
  ui::ScopedLayerAnimationSettings settings(animator);
  settings.AddObserver(scroll_animation_observer_.get());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(animation_duration));
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetAnimationMetricsReporter(state_animation_metrics_reporter_.get());

  layer->SetTransform(gfx::Transform());

  SetIsIgnoringScrollEvents(true);
}

void AppListView::StartCloseAnimation(base::TimeDelta animation_duration) {
  if (is_side_shelf_)
    return;

  if (app_list_state_ != AppListViewState::CLOSED)
    SetState(AppListViewState::CLOSED);

  app_list_main_view_->contents_view()->FadeOutOnClose(animation_duration);
}

void AppListView::SetStateFromSearchBoxView(bool search_box_is_empty) {
  switch (app_list_state_) {
    case AppListViewState::PEEKING:
      if (!search_box_is_empty)
        SetState(AppListViewState::HALF);
      break;
    case AppListViewState::HALF:
      if (search_box_is_empty)
        SetState(AppListViewState::PEEKING);
      break;
    case AppListViewState::FULLSCREEN_SEARCH:
      if (search_box_is_empty) {
        SetState(AppListViewState::FULLSCREEN_ALL_APPS);
        app_list_main_view()->contents_view()->SetActiveState(
            ash::AppListState::kStateApps);
      }
      break;
    case AppListViewState::FULLSCREEN_ALL_APPS:
      if (!search_box_is_empty)
        SetState(AppListViewState::FULLSCREEN_SEARCH);
      break;
    case AppListViewState::CLOSED:
      NOTREACHED();
      break;
  }
}

void AppListView::UpdateYPositionAndOpacity(int y_position_in_screen,
                                            float background_opacity) {
  DCHECK(!is_side_shelf_);
  if (app_list_state_ == AppListViewState::CLOSED)
    return;

  if (fullscreen_widget_->GetLayer()->GetAnimator()->IsAnimatingProperty(
          ui::LayerAnimationElement::TRANSFORM)) {
    fullscreen_widget_->GetLayer()->GetAnimator()->StopAnimatingProperty(
        ui::LayerAnimationElement::TRANSFORM);
  }

  SetIsInDrag(true);
  background_opacity_ = background_opacity;
  gfx::Rect new_widget_bounds = fullscreen_widget_->GetWindowBoundsInScreen();
  app_list_y_position_in_screen_ = std::min(
      std::max(y_position_in_screen, GetDisplayNearestView().bounds().y()),
      GetDisplayNearestView().bounds().bottom() - kShelfSize);
  new_widget_bounds.set_y(app_list_y_position_in_screen_);
  gfx::NativeView native_view = fullscreen_widget_->GetNativeView();
  ::wm::ConvertRectFromScreen(native_view->parent(), &new_widget_bounds);
  native_view->SetBounds(new_widget_bounds);

  DraggingLayout();
}

PaginationModel* AppListView::GetAppsPaginationModel() {
  return GetRootAppsGridView()->pagination_model();
}

gfx::Rect AppListView::GetAppInfoDialogBounds() const {
  gfx::Rect app_info_bounds(GetDisplayNearestView().bounds());
  app_info_bounds.ClampToCenteredSize(
      gfx::Size(kAppInfoDialogWidth, kAppInfoDialogHeight));
  return app_info_bounds;
}

void AppListView::SetIsInDrag(bool is_in_drag) {
  if (app_list_state_ == AppListViewState::CLOSED)
    return;

  if (is_in_drag == is_in_drag_)
    return;

  is_in_drag_ = is_in_drag;
  GetAppsContainerView()->UpdateControlVisibility(app_list_state_, is_in_drag_);
}

int AppListView::GetScreenBottom() {
  return GetDisplayNearestView().bounds().bottom();
}

void AppListView::SetIsIgnoringScrollEvents(bool is_ignoring) {
  DCHECK_NE(is_ignoring_scroll_events_, is_ignoring);
  is_ignoring_scroll_events_ = is_ignoring;
}

void AppListView::DraggingLayout() {
  if (app_list_state_ == AppListViewState::CLOSED)
    return;

  float shield_opacity =
      is_background_blur_enabled_ ? kAppListOpacityWithBlur : kAppListOpacity;
  app_list_background_shield_->layer()->SetOpacity(
      is_in_drag_ ? background_opacity_ : shield_opacity);

  // Updates the opacity of the items in the app list.
  search_box_view_->UpdateOpacity();
  GetAppsContainerView()->UpdateOpacity();

  Layout();
}

void AppListView::RedirectKeyEventToSearchBox(ui::KeyEvent* event) {
  if (event->handled())
    return;

  views::Textfield* search_box = search_box_view_->search_box();
  const bool is_search_box_focused = search_box->HasFocus();
  const bool is_folder_header_view_focused = GetAppsContainerView()
                                                 ->app_list_folder_view()
                                                 ->folder_header_view()
                                                 ->HasTextFocus();
  if (is_search_box_focused || is_folder_header_view_focused) {
    // Do not redirect the key event to the |search_box_| when focus is on a
    // text field.
    return;
  }

  if (CanProcessLeftRightKeyTraversal(*event) ||
      CanProcessUpDownKeyTraversal(*event)) {
    // Do not redirect the arrow keys that are used to do focus traversal.
    return;
  }

  // Redirect key event to |search_box_|.
  search_box->OnKeyEvent(event);
  if (event->handled()) {
    // Set search box focused if the key event is consumed.
    search_box->RequestFocus();
    return;
  }
  if (event->type() == ui::ET_KEY_PRESSED) {
    // Insert it into search box if the key event is a character. Released
    // key should not be handled to prevent inserting duplicate character.
    search_box->InsertChar(*event);
  }
}

void AppListView::OnScreenKeyboardShown(bool shown) {
  if (onscreen_keyboard_shown_ == shown)
    return;

  onscreen_keyboard_shown_ = shown;
  if (!GetAppsContainerView()->IsInFolderView())
    return;
  // Update the folder's bounds to avoid being blocked by the on-screen
  // keyboard.
  GetAppsContainerView()->app_list_folder_view()->UpdatePreferredBounds();
  GetAppsContainerView()->Layout();
  GetAppsContainerView()->SchedulePaint();
}

void AppListView::OnDisplayMetricsChanged(const display::Display& display,
                                          uint32_t changed_metrics) {
  // Set the |fullscreen_widget_| size to fit the new display metrics.
  gfx::Size size = GetDisplayNearestView().size();
  fullscreen_widget_->SetSize(size);

  // Update the |fullscreen_widget_| bounds to accomodate the new work
  // area.
  SetState(app_list_state_);
}

float AppListView::GetAppListBackgroundOpacityDuringDragging() {
  float top_of_applist = fullscreen_widget_->GetWindowBoundsInScreen().y();
  float dragging_height =
      std::max((GetScreenBottom() - kShelfSize - top_of_applist), 0.f);
  float coefficient =
      std::min(dragging_height / (kNumOfShelfSize * kShelfSize), 1.0f);
  float shield_opacity =
      is_background_blur_enabled_ ? kAppListOpacityWithBlur : kAppListOpacity;
  // Assume shelf is opaque when start to drag down the launcher.
  const float shelf_opacity = 1.0f;
  return coefficient * shield_opacity + (1 - coefficient) * shelf_opacity;
}

void AppListView::GetWallpaperProminentColors(
    AppListViewDelegate::GetWallpaperProminentColorsCallback callback) {
  delegate_->GetWallpaperProminentColors(std::move(callback));
}

void AppListView::OnWallpaperColorsChanged() {
  SetBackgroundShieldColor();
}

void AppListView::SetBackgroundShieldColor() {
  // There is a chance when AppListView::OnWallpaperColorsChanged is called
  // from AppListViewDelegate, the |app_list_background_shield_| is not
  // initialized.
  if (!app_list_background_shield_)
    return;

  GetWallpaperProminentColors(base::BindOnce(
      [](base::WeakPtr<AppListView> self,
         const std::vector<SkColor>& prominent_colors) {
        self->app_list_background_shield_->layer()->SetColor(
            GetBackgroundShieldColor(prominent_colors));
      },
      weak_ptr_factory_.GetWeakPtr()));
}

void AppListView::RecordFolderMetrics() {
  int number_of_apps_in_folders = 0;
  int number_of_folders = 0;
  AppListItemList* item_list =
      app_list_main_view_->model()->top_level_item_list();
  for (size_t i = 0; i < item_list->item_count(); ++i) {
    AppListItem* item = item_list->item_at(i);
    if (item->GetItemType() != AppListFolderItem::kItemType)
      continue;
    ++number_of_folders;
    AppListFolderItem* folder = static_cast<AppListFolderItem*>(item);
    if (folder->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM)
      continue;  // Don't count items in OEM folders.
    number_of_apps_in_folders += folder->item_list()->item_count();
  }
  UMA_HISTOGRAM_COUNTS_100(kNumberOfFoldersHistogram, number_of_folders);
  UMA_HISTOGRAM_COUNTS_100(kNumberOfAppsInFoldersHistogram,
                           number_of_apps_in_folders);
}

}  // namespace app_list
