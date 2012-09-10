// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_delegate.h"

#include "ui/base/animation/slide_animation.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

// The duration of the fade animation in milliseconds.
static const int kHideFadeDurationMS = 200;

// The defaut margin between the content and the inside border, in pixels.
static const int kDefaultMargin = 6;

namespace views {

namespace {

// Create a widget to host the bubble.
Widget* CreateBubbleWidget(BubbleDelegateView* bubble) {
  Widget* bubble_widget = new Widget();
  Widget::InitParams bubble_params(Widget::InitParams::TYPE_BUBBLE);
  bubble_params.delegate = bubble;
  bubble_params.transparent = true;
  bubble_params.accept_events = bubble->accept_events();
  if (bubble->parent_window())
    bubble_params.parent = bubble->parent_window();
  else
    bubble_params.parent_widget = bubble->anchor_widget();
  bubble_params.can_activate = bubble->CanActivate();
#if defined(OS_WIN) && !defined(USE_AURA)
  bubble_params.type = Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  bubble_params.transparent = false;
#endif
  bubble_widget->Init(bubble_params);
  return bubble_widget;
}

#if defined(OS_WIN) && !defined(USE_AURA)
// Windows uses two widgets and some extra complexity to host partially
// transparent native controls and use per-pixel HWND alpha on the border.
// TODO(msw): Clean these up when Windows native controls are no longer needed.
class BubbleBorderDelegate : public WidgetDelegate,
                             public WidgetObserver {
 public:
  BubbleBorderDelegate(BubbleDelegateView* bubble, Widget* widget)
      : bubble_(bubble),
        widget_(widget) {
    bubble_->GetWidget()->AddObserver(this);
  }

  virtual ~BubbleBorderDelegate() {
    if (bubble_ && bubble_->GetWidget())
      bubble_->GetWidget()->RemoveObserver(this);
  }

  // WidgetDelegate overrides:
  virtual bool CanActivate() const OVERRIDE { return false; }
  virtual void DeleteDelegate() OVERRIDE { delete this; }
  virtual Widget* GetWidget() OVERRIDE { return widget_; }
  virtual const Widget* GetWidget() const OVERRIDE { return widget_; }
  virtual NonClientFrameView* CreateNonClientFrameView(
      Widget* widget) OVERRIDE {
    return bubble_->CreateNonClientFrameView(widget);
  }

  // WidgetObserver overrides:
  virtual void OnWidgetClosing(Widget* widget) OVERRIDE {
    bubble_ = NULL;
    widget_->Close();
  }

 private:
  BubbleDelegateView* bubble_;
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBorderDelegate);
};

// Create a widget to host the bubble's border.
Widget* CreateBorderWidget(BubbleDelegateView* bubble) {
  Widget* border_widget = new Widget();
  Widget::InitParams border_params(Widget::InitParams::TYPE_BUBBLE);
  border_params.delegate = new BubbleBorderDelegate(bubble, border_widget);
  border_params.transparent = true;
  border_params.parent_widget = bubble->anchor_widget();
  border_params.can_activate = bubble->CanActivate();
  border_widget->Init(border_params);
  return border_widget;
}
#endif

}  // namespace

#if defined(OS_WIN) && !defined(USE_AURA)
const SkColor BubbleDelegateView::kBackgroundColor =
    color_utils::GetSysSkColor(COLOR_WINDOW);
#else
// TODO(beng): source from theme provider.
const SkColor BubbleDelegateView::kBackgroundColor = SK_ColorWHITE;
#endif

BubbleDelegateView::BubbleDelegateView()
    : close_on_esc_(true),
      close_on_deactivate_(true),
      anchor_view_(NULL),
      anchor_widget_(NULL),
      move_with_anchor_(false),
      arrow_location_(BubbleBorder::TOP_LEFT),
      color_(kBackgroundColor),
      margins_(kDefaultMargin, kDefaultMargin, kDefaultMargin, kDefaultMargin),
      original_opacity_(255),
      border_widget_(NULL),
      use_focusless_(false),
      accept_events_(true),
      try_mirroring_arrow_(true),
      parent_window_(NULL) {
  set_background(Background::CreateSolidBackground(color_));
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

BubbleDelegateView::BubbleDelegateView(
    View* anchor_view,
    BubbleBorder::ArrowLocation arrow_location)
    : close_on_esc_(true),
      close_on_deactivate_(true),
      anchor_view_(anchor_view),
      anchor_widget_(NULL),
      move_with_anchor_(false),
      arrow_location_(arrow_location),
      color_(kBackgroundColor),
      margins_(kDefaultMargin, kDefaultMargin, kDefaultMargin, kDefaultMargin),
      original_opacity_(255),
      border_widget_(NULL),
      use_focusless_(false),
      accept_events_(true),
      try_mirroring_arrow_(true),
      parent_window_(NULL) {
  set_background(Background::CreateSolidBackground(color_));
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

BubbleDelegateView::~BubbleDelegateView() {
  if (anchor_widget() != NULL)
    anchor_widget()->RemoveObserver(this);
  anchor_widget_ = NULL;
  anchor_view_ = NULL;
}

// static
Widget* BubbleDelegateView::CreateBubble(BubbleDelegateView* bubble_delegate) {
  bubble_delegate->Init();
  // Determine the anchor widget from the anchor view at bubble creation time.
  bubble_delegate->anchor_widget_ = bubble_delegate->anchor_view() ?
      bubble_delegate->anchor_view()->GetWidget() : NULL;
  if (bubble_delegate->anchor_widget())
    bubble_delegate->anchor_widget()->AddObserver(bubble_delegate);

  Widget* bubble_widget = CreateBubbleWidget(bubble_delegate);

#if defined(OS_WIN) && !defined(USE_AURA)
  // First set the contents view to initialize view bounds for widget sizing.
  bubble_widget->SetContentsView(bubble_delegate->GetContentsView());
  bubble_delegate->border_widget_ = CreateBorderWidget(bubble_delegate);
#endif

  bubble_delegate->SizeToContents();
  bubble_widget->AddObserver(bubble_delegate);
  return bubble_widget;
}

View* BubbleDelegateView::GetInitiallyFocusedView() {
  return this;
}

BubbleDelegateView* BubbleDelegateView::AsBubbleDelegate() {
  return this;
}

bool BubbleDelegateView::CanActivate() const {
  return !use_focusless();
}

View* BubbleDelegateView::GetContentsView() {
  return this;
}

NonClientFrameView* BubbleDelegateView::CreateNonClientFrameView(
    Widget* widget) {
  BubbleBorder::ArrowLocation arrow_loc = arrow_location();
  if (base::i18n::IsRTL())
    arrow_loc = BubbleBorder::horizontal_mirror(arrow_loc);
  // TODO(alicet): Expose the shadow option in BorderContentsView when we make
  // the fullscreen exit bubble use the new bubble code.
  BubbleBorder* border = new BubbleBorder(arrow_loc, BubbleBorder::NO_SHADOW);
  border->set_background_color(color());
  BubbleFrameView* frame_view = new BubbleFrameView(margins(), border);
  frame_view->set_background(new BubbleBackground(border));
  return frame_view;
}

void BubbleDelegateView::OnWidgetClosing(Widget* widget) {
  if (anchor_widget() == widget) {
    anchor_view_ = NULL;
    anchor_widget_ = NULL;
  }
}

void BubbleDelegateView::OnWidgetVisibilityChanged(Widget* widget,
                                                   bool visible) {
  if (widget != GetWidget())
    return;

  if (visible) {
    if (border_widget_) {
      if (CanActivate())
        border_widget_->Show();
      else
        border_widget_->ShowInactive();
    }
    if (CanActivate())
      GetFocusManager()->SetFocusedView(GetInitiallyFocusedView());
    if (anchor_widget() && anchor_widget()->GetTopLevelWidget())
      anchor_widget()->GetTopLevelWidget()->DisableInactiveRendering();
  } else {
    if (border_widget_)
      border_widget_->Hide();
  }
}

void BubbleDelegateView::OnWidgetActivationChanged(Widget* widget,
                                                   bool active) {
  if (close_on_deactivate() && widget == GetWidget() && !active)
    GetWidget()->Close();
}

void BubbleDelegateView::OnWidgetMoved(Widget* widget) {
  if (move_with_anchor() && anchor_widget() == widget)
    SizeToContents();
}

gfx::Rect BubbleDelegateView::GetAnchorRect() {
  if (!anchor_view())
    return gfx::Rect(anchor_point_, gfx::Size());
  gfx::Rect anchor_bounds = anchor_view()->GetBoundsInScreen();
  anchor_bounds.Inset(anchor_insets_);
  return anchor_bounds;
}

void BubbleDelegateView::Show() {
  GetWidget()->Show();
}

void BubbleDelegateView::StartFade(bool fade_in) {
  fade_animation_.reset(new ui::SlideAnimation(this));
  fade_animation_->SetSlideDuration(kHideFadeDurationMS);
  fade_animation_->Reset(fade_in ? 0.0 : 1.0);
  if (fade_in) {
    original_opacity_ = 0;
    if (border_widget_)
      border_widget_->SetOpacity(original_opacity_);
    GetWidget()->SetOpacity(original_opacity_);
    Show();
    fade_animation_->Show();
  } else {
    original_opacity_ = 255;
    fade_animation_->Hide();
  }
}

void BubbleDelegateView::ResetFade() {
  fade_animation_.reset();
  if (border_widget_)
    border_widget_->SetOpacity(original_opacity_);
  GetWidget()->SetOpacity(original_opacity_);
}

void BubbleDelegateView::SetAlignment(BubbleBorder::BubbleAlignment alignment) {
  GetBubbleFrameView()->bubble_border()->set_alignment(alignment);
  SizeToContents();
}

bool BubbleDelegateView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (!close_on_esc() || accelerator.key_code() != ui::VKEY_ESCAPE)
    return false;
  if (fade_animation_.get())
    fade_animation_->Reset();
  GetWidget()->Close();
  return true;
}

void BubbleDelegateView::AnimationEnded(const ui::Animation* animation) {
  if (animation != fade_animation_.get())
    return;
  bool closed = fade_animation_->GetCurrentValue() == 0;
  fade_animation_->Reset();
  if (closed)
    GetWidget()->Close();
}

void BubbleDelegateView::AnimationProgressed(const ui::Animation* animation) {
  if (animation != fade_animation_.get())
    return;
  DCHECK(fade_animation_->is_animating());
  unsigned char opacity = fade_animation_->GetCurrentValue() * 255;
#if defined(OS_WIN) && !defined(USE_AURA)
  // Explicitly set the content Widget's layered style and set transparency via
  // SetLayeredWindowAttributes. This is done because initializing the Widget as
  // transparent and setting opacity via UpdateLayeredWindow doesn't support
  // hosting child native Windows controls.
  const HWND hwnd = GetWidget()->GetNativeView();
  const DWORD style = GetWindowLong(hwnd, GWL_EXSTYLE);
  if ((opacity == 255) == !!(style & WS_EX_LAYERED))
    SetWindowLong(hwnd, GWL_EXSTYLE, style ^ WS_EX_LAYERED);
  SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
  // Update the border widget's opacity.
  border_widget_->SetOpacity(opacity);
#endif
  GetWidget()->SetOpacity(opacity);
}

void BubbleDelegateView::Init() {}

void BubbleDelegateView::SizeToContents() {
#if defined(OS_WIN) && !defined(USE_AURA)
  border_widget_->SetBounds(GetBubbleBounds());
  GetWidget()->SetBounds(GetBubbleClientBounds());

  // Update the local client bounds clipped out by the border widget background.
  // Used to correctly display overlapping semi-transparent widgets on Windows.
  GetBubbleFrameView()->bubble_border()->set_client_bounds(
      GetBubbleFrameView()->GetBoundsForClientView());
#else
  GetWidget()->SetBounds(GetBubbleBounds());
#endif
}

BubbleFrameView* BubbleDelegateView::GetBubbleFrameView() const {
  const Widget* widget = border_widget_ ? border_widget_ : GetWidget();
  const NonClientView* view = widget ? widget->non_client_view() : NULL;
  return view ? static_cast<BubbleFrameView*>(view->frame_view()) : NULL;
}

gfx::Rect BubbleDelegateView::GetBubbleBounds() {
  // The argument rect has its origin at the bubble's arrow anchor point;
  // its size is the preferred size of the bubble's client view (this view).
  return GetBubbleFrameView()->GetUpdatedWindowBounds(GetAnchorRect(),
      GetPreferredSize(), try_mirroring_arrow_);
}

#if defined(OS_WIN) && !defined(USE_AURA)
gfx::Rect BubbleDelegateView::GetBubbleClientBounds() const {
  gfx::Rect client_bounds(GetBubbleFrameView()->GetBoundsForClientView());
  client_bounds.Offset(border_widget_->GetWindowBoundsInScreen().origin());
  return client_bounds;
}
#endif

}  // namespace views
