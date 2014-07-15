// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_view.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/win/windows_version.h"
#include "grit/ui_resources.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/app_list/views/app_list_background.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view_observer.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/speech_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/bubble/bubble_window_targeter.h"
#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif
#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif
#endif  // defined(USE_AURA)

namespace app_list {

namespace {

// The margin from the edge to the speech UI.
const int kSpeechUIMargin = 12;

// The vertical position for the appearing animation of the speech UI.
const float kSpeechUIAppearingPosition = 12;

// The distance between the arrow tip and edge of the anchor view.
const int kArrowOffset = 10;

// Determines whether the current environment supports shadows bubble borders.
bool SupportsShadow() {
#if defined(OS_WIN)
  // Shadows are not supported on Windows without Aero Glass.
  if (!ui::win::IsAeroGlassEnabled() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableDwmComposition)) {
    return false;
  }
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Shadows are not supported on (non-ChromeOS) Linux.
  return false;
#endif
  return true;
}

// The background for the App List overlay, which appears as a white rounded
// rectangle with the given radius and the same size as the target view.
class AppListOverlayBackground : public views::Background {
 public:
  AppListOverlayBackground(int corner_radius)
      : corner_radius_(corner_radius) {};
  virtual ~AppListOverlayBackground() {};

  // Overridden from views::Background:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SK_ColorWHITE);
    canvas->DrawRoundRect(view->GetContentsBounds(), corner_radius_, paint);
  }

 private:
  const int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(AppListOverlayBackground);
};

}  // namespace

// An animation observer to hide the view at the end of the animation.
class HideViewAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  HideViewAnimationObserver()
      : frame_(NULL),
        target_(NULL) {
  }

  virtual ~HideViewAnimationObserver() {
    if (target_)
      StopObservingImplicitAnimations();
  }

  void SetTarget(views::View* target) {
    if (!target_)
      StopObservingImplicitAnimations();
    target_ = target;
  }

  void set_frame(views::BubbleFrameView* frame) { frame_ = frame; }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    if (target_) {
      target_->SetVisible(false);
      target_ = NULL;

      // Should update the background by invoking SchedulePaint().
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
      app_list_main_view_(NULL),
      speech_view_(NULL),
      experimental_banner_view_(NULL),
      overlay_view_(NULL),
      animation_observer_(new HideViewAnimationObserver()) {
  CHECK(delegate);

  delegate_->AddObserver(this);
  delegate_->GetSpeechUI()->AddObserver(this);
}

AppListView::~AppListView() {
  delegate_->GetSpeechUI()->RemoveObserver(this);
  delegate_->RemoveObserver(this);
  animation_observer_.reset();
  // Remove child views first to ensure no remaining dependencies on delegate_.
  RemoveAllChildViews(true);
}

void AppListView::InitAsBubbleAttachedToAnchor(
    gfx::NativeView parent,
    int initial_apps_page,
    views::View* anchor,
    const gfx::Vector2d& anchor_offset,
    views::BubbleBorder::Arrow arrow,
    bool border_accepts_events) {
  SetAnchorView(anchor);
  InitAsBubbleInternal(
      parent, initial_apps_page, arrow, border_accepts_events, anchor_offset);
}

void AppListView::InitAsBubbleAtFixedLocation(
    gfx::NativeView parent,
    int initial_apps_page,
    const gfx::Point& anchor_point_in_screen,
    views::BubbleBorder::Arrow arrow,
    bool border_accepts_events) {
  SetAnchorView(NULL);
  SetAnchorRect(gfx::Rect(anchor_point_in_screen, gfx::Size()));
  InitAsBubbleInternal(
      parent, initial_apps_page, arrow, border_accepts_events, gfx::Vector2d());
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

void AppListView::Close() {
  app_list_main_view_->Close();
  delegate_->Dismiss();
}

void AppListView::UpdateBounds() {
  SizeToContents();
}

void AppListView::SetAppListOverlayVisible(bool visible) {
  DCHECK(overlay_view_);
  overlay_view_->SetVisible(visible);
}

bool AppListView::ShouldCenterWindow() const {
  return delegate_->ShouldCenterWindow();
}

gfx::Size AppListView::GetPreferredSize() const {
  return app_list_main_view_->GetPreferredSize();
}

void AppListView::Paint(gfx::Canvas* canvas, const views::CullSet& cull_set) {
  views::BubbleDelegateView::Paint(canvas, cull_set);
  if (!next_paint_callback_.is_null()) {
    next_paint_callback_.Run();
    next_paint_callback_.Reset();
  }
}

void AppListView::OnThemeChanged() {
#if defined(OS_WIN)
  GetWidget()->Close();
#endif
}

bool AppListView::ShouldHandleSystemCommands() const {
  return true;
}

void AppListView::Prerender() {
  app_list_main_view_->Prerender();
}

void AppListView::OnProfilesChanged() {
  app_list_main_view_->search_box_view()->InvalidateMenu();
}

void AppListView::SetProfileByPath(const base::FilePath& profile_path) {
  delegate_->SetProfileByPath(profile_path);
  app_list_main_view_->ModelChanged();
}

void AppListView::AddObserver(AppListViewObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListView::RemoveObserver(AppListViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

// static
void AppListView::SetNextPaintCallback(const base::Closure& callback) {
  next_paint_callback_ = callback;
}

#if defined(OS_WIN)
HWND AppListView::GetHWND() const {
  gfx::NativeWindow window =
      GetWidget()->GetTopLevelWidget()->GetNativeWindow();
  return window->GetHost()->GetAcceleratedWidget();
}
#endif

PaginationModel* AppListView::GetAppsPaginationModel() {
  return app_list_main_view_->contents_view()
      ->apps_container_view()
      ->apps_grid_view()
      ->pagination_model();
}

void AppListView::InitAsBubbleInternal(gfx::NativeView parent,
                                       int initial_apps_page,
                                       views::BubbleBorder::Arrow arrow,
                                       bool border_accepts_events,
                                       const gfx::Vector2d& anchor_offset) {
  app_list_main_view_ =
      new AppListMainView(delegate_.get(), initial_apps_page, parent);
  AddChildView(app_list_main_view_);
  app_list_main_view_->SetPaintToLayer(true);
  app_list_main_view_->SetFillsBoundsOpaquely(false);
  app_list_main_view_->layer()->SetMasksToBounds(true);

  // Speech recognition is available only when the start page exists.
  if (delegate_ && delegate_->IsSpeechRecognitionEnabled()) {
    speech_view_ = new SpeechView(delegate_.get());
    speech_view_->SetVisible(false);
    speech_view_->SetPaintToLayer(true);
    speech_view_->SetFillsBoundsOpaquely(false);
    speech_view_->layer()->SetOpacity(0.0f);
    AddChildView(speech_view_);
  }

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    // Draw a banner in the corner of the experimental app list.
    experimental_banner_view_ = new views::ImageView;
    const gfx::ImageSkia& experimental_icon =
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_APP_LIST_EXPERIMENTAL_ICON);
    experimental_banner_view_->SetImage(experimental_icon);
    experimental_banner_view_->SetPaintToLayer(true);
    experimental_banner_view_->SetFillsBoundsOpaquely(false);
    AddChildView(experimental_banner_view_);
  }

  OnProfilesChanged();
  set_color(kContentsBackgroundColor);
  set_margins(gfx::Insets());
  set_parent_window(parent);
  set_close_on_deactivate(false);
  set_close_on_esc(false);
  set_anchor_view_insets(gfx::Insets(kArrowOffset + anchor_offset.y(),
                                     kArrowOffset + anchor_offset.x(),
                                     kArrowOffset - anchor_offset.y(),
                                     kArrowOffset - anchor_offset.x()));
  set_border_accepts_events(border_accepts_events);
  set_shadow(SupportsShadow() ? views::BubbleBorder::BIG_SHADOW
                              : views::BubbleBorder::NO_SHADOW_OPAQUE_BORDER);
  views::BubbleDelegateView::CreateBubble(this);
  SetBubbleArrow(arrow);

#if defined(USE_AURA)
  aura::Window* window = GetWidget()->GetNativeWindow();
  window->layer()->SetMasksToBounds(true);
  GetBubbleFrameView()->set_background(new AppListBackground(
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius(),
      app_list_main_view_));
  set_background(NULL);
  window->SetEventTargeter(scoped_ptr<ui::EventTargeter>(
      new views::BubbleWindowTargeter(this)));
#else
  set_background(new AppListBackground(
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius(),
      app_list_main_view_));

  // On non-aura the bubble has two widgets, and it's possible for the border
  // to be shown independently in odd situations. Explicitly hide the bubble
  // widget to ensure that any WM_WINDOWPOSCHANGED messages triggered by the
  // window manager do not have the SWP_SHOWWINDOW flag set which would cause
  // the border to be shown. See http://crbug.com/231687 .
  GetWidget()->Hide();
#endif

  // To make the overlay view, construct a view with a white background, rather
  // than a white rectangle in it. This is because we need overlay_view_ to be
  // drawn to its own layer (so it appears correctly in the foreground).
  const float kOverlayOpacity = 0.75f;
  overlay_view_ = new views::View();
  overlay_view_->SetPaintToLayer(true);
  overlay_view_->layer()->SetOpacity(kOverlayOpacity);
  overlay_view_->SetBoundsRect(GetContentsBounds());
  overlay_view_->SetVisible(false);
  // On platforms that don't support a shadow, the rounded border of the app
  // list is constructed _inside_ the view, so a rectangular background goes
  // over the border in the rounded corners. To fix this, give the background a
  // corner radius 1px smaller than the outer border, so it just reaches but
  // doesn't cover it.
  const int kOverlayCornerRadius =
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius();
  overlay_view_->set_background(new AppListOverlayBackground(
      kOverlayCornerRadius - (SupportsShadow() ? 0 : 1)));
  AddChildView(overlay_view_);

  if (delegate_)
    delegate_->ViewInitialized();
}

void AppListView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  if (delegate_ && delegate_->ForceNativeDesktop())
    params->native_widget = new views::DesktopNativeWidgetAura(widget);
#endif
#if defined(OS_WIN)
  // Windows 7 and higher offer pinning to the taskbar, but we need presence
  // on the taskbar for the user to be able to pin us. So, show the window on
  // the taskbar for these versions of Windows.
  if (base::win::GetVersion() >= base::win::VERSION_WIN7)
    params->force_show_in_taskbar = true;
#elif defined(OS_LINUX)
  // Set up a custom WM_CLASS for the app launcher window. This allows task
  // switchers in X11 environments to distinguish it from main browser windows.
  params->wm_class_name = kAppListWMClass;
  // Show the window in the taskbar, even though it is a bubble, which would not
  // normally be shown.
  params->force_show_in_taskbar = true;
#endif
}

views::View* AppListView::GetInitiallyFocusedView() {
  return app_list::switches::IsExperimentalAppListEnabled()
             ? app_list_main_view_->contents_view()
                   ->start_page_view()
                   ->dummy_search_box_view()
                   ->search_box()
             : app_list_main_view_->search_box_view()->search_box();
}

gfx::ImageSkia AppListView::GetWindowIcon() {
  if (delegate_)
    return delegate_->GetWindowIcon();

  return gfx::ImageSkia();
}

bool AppListView::WidgetHasHitTestMask() const {
  return true;
}

void AppListView::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(
      GetBubbleFrameView()->GetContentsBounds()));
}

bool AppListView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // The accelerator is added by BubbleDelegateView.
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    if (app_list_main_view_->search_box_view()->HasSearch()) {
      app_list_main_view_->search_box_view()->ClearSearch();
    } else if (app_list_main_view_->contents_view()
                   ->apps_container_view()
                   ->IsInFolderView()) {
      app_list_main_view_->contents_view()
          ->apps_container_view()
          ->app_list_folder_view()
          ->CloseFolderPage();
      return true;
    } else {
      GetWidget()->Deactivate();
      Close();
    }
    return true;
  }

  return false;
}

void AppListView::Layout() {
  const gfx::Rect contents_bounds = GetContentsBounds();
  app_list_main_view_->SetBoundsRect(contents_bounds);

  if (speech_view_) {
    gfx::Rect speech_bounds = contents_bounds;
    int preferred_height = speech_view_->GetPreferredSize().height();
    speech_bounds.Inset(kSpeechUIMargin, kSpeechUIMargin);
    speech_bounds.set_height(std::min(speech_bounds.height(),
                                      preferred_height));
    speech_bounds.Inset(-speech_view_->GetInsets());
    speech_view_->SetBoundsRect(speech_bounds);
  }

  if (experimental_banner_view_) {
    // Position the experimental banner in the lower right corner.
    gfx::Rect image_bounds = experimental_banner_view_->GetImageBounds();
    image_bounds.set_origin(
        gfx::Point(contents_bounds.width() - image_bounds.width(),
                   contents_bounds.height() - image_bounds.height()));
    experimental_banner_view_->SetBoundsRect(image_bounds);
  }
}

void AppListView::SchedulePaintInRect(const gfx::Rect& rect) {
  BubbleDelegateView::SchedulePaintInRect(rect);
  if (GetBubbleFrameView())
    GetBubbleFrameView()->SchedulePaint();
}

void AppListView::OnWidgetDestroying(views::Widget* widget) {
  BubbleDelegateView::OnWidgetDestroying(widget);
  if (delegate_ && widget == GetWidget())
    delegate_->ViewClosing();
}

void AppListView::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
  // Do not called inherited function as the bubble delegate auto close
  // functionality is not used.
  if (widget == GetWidget())
    FOR_EACH_OBSERVER(AppListViewObserver, observers_,
                      OnActivationChanged(widget, active));
}

void AppListView::OnWidgetVisibilityChanged(views::Widget* widget,
                                            bool visible) {
  BubbleDelegateView::OnWidgetVisibilityChanged(widget, visible);

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

  if (will_appear)
    speech_view_->SetVisible(true);
  else
    app_list_main_view_->SetVisible(true);
}

}  // namespace app_list
