// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_view.h"

#include <algorithm>
#include <vector>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "components/wallpaper/wallpaper_color_profile.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/speech_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
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
#include "ui/strings/grit/ui_strings.h"
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

// The vertical position for the appearing animation of the speech UI.
constexpr float kSpeechUIAppearingPosition = 12;

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
      short_animations_for_testing_(false),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()),
      is_background_blur_enabled_(features::IsBackgroundBlurEnabled()),
      is_app_list_focus_enabled_(features::IsAppListFocusEnabled()),
      display_observer_(this),
      animation_observer_(new HideViewAnimationObserver()),
      previous_arrow_key_traversal_enabled_(
          views::FocusManager::arrow_key_traversal_enabled()) {
  CHECK(delegate);
  delegate_->GetSpeechUI()->AddObserver(this);

  if (is_fullscreen_app_list_enabled_) {
    display_observer_.Add(display::Screen::GetScreen());
    delegate_->AddObserver(this);
  }
  if (is_app_list_focus_enabled_) {
    // Enable arrow key in FocusManager. Arrow left/right and up/down triggers
    // the same focus movement as tab/shift+tab.
    views::FocusManager::set_arrow_key_traversal_enabled(true);
  }
}

AppListView::~AppListView() {
  delegate_->GetSpeechUI()->RemoveObserver(this);
  if (is_fullscreen_app_list_enabled_)
    delegate_->RemoveObserver(this);

  animation_observer_.reset();
  // Remove child views first to ensure no remaining dependencies on delegate_.
  RemoveAllChildViews(true);
  if (is_app_list_focus_enabled_) {
    views::FocusManager::set_arrow_key_traversal_enabled(
        previous_arrow_key_traversal_enabled_);
  }
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
  InitContents(params.initial_apps_page);
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  set_color(kContentsBackgroundColor);
  set_parent_window(params.parent);

  if (is_fullscreen_app_list_enabled_)
    InitializeFullscreen(params.parent, params.parent_container_id);
  else
    InitializeBubble();

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

void AppListView::Dismiss() {
  app_list_main_view_->Close();
  delegate_->Dismiss();
  SetState(CLOSED);
  GetWidget()->Deactivate();
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

void AppListView::InitContents(int initial_apps_page) {
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

  app_list_main_view_->Init(
      is_fullscreen_app_list_enabled_ ? 0 : initial_apps_page,
      search_box_view_);

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
      base::MakeUnique<AppListEventTargeter>());

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

void AppListView::InitializeBubble() {
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
          Dismiss();
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
      Dismiss();
      return;
    }
    switch (app_list_state_) {
      case FULLSCREEN_ALL_APPS:
        if (drag_delta < -app_list_threshold) {
          if (is_tablet_mode_ || is_side_shelf_)
            Dismiss();
          else
            SetState(PEEKING);
        } else {
          SetState(app_list_state_);
        }
        break;
      case FULLSCREEN_SEARCH:
        if (drag_delta < -app_list_threshold)
          Dismiss();
        else
          SetState(app_list_state_);
        break;
      case HALF:
        if (drag_delta > app_list_threshold)
          SetState(FULLSCREEN_SEARCH);
        else if (drag_delta < -app_list_threshold)
          Dismiss();
        else
          SetState(app_list_state_);
        break;
      case PEEKING:
        if (drag_delta > app_list_threshold) {
          SetState(FULLSCREEN_ALL_APPS);
          UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                    kSwipe, kMaxPeekingToFullscreen);
        } else if (drag_delta < -app_list_threshold) {
          Dismiss();
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

void AppListView::SetChildViewsForStateTransition(AppListState target_state) {
  if (target_state != PEEKING && target_state != FULLSCREEN_ALL_APPS)
    return;

  AppsContainerView* apps_container_view =
      app_list_main_view_->contents_view()->apps_container_view();

  if (apps_container_view->IsInFolderView())
    apps_container_view->ResetForShowApps();

  if (target_state == PEEKING) {
    app_list_main_view_->contents_view()->SetActiveState(
        AppListModel::STATE_START);
    // Set the apps to first page at STATE_START state.
    PaginationModel* pagination_model = GetAppsPaginationModel();
    if (pagination_model->total_pages() > 0 &&
        pagination_model->selected_page() != 0) {
      pagination_model->SelectPage(0, false /* animate */);
    }
  } else {
    // Set timer to ignore further scroll events for this transition.
    GetAppsGridView()->StartTimerToIgnoreScrollEvents();

    app_list_main_view_->contents_view()->SetActiveState(
        AppListModel::STATE_APPS, !is_side_shelf_);
  }
}

void AppListView::ConvertAppListStateToFullscreenEquivalent(
    AppListState* target_state) {
  if (!(is_side_shelf_ || is_tablet_mode_))
    return;

  // If side shelf or tablet mode are active, all transitions should be
  // made to the tablet mode/side shelf friendly versions.
  if (*target_state == HALF) {
    *target_state = FULLSCREEN_SEARCH;
  } else if (*target_state == PEEKING) {
    // FULLSCREEN_ALL_APPS->PEEKING in tablet/side shelf mode should close
    // instead of going to PEEKING.
    *target_state =
        app_list_state_ == FULLSCREEN_ALL_APPS ? CLOSED : FULLSCREEN_ALL_APPS;
  }
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

void AppListView::MaybeCreateAccessibilityEvent(AppListState new_state) {
  if (new_state != PEEKING && new_state != FULLSCREEN_ALL_APPS)
    return;

  DCHECK(state_announcement_ == base::string16());

  if (new_state == PEEKING) {
    state_announcement_ = l10n_util::GetStringUTF16(
        IDS_APP_LIST_SUGGESTED_APPS_ACCESSIBILITY_ANNOUNCEMENT);
  } else {
    state_announcement_ = l10n_util::GetStringUTF16(
        IDS_APP_LIST_ALL_APPS_ACCESSIBILITY_ANNOUNCEMENT);
  }
  NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
  state_announcement_ = base::string16();
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

  if (!HandleScroll(event->y_offset(), event->type()))
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
      if (HandleScroll(event->AsMouseWheelEvent()->offset().y(),
                       ui::ET_MOUSEWHEEL))
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
      if (HandleScroll(event->AsMouseWheelEvent()->offset().y(),
                       ui::ET_MOUSEWHEEL))
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
    Dismiss();
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

void AppListView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(state_announcement_);
  node_data->role = ui::AX_ROLE_DESKTOP;
}

void AppListView::OnKeyEvent(ui::KeyEvent* event) {
  views::Textfield* search_box = search_box_view_->search_box();
  bool is_search_box_focused = search_box->HasFocus();
  bool is_folder_header_view_focused = app_list_main_view_->contents_view()
                                           ->apps_container_view()
                                           ->app_list_folder_view()
                                           ->folder_header_view()
                                           ->HasTextFocus();
  if (is_app_list_focus_enabled_ && !is_search_box_focused &&
      !is_folder_header_view_focused && !SearchBoxView::IsArrowKey(*event)) {
    views::Textfield* search_box = search_box_view_->search_box();
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

bool AppListView::HandleScroll(int offset, ui::EventType type) {
  if (app_list_state_ != PEEKING && app_list_state_ != FULLSCREEN_ALL_APPS)
    return false;

  // Let the Apps grid view handle the event first in FULLSCREEN_ALL_APPS.
  if (app_list_state_ == FULLSCREEN_ALL_APPS &&
      GetAppsGridView()->HandleScrollFromAppListView(offset, type)) {
    // Set the scroll ignore timer to avoid processing the tail end of the
    // stream of scroll events, which would close the view.
    SetOrRestartScrollIgnoreTimer();
    return true;
  }

  if (ShouldIgnoreScrollEvents())
    return true;

  // If the event is a mousewheel event, the offset is always large enough,
  // otherwise the offset must be larger than the scroll threshold.
  if (type == ui::ET_MOUSEWHEEL ||
      abs(offset) > kAppListMinScrollToSwitchStates) {
    if (offset >= 0) {
      Dismiss();
    } else {
      if (app_list_state_ == FULLSCREEN_ALL_APPS)
        return true;
      SetState(FULLSCREEN_ALL_APPS);
      const AppListPeekingToFullscreenSource source =
          type == ui::ET_MOUSEWHEEL ? kMousewheelScroll : kMousepadScroll;
      UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram, source,
                                kMaxPeekingToFullscreen);
    }
  }
  return true;
}

void AppListView::SetOrRestartScrollIgnoreTimer() {
  scroll_ignore_timer_.reset(new base::ElapsedTimer());
}

bool AppListView::ShouldIgnoreScrollEvents() {
  return scroll_ignore_timer_ &&
         scroll_ignore_timer_->Elapsed() <=
             base::TimeDelta::FromMilliseconds(kScrollIgnoreTimeMs);
}

void AppListView::SetState(AppListState new_state) {
  // Do not allow the state to be changed once it has been set to CLOSED.
  if (app_list_state_ == CLOSED)
    return;

  AppListState new_state_override = new_state;
  ConvertAppListStateToFullscreenEquivalent(&new_state_override);
  MaybeCreateAccessibilityEvent(new_state_override);
  SetChildViewsForStateTransition(new_state_override);
  StartAnimationForState(new_state_override);
  RecordStateTransitionForUma(new_state_override);
  model_->SetStateFullscreen(new_state_override);
  app_list_state_ = new_state_override;
  if (new_state_override == CLOSED) {
    return;
  }

  // Reset the focus to initially focused view. This should be done before
  // updating visibility of views, because setting focused view invisible
  // automatically moves focus to next focusable view, which potentially
  // causes bugs.
  GetInitiallyFocusedView()->RequestFocus();

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

  int animation_duration;
  // If animating to or from a fullscreen state, animate over 250ms, else
  // animate over 200 ms.
  if (short_animations_for_testing_) {
    animation_duration = kAppListAnimationDurationTestMs;
  } else if (is_fullscreen() || target_state == FULLSCREEN_ALL_APPS ||
             target_state == FULLSCREEN_SEARCH) {
    animation_duration = kAppListAnimationDurationFromFullscreenMs;
  } else {
    animation_duration = kAppListAnimationDurationMs;
  }

  std::unique_ptr<ui::LayerAnimationElement> bounds_animation_element =
      ui::LayerAnimationElement::CreateBoundsElement(
          target_bounds, base::TimeDelta::FromMilliseconds(animation_duration));

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

  if (app_list_state_ != CLOSED)
    SetState(CLOSED);

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
  DCHECK(!is_side_shelf_);
  if (app_list_state_ == CLOSED)
    return;

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
  if (app_list_state_ == CLOSED)
    return;

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
  if (app_list_state_ == CLOSED)
    return;

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
  // Assume shelf is opaque when start to drag down the launcher.
  const float shelf_opacity = 1.0f;
  return coefficient * shield_opacity + (1 - coefficient) * shelf_opacity;
}

void AppListView::GetWallpaperProminentColors(std::vector<SkColor>* colors) {
  delegate_->GetWallpaperProminentColors(colors);
}

void AppListView::OnWallpaperColorsChanged() {
  SetBackgroundShieldColor();
}

void AppListView::SetBackgroundShieldColor() {
  // There is a chance when AppListView::OnWallpaperColorsChanged is called
  // from AppListViewDelegate, the |app_list_background_shield_| is not
  // initialized.
  if (!is_fullscreen_app_list_enabled_ || !app_list_background_shield_)
    return;

  std::vector<SkColor> prominent_colors;
  GetWallpaperProminentColors(&prominent_colors);
  app_list_background_shield_->layer()->SetColor(
      GetBackgroundShieldColor(prominent_colors));
}

}  // namespace app_list
