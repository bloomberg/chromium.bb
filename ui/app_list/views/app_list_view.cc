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
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/bubble_window_targeter.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/masked_window_targeter.h"
#include "ui/wm/core/shadow_types.h"

namespace app_list {

namespace {

// The margin from the edge to the speech UI.
constexpr int kSpeechUIMargin = 12;

// The height/width of the shelf from the bottom/side of the screen.
constexpr int kShelfSize = 48;

// The height of the peeking app list from the bottom of the screen.
constexpr int kPeekingAppListHeight = 320;

// The height of the half app list from the bottom of the screen.
constexpr int kHalfAppListHeight = 561;

// The fraction of app list height that the app list must be released at in
// order to transition to the next state.
constexpr int kAppListThresholdDenominator = 3;

// The velocity the app list must be dragged in order to transition to the next
// state, measured in DIPs/event.
constexpr int kAppListDragVelocityThreshold = 25;

// The DIP distance from the bezel that a drag event must end within to transfer
// the |app_list_state_|.
constexpr int kAppListBezelMargin = 50;

// The opacity of the app list background.
constexpr float kAppListOpacity = 0.8;

// The vertical position for the appearing animation of the speech UI.
constexpr float kSpeechUIAppearingPosition = 12;

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

// An event targeter for the search box widget which will ignore events that
// are on the search box's shadow.
class SearchBoxWindowTargeter : public wm::MaskedWindowTargeter {
 public:
  explicit SearchBoxWindowTargeter(views::View* search_box)
      : wm::MaskedWindowTargeter(search_box->GetWidget()->GetNativeWindow()),
        search_box_(search_box) {}
  ~SearchBoxWindowTargeter() override {}

 private:
  // wm::MaskedWindowTargeter:
  bool GetHitTestMask(aura::Window* window, gfx::Path* mask) const override {
    mask->addRect(gfx::RectToSkRect(search_box_->GetContentsBounds()));
    return true;
  }

  views::View* search_box_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxWindowTargeter);
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
      app_list_main_view_(nullptr),
      speech_view_(nullptr),
      search_box_focus_host_(nullptr),
      search_box_widget_(nullptr),
      search_box_view_(nullptr),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()),
      app_list_state_(PEEKING),
      display_observer_(this),
      overlay_view_(nullptr),
      animation_observer_(new HideViewAnimationObserver()) {
  CHECK(delegate);
  delegate_->GetSpeechUI()->AddObserver(this);

  if (is_fullscreen_app_list_enabled_)
    display_observer_.Add(display::Screen::GetScreen());
}

AppListView::~AppListView() {
  delegate_->GetSpeechUI()->RemoveObserver(this);
  animation_observer_.reset();
  // Remove child views first to ensure no remaining dependencies on delegate_.
  RemoveAllChildViews(true);
}

void AppListView::Initialize(gfx::NativeView parent,
                             int initial_apps_page,
                             bool is_maximize_mode,
                             bool is_side_shelf) {
  base::Time start_time = base::Time::Now();
  is_maximize_mode_ = is_maximize_mode;
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

  if (delegate_)
    delegate_->ViewInitialized();

  UMA_HISTOGRAM_TIMES("Apps.AppListCreationTime",
                      base::Time::Now() - start_time);
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

PaginationModel* AppListView::GetAppsPaginationModel() {
  return app_list_main_view_->contents_view()
      ->apps_container_view()
      ->apps_grid_view()
      ->pagination_model();
}

void AppListView::InitContents(gfx::NativeView parent, int initial_apps_page) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/440224 and
  // crbug.com/441028 are fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "440224, 441028 AppListView::InitContents"));

  if (is_fullscreen_app_list_enabled_) {
    // The shield view that colors the background of the app list and makes it
    // transparent.
    app_list_background_shield_ = new views::View;
    app_list_background_shield_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    app_list_background_shield_->layer()->SetColor(SK_ColorBLACK);
    app_list_background_shield_->layer()->SetOpacity(kAppListOpacity);
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

  app_list_main_view_->Init(parent, initial_apps_page, search_box_view_);

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

  // Mouse events on the search box shadow should not be captured.
  aura::Window* window = search_box_widget_->GetNativeWindow();
  window->SetEventTargeter(
      base::MakeUnique<SearchBoxWindowTargeter>(search_box_view_));

  app_list_main_view_->contents_view()->Layout();
}

void AppListView::InitializeFullscreen(gfx::NativeView parent,
                                       int initial_apps_page) {
  gfx::Rect display_work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(parent_window())
          .work_area();

  gfx::Rect app_list_overlay_view_bounds(
      display_work_area_bounds.x(),
      display_work_area_bounds.height() + kShelfSize - kPeekingAppListHeight,
      display_work_area_bounds.width(),
      display_work_area_bounds.height() + kShelfSize);

  fullscreen_widget_ = new views::Widget;
  views::Widget::InitParams app_list_overlay_view_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);

  app_list_overlay_view_params.name = "AppList";
  app_list_overlay_view_params.parent = parent;
  app_list_overlay_view_params.delegate = this;
  app_list_overlay_view_params.opacity =
      views::Widget::InitParams::TRANSLUCENT_WINDOW;
  app_list_overlay_view_params.bounds = app_list_overlay_view_bounds;
  app_list_overlay_view_params.layer_type = ui::LAYER_SOLID_COLOR;
  fullscreen_widget_->Init(app_list_overlay_view_params);

  overlay_view_ = new AppListOverlayView(0 /* no corners */);
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

  aura::Window* window = GetWidget()->GetNativeWindow();
  window->SetEventTargeter(base::MakeUnique<views::BubbleWindowTargeter>(this));

  const int kOverlayCornerRadius =
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius();
  overlay_view_ = new AppListOverlayView(kOverlayCornerRadius);
  overlay_view_->SetBoundsRect(GetContentsBounds());
}

void AppListView::StartDrag(const gfx::Point& location) {
  initial_drag_point_ = location;
}

void AppListView::UpdateDrag(const gfx::Point& location) {
  // Update the bounds of the widget while maintaining the
  // relative position of the top of the widget and the mouse/gesture.
  // Block drags north of 0 and recalculate the initial_drag_point_.
  int const new_y_position = location.y() - initial_drag_point_.y() +
                             fullscreen_widget_->GetWindowBoundsInScreen().y();
  gfx::Rect new_widget_bounds = fullscreen_widget_->GetWindowBoundsInScreen();
  if (new_y_position < 0) {
    new_widget_bounds.set_y(0);
    initial_drag_point_ = location;
  } else {
    new_widget_bounds.set_y(new_y_position);
  }
  fullscreen_widget_->SetBounds(new_widget_bounds);
}

void AppListView::EndDrag(const gfx::Point& location) {
  // Change the app list state based on where the drag ended. If fling velocity
  // was over the threshold, snap to the next state in the direction of the
  // fling.
  int const new_y_position = location.y() - initial_drag_point_.y() +
                             fullscreen_widget_->GetWindowBoundsInScreen().y();
  if (std::abs(last_fling_velocity_) > kAppListDragVelocityThreshold) {
    // If the user releases drag with velocity over the threshold, snap to
    // the next state, ignoring the drag release position.

    if (last_fling_velocity_ > 0) {
      switch (app_list_state_) {
        case PEEKING:
        case HALF:
        case FULLSCREEN_SEARCH:
          SetState(CLOSED);
          break;
        case FULLSCREEN_ALL_APPS:
          SetState(is_maximize_mode_ || is_side_shelf_ ? CLOSED : PEEKING);
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
          SetState(FULLSCREEN_ALL_APPS);
          break;
        case CLOSED:
          NOTREACHED();
          break;
      }
    }
  } else {
    int display_height = display::Screen::GetScreen()
                             ->GetDisplayNearestView(parent_window())
                             .size().height();
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

    int app_list_threshold = app_list_height / kAppListThresholdDenominator;
    int drag_delta = app_list_y_for_state - new_y_position;
    gfx::Point location_in_screen_coordinates = location;
    ConvertPointToScreen(this, &location_in_screen_coordinates);
    switch (app_list_state_) {
      case FULLSCREEN_ALL_APPS:
        if (std::abs(drag_delta) > app_list_threshold)
          SetState(is_maximize_mode_ || is_side_shelf_ ? CLOSED : PEEKING);
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
        } else if (location_in_screen_coordinates.y() >=
                   display_height - kAppListBezelMargin) {
          // If the user drags to the bezel, close the app list.
          SetState(CLOSED);
        } else {
          SetState(app_list_state_);
        }
        break;
      case PEEKING:
        if (std::abs(drag_delta) > app_list_threshold) {
          SetState(drag_delta > 0 ? FULLSCREEN_ALL_APPS : CLOSED);
        } else if (location_in_screen_coordinates.y() >=
                   display_height - kAppListBezelMargin) {
          // If the user drags to the bezel, close the app list.
          SetState(CLOSED);
        } else {
          SetState(app_list_state_);
        }
        break;
      case CLOSED:
        NOTREACHED();
        break;
    }
  }
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

void AppListView::OnMaximizeModeChanged(bool started) {
  is_maximize_mode_ = started;
  if (is_maximize_mode_ && !is_fullscreen()) {
    // Set |app_list_state_| to a maximize mode friendly state.
    SetState(app_list_state_ == PEEKING ? FULLSCREEN_ALL_APPS
                                        : FULLSCREEN_SEARCH);
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

void AppListView::OnMouseEvent(ui::MouseEvent* event) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      StartDrag(event->location());
      event->SetHandled();
      break;
    case ui::ET_MOUSE_DRAGGED:
      UpdateDrag(event->location());
      event->SetHandled();
      break;
    case ui::ET_MOUSE_RELEASED:
      EndDrag(event->location());
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
    case ui::ET_GESTURE_SCROLL_BEGIN:
      StartDrag(event->location());
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      last_fling_velocity_ = event->details().velocity_y();
      UpdateDrag(event->location());
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      EndDrag(event->location());
      event->SetHandled();
      break;
    default:
      break;
  }
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

  // Make sure to layout |app_list_main_view_| and |speech_view_| at the center
  // of the widget.
  gfx::Rect centered_bounds = contents_bounds;
  centered_bounds.ClampToCenteredSize(gfx::Size(
      app_list_main_view_->contents_view()->GetDefaultContentsBounds().width(),
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

  if (is_fullscreen_app_list_enabled_) {
    app_list_main_view_->contents_view()->Layout();
    app_list_background_shield_->SetBoundsRect(contents_bounds);
  }
}

void AppListView::SchedulePaintInRect(const gfx::Rect& rect) {
  BubbleDialogDelegateView::SchedulePaintInRect(rect);
  if (GetBubbleFrameView())
    GetBubbleFrameView()->SchedulePaint();
}

void AppListView::SetState(AppListState new_state) {
  AppListState new_state_override = new_state;
  if (is_side_shelf_ || is_maximize_mode_) {
    // If side shelf or maximize mode are active, all transitions should be
    // made to the maximize mode/side shelf friendly versions.
    if (new_state == PEEKING)
      new_state_override = FULLSCREEN_ALL_APPS;
    else if (new_state == HALF)
      new_state_override = FULLSCREEN_SEARCH;
  }

  gfx::Rect new_widget_bounds = fullscreen_widget_->GetWindowBoundsInScreen();
  int display_height = display::Screen::GetScreen()
                           ->GetDisplayNearestView(parent_window())
                           .work_area()
                           .bottom();

  switch (new_state_override) {
    case PEEKING: {
      switch (app_list_state_) {
        case HALF:
        case FULLSCREEN_ALL_APPS:
        case PEEKING: {
          int peeking_app_list_y = display_height - kPeekingAppListHeight;
          new_widget_bounds.set_y(peeking_app_list_y);
          app_list_main_view_->contents_view()->SetActiveState(
              AppListModel::STATE_START);
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
      switch (app_list_state_) {
        case PEEKING:
        case HALF: {
          int half_app_list_y = display_height - kHalfAppListHeight;
          new_widget_bounds.set_y(half_app_list_y);
          break;
        }
        case FULLSCREEN_SEARCH:
        case FULLSCREEN_ALL_APPS:
        case CLOSED:
          NOTREACHED();
          break;
      }
      break;
    case FULLSCREEN_ALL_APPS:
      new_widget_bounds.set_y(0);
      app_list_main_view_->contents_view()->SetActiveState(
          AppListModel::STATE_APPS);
      break;
    case FULLSCREEN_SEARCH:
      new_widget_bounds.set_y(0);
      break;
    case CLOSED:
      app_list_main_view_->Close();
      delegate_->Dismiss();
      break;
  }
  fullscreen_widget_->SetBounds(new_widget_bounds);
  app_list_state_ = new_state_override;
}

void AppListView::OnWidgetDestroying(views::Widget* widget) {
  BubbleDialogDelegateView::OnWidgetDestroying(widget);
  if (delegate_ && widget == GetWidget())
    delegate_->ViewClosing();
}

void AppListView::OnWidgetVisibilityChanged(views::Widget* widget,
                                            bool visible) {
  BubbleDialogDelegateView::OnWidgetVisibilityChanged(widget, visible);

  if (widget != GetWidget())
    return;

  if (!visible)
    app_list_main_view_->ResetForShow();
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
    // Refocus the search box. However, if the app list widget does not have
    // focus, it means another window has already taken focus, and we *must not*
    // focus the search box (or we would steal focus back into the app list).
    if (GetWidget()->IsActive())
      search_box_view_->search_box()->RequestFocus();
  }
}

void AppListView::OnDisplayMetricsChanged(const display::Display& display,
                                          uint32_t changed_metrics) {
  if (!is_fullscreen_app_list_enabled_)
    return;

  // Set the |fullscreen_widget_| size to fit the new display metrics.
  gfx::Size size = display::Screen::GetScreen()
                       ->GetDisplayNearestView(parent_window())
                       .work_area()
                       .size();
  size.Enlarge(0, kShelfSize);
  fullscreen_widget_->SetSize(size);

  // Update the |fullscreen_widget_| bounds to accomodate the new work area.
  SetState(app_list_state_);
}

}  // namespace app_list
