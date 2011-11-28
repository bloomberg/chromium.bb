// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/toplevel_frame_view.h"

#include "grit/ui_resources.h"
#include "ui/aura/cursor.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

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
    canvas->FillRect(GetBackgroundColor(), GetLocalBounds());
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

// Base class for all animatable frame components such as sizing borders and
// the window's caption. Provides shared animation and event-handling logic.
class FrameComponent : public views::View,
                       public ui::AnimationDelegate {
 public:
  virtual ~FrameComponent() {
  }

  // Control animations.
  void Show() {
    animation_->Show();
  }
  void Hide() {
    animation_->Hide();
  }

  // Current animation state.
  bool IsShowing() const {
    return animation_->IsShowing();
  }
  bool IsHiding() const {
    return animation_->IsClosing();
  }

  // Returns true if the view ignores events to itself or its children at the
  // specified point in its parent's coordinates. By default, any events within
  // the bounds of this view are ignored so that the parent (the NCFV) can
  // handle them instead. Derived classes can override to disable this for some
  // of their children.
  virtual bool IgnoreEventsForPoint(const gfx::Point& point) {
    gfx::Point translated_point(point);
    ConvertPointToView(parent(), this, &translated_point);
    return HitTest(translated_point);
  }

 protected:
  FrameComponent()
      : ALLOW_THIS_IN_INITIALIZER_LIST(
            animation_(new ui::SlideAnimation(this))) {
    animation_->SetSlideDuration(kHoverFadeDurationMs);
  }

  // Most of the frame components are rendered with a transparent bg.
  void PaintTransparentBackground(gfx::Canvas* canvas) {
    // Fill with current opacity value.
    int opacity = animation_->CurrentValueBetween(kFrameBorderHiddenOpacity,
                                                  kFrameBorderVisibleOpacity);
    canvas->FillRect(SkColorSetARGB(opacity,
                                    SkColorGetR(kFrameColor),
                                    SkColorGetG(kFrameColor),
                                    SkColorGetB(kFrameColor)),
                     GetLocalBounds());
  }

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    SchedulePaint();
  }

 private:
  scoped_ptr<ui::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(FrameComponent);
};

// A view that renders the title bar of the window, and also hosts the window
// controls.
class WindowCaption : public FrameComponent,
                      public views::ButtonListener {
 public:
  WindowCaption() {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    close_button_ =
        new WindowControlButton(this, SK_ColorRED,
                                *rb.GetBitmapNamed(IDR_AURA_WINDOW_CLOSE_ICON));
    zoom_button_ =
        new WindowControlButton(this, SK_ColorGREEN,
                                *rb.GetBitmapNamed(IDR_AURA_WINDOW_ZOOM_ICON));
    AddChildView(close_button_);
    AddChildView(zoom_button_);
  }
  virtual ~WindowCaption() {}

  // Returns the hit-test code for the specified point in parent coordinates.
  int NonClientHitTest(const gfx::Point& point) {
    gfx::Point translated_point(point);
    View::ConvertPointToView(parent(), this, &translated_point);
    // The window controls come second.
    if (close_button_->GetMirroredBounds().Contains(translated_point))
      return HTCLOSE;
    else if (zoom_button_->GetMirroredBounds().Contains(translated_point))
      return HTMAXBUTTON;
    return HTNOWHERE;
  }

  // Overridden from FrameComponent:
  virtual bool IgnoreEventsForPoint(const gfx::Point& point) OVERRIDE {
    gfx::Point translated_point(point);
    ConvertPointToView(parent(), this, &translated_point);
    if (PointIsInChildView(close_button_, translated_point))
      return false;
    if (PointIsInChildView(zoom_button_, translated_point))
      return false;
    return FrameComponent::IgnoreEventsForPoint(point);
  }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(0, close_button_->GetPreferredSize().height());
  }

 private:
  // Returns true if the specified |point| in this view's coordinates hit tests
  // against |child|, a child view of this view.
  bool PointIsInChildView(views::View* child,
                          const gfx::Point& point) const {
    gfx::Point child_point(point);
    ConvertPointToView(this, child, &child_point);
    return child->HitTest(child_point);
  }

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    PaintTransparentBackground(canvas);
  }
  virtual void Layout() OVERRIDE {
    gfx::Size close_button_ps = close_button_->GetPreferredSize();
    close_button_->SetBoundsRect(
        gfx::Rect(width() - close_button_ps.width(),
                  0, close_button_ps.width(), close_button_ps.height()));

    gfx::Size zoom_button_ps = zoom_button_->GetPreferredSize();
    zoom_button_->SetBoundsRect(
        gfx::Rect(close_button_->x() - zoom_button_ps.width(), 0,
                  zoom_button_ps.width(), zoom_button_ps.height()));
  }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    if (sender == close_button_) {
      GetWidget()->Close();
    } else if (sender == zoom_button_) {
      if (GetWidget()->IsMaximized())
        GetWidget()->Restore();
      else
        GetWidget()->Maximize();
    }
  }

  views::Button* close_button_;
  views::Button* zoom_button_;

  DISALLOW_COPY_AND_ASSIGN(WindowCaption);
};

// A class that renders the sizing border that appears when the user moves
// their mouse over a sizing edge. This view is not actually responsible for
// resizing the window, the EventFilter is.
class SizingBorder : public FrameComponent {
 public:
  SizingBorder() {}
  virtual ~SizingBorder() {}

  void Configure(const gfx::Rect& hidden_bounds,
                 const gfx::Rect& visible_bounds) {
    hidden_bounds_ = hidden_bounds;
    visible_bounds_ = visible_bounds;
    SetBoundsRect(hidden_bounds_);
  }

 protected:
  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    gfx::Rect current_bounds = animation->CurrentValueBetween(hidden_bounds_,
                                                              visible_bounds_);
    SetBoundsRect(current_bounds);
    FrameComponent::AnimationProgressed(animation);
  }

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    PaintTransparentBackground(canvas);
  }

  // Each of these represents the hidden/visible states of the sizing border.
  // When the view is shown or hidden it animates between them.
  gfx::Rect hidden_bounds_;
  gfx::Rect visible_bounds_;

  DISALLOW_COPY_AND_ASSIGN(SizingBorder);
};

////////////////////////////////////////////////////////////////////////////////
// ToplevelFrameView, public:

ToplevelFrameView::ToplevelFrameView()
    : current_hittest_code_(HTNOWHERE),
      caption_(new WindowCaption),
      left_edge_(new SizingBorder),
      right_edge_(new SizingBorder),
      bottom_edge_(new SizingBorder) {
  AddChildView(caption_);
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
  return caption_->GetPreferredSize().height();
}

int ToplevelFrameView::NonClientHitTestImpl(const gfx::Point& point) {
  // Sanity check.
  if (!GetLocalBounds().Contains(point))
    return HTNOWHERE;

  // The client view gets first crack at claiming the point.
  int frame_component = GetWidget()->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  frame_component = caption_->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

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

void ToplevelFrameView::ShowFrameComponent(FrameComponent* frame_component) {
  if (frame_component && !frame_component->IsShowing())
    frame_component->Show();
  if (caption_ != frame_component && !caption_->IsHiding())
    caption_->Hide();
  if (left_edge_ != frame_component && !left_edge_->IsHiding())
    left_edge_->Hide();
  if (right_edge_ != frame_component && !right_edge_->IsHiding())
    right_edge_->Hide();
  if (bottom_edge_ != frame_component && !bottom_edge_->IsHiding())
    bottom_edge_->Hide();
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

  caption_->SetBounds(NonClientBorderThickness(), 0,
                      width() - 2 * NonClientBorderThickness(),
                      NonClientTopBorderHeight());

  left_edge_->Configure(GetHiddenBoundsForSizingBorder(HTLEFT),
                        GetVisibleBoundsForSizingBorder(HTLEFT));
  right_edge_->Configure(GetHiddenBoundsForSizingBorder(HTRIGHT),
                         GetVisibleBoundsForSizingBorder(HTRIGHT));
  bottom_edge_->Configure(GetHiddenBoundsForSizingBorder(HTBOTTOM),
                          GetVisibleBoundsForSizingBorder(HTBOTTOM));
}

void ToplevelFrameView::OnMouseMoved(const views::MouseEvent& event) {
  switch (current_hittest_code_) {
    case HTLEFT:
      ShowFrameComponent(left_edge_);
      break;
    case HTRIGHT:
      ShowFrameComponent(right_edge_);
      break;
    case HTBOTTOM:
      ShowFrameComponent(bottom_edge_);
      break;
    case HTCAPTION:
      ShowFrameComponent(caption_);
      break;
    default:
      break;
  }
}

void ToplevelFrameView::OnMouseExited(const views::MouseEvent& event) {
  ShowFrameComponent(NULL);
}

views::View* ToplevelFrameView::GetEventHandlerForPoint(
    const gfx::Point& point) {
  if (left_edge_->IgnoreEventsForPoint(point) ||
      right_edge_->IgnoreEventsForPoint(point) ||
      bottom_edge_->IgnoreEventsForPoint(point) ||
      caption_->IgnoreEventsForPoint(point)) {
    return this;
  }
  return View::GetEventHandlerForPoint(point);
}

gfx::NativeCursor ToplevelFrameView::GetCursor(const views::MouseEvent& event) {
  switch (current_hittest_code_) {
    case HTBOTTOM:
      return aura::kCursorSouthResize;
    case HTBOTTOMLEFT:
      return aura::kCursorSouthWestResize;
    case HTBOTTOMRIGHT:
      return aura::kCursorSouthEastResize;
    case HTLEFT:
      return aura::kCursorWestResize;
    case HTRIGHT:
      return aura::kCursorEastResize;
    case HTTOP:
      return aura::kCursorNorthResize;
    case HTTOPLEFT:
      return aura::kCursorNorthWestResize;
    case HTTOPRIGHT:
      return aura::kCursorNorthEastResize;
    default:
      return aura::kCursorNull;
  }
}

}  // namespace internal
}  // namespace aura_shell
