// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_delegate.h"

#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

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
  bubble_params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
  bubble_params.accept_events = bubble->accept_events();
  if (bubble->parent_window())
    bubble_params.parent = bubble->parent_window();
  else if (bubble->anchor_widget())
    bubble_params.parent = bubble->anchor_widget()->GetNativeView();
  bubble_params.can_activate = bubble->CanActivate();
#if defined(OS_WIN) && !defined(USE_AURA)
  bubble_params.type = Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  bubble_params.opacity = Widget::InitParams::OPAQUE_WINDOW;
#endif
  bubble_widget->Init(bubble_params);
  return bubble_widget;
}

}  // namespace

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
    DetachFromBubble();
  }

  // WidgetDelegate overrides:
  virtual bool CanActivate() const OVERRIDE { return false; }
  virtual string16 GetWindowTitle() const OVERRIDE {
    return bubble_->GetWindowTitle();
  }
  virtual bool ShouldShowCloseButton() const OVERRIDE {
    return bubble_->ShouldShowCloseButton();
  }
  virtual void DeleteDelegate() OVERRIDE { delete this; }
  virtual Widget* GetWidget() OVERRIDE { return widget_; }
  virtual const Widget* GetWidget() const OVERRIDE { return widget_; }
  virtual NonClientFrameView* CreateNonClientFrameView(
      Widget* widget) OVERRIDE {
    return bubble_->CreateNonClientFrameView(widget);
  }

  // WidgetObserver overrides:
  virtual void OnWidgetDestroying(Widget* widget) OVERRIDE {
    DetachFromBubble();
    widget_->Close();
  }

 private:
  // Removes state installed on |bubble_|, including references |bubble_| has to
  // us.
  void DetachFromBubble() {
    if (!bubble_)
      return;
    if (bubble_->GetWidget())
      bubble_->GetWidget()->RemoveObserver(this);
    bubble_->border_widget_ = NULL;
    bubble_ = NULL;
  }

  BubbleDelegateView* bubble_;
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBorderDelegate);
};

namespace {

// Create a widget to host the bubble's border.
Widget* CreateBorderWidget(BubbleDelegateView* bubble) {
  Widget* border_widget = new Widget();
  Widget::InitParams border_params(Widget::InitParams::TYPE_BUBBLE);
  border_params.delegate = new BubbleBorderDelegate(bubble, border_widget);
  border_params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
  border_params.parent = bubble->GetWidget()->GetNativeView();
  border_params.can_activate = false;
  border_params.accept_events = bubble->border_accepts_events();
  border_widget->Init(border_params);
  border_widget->set_focus_on_creation(false);
  return border_widget;
}

}  // namespace

#endif

BubbleDelegateView::BubbleDelegateView()
    : close_on_esc_(true),
      close_on_deactivate_(true),
      anchor_view_(NULL),
      anchor_widget_(NULL),
      move_with_anchor_(false),
      arrow_(BubbleBorder::TOP_LEFT),
      shadow_(BubbleBorder::SMALL_SHADOW),
      color_explicitly_set_(false),
      margins_(kDefaultMargin, kDefaultMargin, kDefaultMargin, kDefaultMargin),
      original_opacity_(255),
      border_widget_(NULL),
      use_focusless_(false),
      accept_events_(true),
      border_accepts_events_(true),
      adjust_if_offscreen_(true),
      parent_window_(NULL) {
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  UpdateColorsFromTheme(GetNativeTheme());
}

BubbleDelegateView::BubbleDelegateView(
    View* anchor_view,
    BubbleBorder::Arrow arrow)
    : close_on_esc_(true),
      close_on_deactivate_(true),
      anchor_view_(anchor_view),
      anchor_widget_(NULL),
      move_with_anchor_(false),
      arrow_(arrow),
      shadow_(BubbleBorder::SMALL_SHADOW),
      color_explicitly_set_(false),
      margins_(kDefaultMargin, kDefaultMargin, kDefaultMargin, kDefaultMargin),
      original_opacity_(255),
      border_widget_(NULL),
      use_focusless_(false),
      accept_events_(true),
      border_accepts_events_(true),
      adjust_if_offscreen_(true),
      parent_window_(NULL) {
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  UpdateColorsFromTheme(GetNativeTheme());
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

#if defined(OS_WIN)
#if defined(USE_AURA)
  // If glass is enabled, the bubble is allowed to extend outside the bounds of
  // the parent frame and let DWM handle compositing.  If not, then we don't
  // want to allow the bubble to extend the frame because it will be clipped.
  bubble_delegate->set_adjust_if_offscreen(ui::win::IsAeroGlassEnabled());
#else
  // First set the contents view to initialize view bounds for widget sizing.
  bubble_widget->SetContentsView(bubble_delegate->GetContentsView());
  bubble_delegate->border_widget_ = CreateBorderWidget(bubble_delegate);
#endif
#endif

  bubble_delegate->SizeToContents();
  bubble_widget->AddObserver(bubble_delegate);
  return bubble_widget;
}

BubbleDelegateView* BubbleDelegateView::AsBubbleDelegate() {
  return this;
}

bool BubbleDelegateView::CanActivate() const {
  return !use_focusless();
}

bool BubbleDelegateView::ShouldShowCloseButton() const {
  return false;
}

View* BubbleDelegateView::GetContentsView() {
  return this;
}

NonClientFrameView* BubbleDelegateView::CreateNonClientFrameView(
    Widget* widget) {
  BubbleFrameView* frame = new BubbleFrameView(margins());
  const BubbleBorder::Arrow adjusted_arrow = base::i18n::IsRTL() ?
      BubbleBorder::horizontal_mirror(arrow()) : arrow();
  frame->SetBubbleBorder(new BubbleBorder(adjusted_arrow, shadow(), color()));
  return frame;
}

void BubbleDelegateView::OnWidgetDestroying(Widget* widget) {
  if (anchor_widget() == widget) {
    anchor_widget_->RemoveObserver(this);
    anchor_view_ = NULL;
    anchor_widget_ = NULL;
  }
}

void BubbleDelegateView::OnWidgetVisibilityChanging(Widget* widget,
                                                    bool visible) {
#if defined(OS_WIN)
  // On Windows we need to handle this before the bubble is visible or hidden.
  // Please see the comment on the OnWidgetVisibilityChanging function. On
  // other platforms it is fine to handle it after the bubble is shown/hidden.
  HandleVisibilityChanged(widget, visible);
#endif
}

void BubbleDelegateView::OnWidgetVisibilityChanged(Widget* widget,
                                                   bool visible) {
#if !defined(OS_WIN)
  HandleVisibilityChanged(widget, visible);
#endif
}

void BubbleDelegateView::OnWidgetActivationChanged(Widget* widget,
                                                   bool active) {
  if (close_on_deactivate() && widget == GetWidget() && !active)
    GetWidget()->Close();
}

void BubbleDelegateView::OnWidgetBoundsChanged(Widget* widget,
                                               const gfx::Rect& new_bounds) {
  if (move_with_anchor() && anchor_widget() == widget)
    SizeToContents();
}

gfx::Rect BubbleDelegateView::GetAnchorRect() {
  if (!anchor_view())
    return anchor_rect_;
  gfx::Rect anchor_bounds = anchor_view()->GetBoundsInScreen();
  anchor_bounds.Inset(anchor_view_insets_);
  return anchor_bounds;
}

void BubbleDelegateView::StartFade(bool fade_in) {
#if defined(USE_AURA)
  // Use AURA's window layer animation instead of fading. This ensures that
  // hosts which rely on the layer animation callbacks to close the window
  // work correctly.
  if (fade_in)
    GetWidget()->Show();
  else
    GetWidget()->Close();
#else
  fade_animation_.reset(new gfx::SlideAnimation(this));
  fade_animation_->SetSlideDuration(GetFadeDuration());
  fade_animation_->Reset(fade_in ? 0.0 : 1.0);
  if (fade_in) {
    original_opacity_ = 0;
    if (border_widget_)
      border_widget_->SetOpacity(original_opacity_);
    GetWidget()->SetOpacity(original_opacity_);
    GetWidget()->Show();
    fade_animation_->Show();
  } else {
    original_opacity_ = 255;
    fade_animation_->Hide();
  }
#endif
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

void BubbleDelegateView::SetArrowPaintType(
    BubbleBorder::ArrowPaintType paint_type) {
  GetBubbleFrameView()->bubble_border()->set_paint_arrow(paint_type);
  SizeToContents();
}

void BubbleDelegateView::OnAnchorViewBoundsChanged() {
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

void BubbleDelegateView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateColorsFromTheme(theme);
}

void BubbleDelegateView::AnimationEnded(const gfx::Animation* animation) {
  if (animation != fade_animation_.get())
    return;
  bool closed = fade_animation_->GetCurrentValue() == 0;
  fade_animation_->Reset();
  if (closed)
    GetWidget()->Close();
}

void BubbleDelegateView::AnimationProgressed(const gfx::Animation* animation) {
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
      GetPreferredSize(), adjust_if_offscreen_);
}

int BubbleDelegateView::GetFadeDuration() {
  return kHideFadeDurationMS;
}

void BubbleDelegateView::UpdateColorsFromTheme(const ui::NativeTheme* theme) {
  if (!color_explicitly_set_) {
    color_ = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_WindowBackground);
  }
  set_background(Background::CreateSolidBackground(color()));
  BubbleFrameView* frame_view = GetBubbleFrameView();
  if (frame_view)
    frame_view->bubble_border()->set_background_color(color());
}

#if defined(OS_WIN) && !defined(USE_AURA)
gfx::Rect BubbleDelegateView::GetBubbleClientBounds() const {
  gfx::Rect client_bounds(GetBubbleFrameView()->GetBoundsForClientView());
  client_bounds.Offset(
      border_widget_->GetWindowBoundsInScreen().OffsetFromOrigin());
  return client_bounds;
}
#endif

void BubbleDelegateView::HandleVisibilityChanged(Widget* widget,
                                                 bool visible) {
  if (widget != GetWidget())
    return;

  if (visible) {
    if (border_widget_)
      border_widget_->ShowInactive();
    if (anchor_widget() && anchor_widget()->GetTopLevelWidget())
      anchor_widget()->GetTopLevelWidget()->DisableInactiveRendering();
  } else {
    if (border_widget_)
      border_widget_->Hide();
  }
}

}  // namespace views
