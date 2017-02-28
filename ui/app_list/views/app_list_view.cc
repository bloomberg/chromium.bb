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
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
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
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/bubble_window_targeter.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/masked_window_targeter.h"
#include "ui/wm/core/shadow_types.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace app_list {

namespace {

// The margin from the edge to the speech UI.
const int kSpeechUIMargin = 12;

// The vertical position for the appearing animation of the speech UI.
const float kSpeechUIAppearingPosition = 12;

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
  HideViewAnimationObserver()
      : frame_(NULL),
        target_(NULL) {
  }

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
      overlay_view_(nullptr),
      animation_observer_(new HideViewAnimationObserver()) {
  CHECK(delegate);

  delegate_->GetSpeechUI()->AddObserver(this);
}

AppListView::~AppListView() {
  delegate_->GetSpeechUI()->RemoveObserver(this);
  animation_observer_.reset();
  // Remove child views first to ensure no remaining dependencies on delegate_.
  RemoveAllChildViews(true);
}

void AppListView::InitAsBubble(gfx::NativeView parent, int initial_apps_page) {
  base::Time start_time = base::Time::Now();

  InitContents(parent, initial_apps_page);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  set_margins(gfx::Insets());
  set_parent_window(parent);
  set_close_on_deactivate(false);
  set_shadow(views::BubbleBorder::NO_ASSETS);
  set_color(kContentsBackgroundColor);
  // This creates the app list widget. (Before this, child widgets cannot be
  // created.)
  views::BubbleDialogDelegateView::CreateBubble(this);

  SetBubbleArrow(views::BubbleBorder::FLOAT);
  // We can now create the internal widgets.
  InitChildWidgets();

  aura::Window* window = GetWidget()->GetNativeWindow();
  window->SetEventTargeter(base::MakeUnique<views::BubbleWindowTargeter>(this));

  const int kOverlayCornerRadius =
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius();
  overlay_view_ = new AppListOverlayView(kOverlayCornerRadius);
  overlay_view_->SetBoundsRect(GetContentsBounds());
  AddChildView(overlay_view_);

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

void AppListView::SetAnchorPoint(const gfx::Point& anchor_point) {
  SetAnchorRect(gfx::Rect(anchor_point, gfx::Size()));
}

void AppListView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  app_list_main_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppListView::ShowWhenReady() {
  app_list_main_view_->ShowAppListWhenReady();
}

void AppListView::CloseAppList() {
  app_list_main_view_->Close();
  delegate_->Dismiss();
}

void AppListView::UpdateBounds() {
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

gfx::Size AppListView::GetPreferredSize() const {
  return app_list_main_view_->GetPreferredSize();
}

void AppListView::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
  if (!next_paint_callback_.is_null()) {
    next_paint_callback_.Run();
    next_paint_callback_.Reset();
  }
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

void AppListView::SetProfileByPath(const base::FilePath& profile_path) {
  delegate_->SetProfileByPath(profile_path);
  app_list_main_view_->ModelChanged();
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

  app_list_main_view_ = new AppListMainView(delegate_);
  AddChildView(app_list_main_view_);
  app_list_main_view_->SetPaintToLayer();
  app_list_main_view_->layer()->SetFillsBoundsOpaquely(false);
  app_list_main_view_->layer()->SetMasksToBounds(true);

  // This will be added to the |search_box_widget_| after the app list widget is
  // initialized.
  search_box_view_ = new SearchBoxView(app_list_main_view_, delegate_);
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

void AppListView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
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

  mask->addRect(gfx::RectToSkRect(
      GetBubbleFrameView()->GetContentsBounds()));
}

bool AppListView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());

  // If the ContentsView does not handle the back action, then this is the
  // top level, so we close the app list.
  if (!app_list_main_view_->contents_view()->Back()) {
    GetWidget()->Deactivate();
    CloseAppList();
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
    speech_bounds.set_height(std::min(speech_bounds.height(),
                                      preferred_height));
    speech_bounds.Inset(-speech_view_->GetInsets());
    speech_view_->SetBoundsRect(speech_bounds);
  }
}

void AppListView::SchedulePaintInRect(const gfx::Rect& rect) {
  BubbleDialogDelegateView::SchedulePaintInRect(rect);
  if (GetBubbleFrameView())
    GetBubbleFrameView()->SchedulePaint();
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
  speech_transform.Translate(
      0, SkFloatToMScalar(kSpeechUIAppearingPosition));
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

}  // namespace app_list
