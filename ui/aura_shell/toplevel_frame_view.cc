// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/toplevel_frame_view.h"

#include "grit/ui_resources.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/custom_button.h"
#include "views/widget/widget.h"
#include "views/widget/widget_delegate.h"

#if !defined(OS_WIN)
#include "views/window/hit_test.h"
#endif

namespace aura_shell {
namespace internal {

namespace {
// The thickness of the left, right and bottom edges of the frame.
const int kFrameBorderThickness = 8;
const int kFrameOpacity = 50;
// The color used to fill the frame.
const SkColor kFrameColor = SkColorSetARGB(kFrameOpacity, 0, 0, 0);
const int kFrameBorderHiddenOpacity = 0;
const int kFrameBorderVisibleOpacity = kFrameOpacity;
// How long the hover animation takes if uninterrupted.
const int kHoverFadeDurationMs = 250;
}  // namespace

// Buttons for window controls - close, zoom, etc.
class WindowControlButton : public views::CustomButton {
 public:
  WindowControlButton(views::ButtonListener* listener,
                      SkColor color,
                      const SkBitmap& icon)
      : views::CustomButton(listener),
        color_(color),
        icon_(icon) {
  }
  virtual ~WindowControlButton() {}

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRectInt(GetBackgroundColor(), 0, 0, width(), height());
    canvas->DrawBitmapInt(icon_, 0, 0);
  }
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size(icon_.width(), icon_.height());
    size.Enlarge(3, 2);
    return size;
  }

 private:
  SkColor GetBackgroundColor() {
    return SkColorSetARGB(hover_animation_->CurrentValueBetween(0, 150),
                          SkColorGetR(color_),
                          SkColorGetG(color_),
                          SkColorGetB(color_));
  }

  SkColor color_;
  SkBitmap icon_;

  DISALLOW_COPY_AND_ASSIGN(WindowControlButton);
};

class SizingBorder : public views::View,
                     public ui::AnimationDelegate {
 public:
  SizingBorder()
      : ALLOW_THIS_IN_INITIALIZER_LIST(
            animation_(new ui::SlideAnimation(this))) {
    animation_->SetSlideDuration(kHoverFadeDurationMs);
  }
  virtual ~SizingBorder() {}

  void Configure(const gfx::Rect& hidden_bounds,
                 const gfx::Rect& visible_bounds) {
    hidden_bounds_ = hidden_bounds;
    visible_bounds_ = visible_bounds;
    SetBoundsRect(hidden_bounds_);
  }

  void Show() {
    animation_->Show();
  }

  void Hide() {
    animation_->Hide();
  }

  bool IsShowing() const {
    return animation_->IsShowing();
  }

  bool IsHiding() const {
    return animation_->IsClosing();
  }

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    // Fill with current opacity value.
    int opacity = animation_->CurrentValueBetween(kFrameBorderHiddenOpacity,
                                                  kFrameBorderVisibleOpacity);
    canvas->FillRectInt(SkColorSetARGB(opacity,
                                       SkColorGetR(kFrameColor),
                                       SkColorGetG(kFrameColor),
                                       SkColorGetB(kFrameColor)),
                        0, 0, width(), height());
  }

  // Overridden from ui::AnimationDelegate:
  void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    // TODO: update bounds.
    gfx::Rect current_bounds = animation_->CurrentValueBetween(hidden_bounds_,
                                                               visible_bounds_);
    SetBoundsRect(current_bounds);
    SchedulePaint();
  }

  // Each of these represents the hidden/visible states of the sizing border.
  // When the view is shown or hidden it animates between them.
  gfx::Rect hidden_bounds_;
  gfx::Rect visible_bounds_;

  scoped_ptr<ui::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(SizingBorder);
};

////////////////////////////////////////////////////////////////////////////////
// ToplevelFrameView, public:

ToplevelFrameView::ToplevelFrameView()
    : current_hittest_code_(HTNOWHERE),
      left_edge_(new SizingBorder),
      right_edge_(new SizingBorder),
      bottom_edge_(new SizingBorder) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  close_button_ =
      new WindowControlButton(this, SK_ColorRED,
                              *rb.GetBitmapNamed(IDR_AURA_WINDOW_CLOSE_ICON));
  zoom_button_ =
      new WindowControlButton(this, SK_ColorGREEN,
                              *rb.GetBitmapNamed(IDR_AURA_WINDOW_ZOOM_ICON));
  AddChildView(close_button_);
  AddChildView(zoom_button_);

  AddChildView(left_edge_);
  AddChildView(right_edge_);
  AddChildView(bottom_edge_);
}

ToplevelFrameView::~ToplevelFrameView() {
}

////////////////////////////////////////////////////////////////////////////////
// ToplevelFrameView, private:

int ToplevelFrameView::NonClientBorderThickness() const {
  return kFrameBorderThickness;
}

int ToplevelFrameView::NonClientTopBorderHeight() const {
  return close_button_->GetPreferredSize().height();
}

int ToplevelFrameView::NonClientHitTestImpl(const gfx::Point& point) {
  // Sanity check.
  if (!GetLocalBounds().Contains(point))
    return HTNOWHERE;

  // The client view gets first crack at claiming the point.
  int frame_component = GetWidget()->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // The window controls come second.
  if (close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;
  else if (zoom_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;

  // Finally other portions of the frame/sizing border.
  frame_component =
      GetHTComponentForFrame(point,
                             NonClientBorderThickness(),
                             NonClientBorderThickness(),
                             NonClientBorderThickness(),
                             NonClientBorderThickness(),
                             GetWidget()->widget_delegate()->CanResize());

  // Coerce HTCAPTION as a fallback.
  return frame_component == HTNOWHERE ? HTCAPTION : frame_component;
}

void ToplevelFrameView::ShowSizingBorder(SizingBorder* sizing_border) {
  if (sizing_border && !sizing_border->IsShowing())
    sizing_border->Show();
  if (left_edge_ != sizing_border && !left_edge_->IsHiding())
    left_edge_->Hide();
  if (right_edge_ != sizing_border && !right_edge_->IsHiding())
    right_edge_->Hide();
  if (bottom_edge_ != sizing_border && !bottom_edge_->IsHiding())
    bottom_edge_->Hide();
}

bool ToplevelFrameView::PointIsInChildView(views::View* child,
                                           const gfx::Point& point) const {
  gfx::Point child_point(point);
  ConvertPointToView(this, child, &child_point);
  return child->HitTest(child_point);
}

gfx::Rect ToplevelFrameView::GetHiddenBoundsForSizingBorder(
    int frame_component) const {
  int border_size = NonClientBorderThickness();
  int caption_height = NonClientTopBorderHeight();
  switch (frame_component) {
    case HTLEFT:
      return gfx::Rect(border_size, caption_height, border_size,
                       height() - border_size - caption_height);
    case HTBOTTOM:
      return gfx::Rect(border_size, height() - 2 * border_size,
                       width() - 2 * border_size, border_size);
    case HTRIGHT:
      return gfx::Rect(width() - 2 * border_size, caption_height, border_size,
                       height() - border_size - caption_height);
    default:
      break;
  }
  return gfx::Rect();
}

gfx::Rect ToplevelFrameView::GetVisibleBoundsForSizingBorder(
    int frame_component) const {
  int border_size = NonClientBorderThickness();
  int caption_height = NonClientTopBorderHeight();
  switch (frame_component) {
    case HTLEFT:
      return gfx::Rect(0, caption_height, border_size,
                       height() - border_size - caption_height);
    case HTBOTTOM:
      return gfx::Rect(border_size, height() - border_size,
                       width() - 2 * border_size, border_size);
    case HTRIGHT:
      return gfx::Rect(width() - border_size, caption_height, border_size,
                       height() - border_size - caption_height);
    default:
      break;
  }
  return gfx::Rect();
}

////////////////////////////////////////////////////////////////////////////////
// ToplevelFrameView, views::NonClientFrameView overrides:

gfx::Rect ToplevelFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect ToplevelFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(-NonClientBorderThickness(),
                      -NonClientTopBorderHeight(),
                      -NonClientBorderThickness(),
                      -NonClientBorderThickness());
  return window_bounds;
}

int ToplevelFrameView::NonClientHitTest(const gfx::Point& point) {
  current_hittest_code_ = NonClientHitTestImpl(point);
  return current_hittest_code_;
}

void ToplevelFrameView::GetWindowMask(const gfx::Size& size,
                                      gfx::Path* window_mask) {
  // Nothing.
}

void ToplevelFrameView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

void ToplevelFrameView::ResetWindowControls() {
  NOTIMPLEMENTED();
}

void ToplevelFrameView::UpdateWindowIcon() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// ToplevelFrameView, views::View overrides:

void ToplevelFrameView::Layout() {
  client_view_bounds_ = GetLocalBounds();
  client_view_bounds_.Inset(NonClientBorderThickness(),
                            NonClientTopBorderHeight(),
                            NonClientBorderThickness(),
                            NonClientBorderThickness());

  gfx::Size close_button_ps = close_button_->GetPreferredSize();
  close_button_->SetBoundsRect(
      gfx::Rect(width() - close_button_ps.width() - NonClientBorderThickness(),
                0, close_button_ps.width(), close_button_ps.height()));

  gfx::Size zoom_button_ps = zoom_button_->GetPreferredSize();
  zoom_button_->SetBoundsRect(
      gfx::Rect(close_button_->x() - zoom_button_ps.width(), 0,
                zoom_button_ps.width(), zoom_button_ps.height()));

  left_edge_->Configure(GetHiddenBoundsForSizingBorder(HTLEFT),
                        GetVisibleBoundsForSizingBorder(HTLEFT));
  right_edge_->Configure(GetHiddenBoundsForSizingBorder(HTRIGHT),
                         GetVisibleBoundsForSizingBorder(HTRIGHT));
  bottom_edge_->Configure(GetHiddenBoundsForSizingBorder(HTBOTTOM),
                          GetVisibleBoundsForSizingBorder(HTBOTTOM));
}

void ToplevelFrameView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect caption_rect(NonClientBorderThickness(), 0,
                         width() - 2 * NonClientBorderThickness(),
                         NonClientTopBorderHeight());
  canvas->FillRectInt(kFrameColor, caption_rect.x(), caption_rect.y(),
                      caption_rect.width(), caption_rect.height());
}

void ToplevelFrameView::OnMouseMoved(const views::MouseEvent& event) {
  switch (current_hittest_code_) {
    case HTLEFT:
      ShowSizingBorder(left_edge_);
      break;
    case HTRIGHT:
      ShowSizingBorder(right_edge_);
      break;
    case HTBOTTOM:
      ShowSizingBorder(bottom_edge_);
      break;
    default:
      break;
  }
}

void ToplevelFrameView::OnMouseExited(const views::MouseEvent& event) {
  ShowSizingBorder(NULL);
}

views::View* ToplevelFrameView::GetEventHandlerForPoint(
    const gfx::Point& point) {
  if (PointIsInChildView(left_edge_, point) ||
      PointIsInChildView(right_edge_, point) ||
      PointIsInChildView(bottom_edge_, point)) {
    return this;
  }
  return View::GetEventHandlerForPoint(point);
}


////////////////////////////////////////////////////////////////////////////////
// ToplevelFrameView, views::ButtonListener implementation:

void ToplevelFrameView::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  if (sender == close_button_) {
    GetWidget()->Close();
  } else if (sender == zoom_button_) {
    if (GetWidget()->IsMaximized())
      GetWidget()->Restore();
    else
      GetWidget()->Maximize();
  }
}

}  // namespace internal
}  // namespace aura_shell
