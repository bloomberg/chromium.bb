// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_delegate.h"

#include "ui/base/animation/slide_animation.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/widget/widget.h"

// The duration of the fade animation in milliseconds.
static const int kHideFadeDurationMS = 200;

namespace views {

namespace {

// Create a widget to host the bubble.
Widget* CreateBubbleWidget(BubbleDelegateView* bubble, Widget* parent) {
  Widget* bubble_widget = new Widget();
  Widget::InitParams bubble_params(Widget::InitParams::TYPE_BUBBLE);
  bubble_params.delegate = bubble;
  bubble_params.transparent = true;
  bubble_params.parent_widget = parent;
#if defined(OS_WIN) && !defined(USE_AURA)
  bubble_params.type = Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  bubble_params.transparent = false;
#endif
  bubble_widget->Init(bubble_params);
  return bubble_widget;
}

#if defined(OS_WIN) && !defined(USE_AURA)
// The border widget's delegate, needed for transparent Windows native controls.
// TODO(msw): Remove this when Windows native controls are no longer needed.
class VIEWS_EXPORT BubbleBorderDelegateView : public WidgetDelegateView {
 public:
  explicit BubbleBorderDelegateView(BubbleDelegateView* bubble)
      : bubble_(bubble) {}
  virtual ~BubbleBorderDelegateView() {}

  // WidgetDelegateView overrides:
  virtual bool CanActivate() const OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

 private:
  BubbleDelegateView* bubble_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBorderDelegateView);
};

bool BubbleBorderDelegateView::CanActivate() const { return false; }

NonClientFrameView* BubbleBorderDelegateView::CreateNonClientFrameView() {
  return bubble_->CreateNonClientFrameView();
}

// Create a widget to host the bubble's border.
Widget* CreateBorderWidget(BubbleDelegateView* bubble, Widget* parent) {
  Widget* border_widget = new Widget();
  Widget::InitParams border_params(Widget::InitParams::TYPE_BUBBLE);
  border_params.delegate = new BubbleBorderDelegateView(bubble);
  border_params.transparent = true;
  border_params.parent_widget = parent;
  if (!border_params.parent_widget)
    border_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  border_widget->Init(border_params);
  return border_widget;
}
#endif

}  // namespace

BubbleDelegateView::BubbleDelegateView()
    : close_on_esc_(true),
      close_on_deactivate_(true),
      allow_bubble_offscreen_(false),
      anchor_view_(NULL),
      arrow_location_(BubbleBorder::TOP_LEFT),
      color_(SK_ColorWHITE),
      border_widget_(NULL),
      use_focusless_(false) {
  set_background(views::Background::CreateSolidBackground(color_));
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, 0));
}

BubbleDelegateView::BubbleDelegateView(
    View* anchor_view,
    BubbleBorder::ArrowLocation arrow_location,
    const SkColor& color)
    : close_on_esc_(true),
      close_on_deactivate_(true),
      allow_bubble_offscreen_(false),
      anchor_view_(anchor_view),
      arrow_location_(arrow_location),
      color_(color),
      original_opacity_(255),
      border_widget_(NULL),
      use_focusless_(false) {
  set_background(views::Background::CreateSolidBackground(color_));
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, 0));
}

BubbleDelegateView::~BubbleDelegateView() {
  if (border_widget_)
    border_widget_->Close();
}

// static
Widget* BubbleDelegateView::CreateBubble(BubbleDelegateView* bubble_delegate) {
  bubble_delegate->Init();
  Widget* parent = bubble_delegate->anchor_view() ?
      bubble_delegate->anchor_view()->GetWidget() : NULL;
  Widget* bubble_widget = CreateBubbleWidget(bubble_delegate, parent);

#if defined(OS_WIN) && !defined(USE_AURA)
  // First set the contents view to initialize view bounds for widget sizing.
  bubble_widget->SetContentsView(bubble_delegate->GetContentsView());
  bubble_delegate->border_widget_ = CreateBorderWidget(bubble_delegate, parent);
#endif

  bubble_delegate->SizeToContents();
  bubble_widget->AddObserver(bubble_delegate);
  if (parent && parent->GetTopLevelWidget())
    parent->GetTopLevelWidget()->DisableInactiveRendering();
  return bubble_widget;
}

View* BubbleDelegateView::GetInitiallyFocusedView() {
  return this;
}

BubbleDelegateView* BubbleDelegateView::AsBubbleDelegate() {
  return this;
}

View* BubbleDelegateView::GetContentsView() {
  return this;
}

NonClientFrameView* BubbleDelegateView::CreateNonClientFrameView() {
  return new BubbleFrameView(GetArrowLocation(),
                             GetPreferredSize(),
                             GetColor(),
                             allow_bubble_offscreen_);
}

void BubbleDelegateView::OnWidgetActivationChanged(Widget* widget,
                                                   bool active) {
  if (close_on_deactivate() && widget == GetWidget() && !active) {
    GetWidget()->RemoveObserver(this);
    GetWidget()->Close();
  }
}

gfx::Point BubbleDelegateView::GetAnchorPoint() {
  if (!anchor_view())
    return gfx::Point();

  BubbleBorder::ArrowLocation location = GetArrowLocation();
  gfx::Point anchor(anchor_view()->bounds().CenterPoint());
  // By default, pick the middle of |anchor_view_|'s edge opposite the arrow.
  if (BubbleBorder::is_arrow_on_horizontal(location)) {
    anchor.SetPoint(anchor_view()->width() / 2,
        BubbleBorder::is_arrow_on_top(location) ? anchor_view()->height() : 0);
  } else if (BubbleBorder::has_arrow(location)) {
    anchor.SetPoint(
        BubbleBorder::is_arrow_on_left(location) ? anchor_view()->width() : 0,
        anchor_view_->height() / 2);
  }
  View::ConvertPointToScreen(anchor_view(), &anchor);
  return anchor;
}

BubbleBorder::ArrowLocation BubbleDelegateView::GetArrowLocation() const {
  return arrow_location_;
}

SkColor BubbleDelegateView::GetColor() const {
  return color_;
}

void BubbleDelegateView::Show() {
  if (border_widget_)
    border_widget_->Show();
  GetWidget()->Show();
  GetFocusManager()->SetFocusedView(GetInitiallyFocusedView());
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
  border_widget_->non_client_view()->SchedulePaint();
#endif
  GetWidget()->SetOpacity(opacity);
  SchedulePaint();
}

void BubbleDelegateView::Init() {}

void BubbleDelegateView::SizeToContents() {
#if defined(OS_WIN) && !defined(USE_AURA)
  border_widget_->SetBounds(GetBubbleBounds());
  GetWidget()->SetBounds(GetBubbleClientBounds());
#else
  GetWidget()->SetBounds(GetBubbleBounds());
#endif
}

BubbleFrameView* BubbleDelegateView::GetBubbleFrameView() const {
  const Widget* widget = border_widget_ ? border_widget_ : GetWidget();
  return static_cast<BubbleFrameView*>(widget->non_client_view()->frame_view());
}

gfx::Rect BubbleDelegateView::GetBubbleBounds() {
  // The argument rect has its origin at the bubble's arrow anchor point;
  // its size is the preferred size of the bubble's client view (this view).
  return GetBubbleFrameView()->GetWindowBoundsForClientBounds(
      gfx::Rect(GetAnchorPoint(), GetPreferredSize()));
}

#if defined(OS_WIN) && !defined(USE_AURA)
gfx::Rect BubbleDelegateView::GetBubbleClientBounds() const {
  gfx::Rect client_bounds(GetBubbleFrameView()->GetBoundsForClientView());
  client_bounds.Offset(border_widget_->GetWindowScreenBounds().origin());
  return client_bounds;
}
#endif

}  // namespace views
