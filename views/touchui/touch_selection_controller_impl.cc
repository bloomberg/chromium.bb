// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/touchui/touch_selection_controller_impl.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"
#include "views/widget/widget.h"
#include "views/controls/label.h"
#include "views/background.h"

namespace {

// Constants defining the visual attributes of selection handles
const int kSelectionHandleRadius = 10;
const int kSelectionHandleCursorHeight = 10;
const int kSelectionHandleAlpha = 0x7F;
const SkColor kSelectionHandleColor =
    SkColorSetA(SK_ColorBLUE, kSelectionHandleAlpha);

// Convenience struct to represent a circle shape.
struct Circle {
  int radius;
  gfx::Point center;
  SkColor color;
};

// Creates a widget to host SelectionHandleView.
views::Widget* CreateSelectionHandleWidget() {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.can_activate = false;
  params.transparent = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  return widget;
}

void PaintCircle(const Circle& circle, gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(circle.color);
  gfx::Path path;
  gfx::Rect bounds(circle.center.x() - circle.radius,
                   circle.center.y() - circle.radius,
                   circle.radius * 2,
                   circle.radius * 2);
  SkRect rect;
  rect.set(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()),
           SkIntToScalar(bounds.right()), SkIntToScalar(bounds.bottom()));
  SkScalar radius = SkIntToScalar(circle.radius);
  path.addRoundRect(rect, radius, radius);
  canvas->AsCanvasSkia()->drawPath(path, paint);
}

}  // namespace

namespace views {

// A View that displays the text selection handle.
class TouchSelectionControllerImpl::SelectionHandleView : public View {
 public:
  SelectionHandleView(TouchSelectionControllerImpl* controller)
      : controller_(controller) {
    widget_.reset(CreateSelectionHandleWidget());
    widget_->SetContentsView(this);
    widget_->SetAlwaysOnTop(true);

    // We are owned by the TouchSelectionController.
    set_parent_owned(false);
  }

  virtual ~SelectionHandleView() {
    widget_->Close();
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    Circle circle = {kSelectionHandleRadius, gfx::Point(kSelectionHandleRadius,
                     kSelectionHandleRadius + kSelectionHandleCursorHeight),
                     kSelectionHandleColor};
    PaintCircle(circle, canvas);
    canvas->DrawLineInt(kSelectionHandleColor, kSelectionHandleRadius, 0,
                        kSelectionHandleRadius, kSelectionHandleCursorHeight);
  }

  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE {
    controller_->dragging_handle_ = this;
    return true;
  }

  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE {
    controller_->SelectionHandleDragged(event.location());
    return true;
  }

  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE {
    controller_->dragging_handle_ = NULL;
  }

  virtual void OnMouseCaptureLost() OVERRIDE {
    controller_->dragging_handle_ = NULL;
  }

  virtual void SetVisible(bool visible) OVERRIDE {
    // We simply show/hide the container widget.
    if (visible != widget_->IsVisible()) {
      if (visible)
        widget_->Show();
      else
        widget_->Hide();
    }
    View::SetVisible(visible);
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(2 * kSelectionHandleRadius,
                     2 * kSelectionHandleRadius + kSelectionHandleCursorHeight);
  }

  void SetScreenPosition(const gfx::Point& position) {
    gfx::Rect widget_bounds(position.x() - kSelectionHandleRadius, position.y(),
        2 * kSelectionHandleRadius,
        2 * kSelectionHandleRadius + kSelectionHandleCursorHeight);
    widget_->SetBounds(widget_bounds);
  }

  gfx::Point GetScreenPosition() {
    return widget_->GetClientAreaScreenBounds().origin();
  }

 private:
  scoped_ptr<Widget> widget_;
  TouchSelectionControllerImpl* controller_;

  DISALLOW_COPY_AND_ASSIGN(SelectionHandleView);
};

TouchSelectionControllerImpl::TouchSelectionControllerImpl(
    TouchSelectionClientView* client_view)
    : client_view_(client_view),
      selection_handle_1_(new SelectionHandleView(this)),
      selection_handle_2_(new SelectionHandleView(this)),
      dragging_handle_(NULL) {
}

TouchSelectionControllerImpl::~TouchSelectionControllerImpl() {
}

void TouchSelectionControllerImpl::SelectionChanged(const gfx::Point& p1,
                                                    const gfx::Point& p2) {
  gfx::Point screen_pos_1(p1);
  View::ConvertPointToScreen(client_view_, &screen_pos_1);
  gfx::Point screen_pos_2(p2);
  View::ConvertPointToScreen(client_view_, &screen_pos_2);

  if (dragging_handle_) {
    // We need to reposition only the selection handle that is being dragged.
    // The other handle stays the same. Also, the selection handle being dragged
    // will always be at the end of selection, while the other handle will be at
    // the start.
    dragging_handle_->SetScreenPosition(screen_pos_2);
  } else {
    // Check if there is any selection at all.
    if (screen_pos_1 == screen_pos_2) {
      selection_handle_1_->SetVisible(false);
      selection_handle_2_->SetVisible(false);
      return;
    }

    if (client_view_->bounds().Contains(p1)) {
      selection_handle_1_->SetScreenPosition(screen_pos_1);
      selection_handle_1_->SetVisible(true);
    } else {
      selection_handle_1_->SetVisible(false);
    }

    if (client_view_->bounds().Contains(p2)) {
      selection_handle_2_->SetScreenPosition(screen_pos_2);
      selection_handle_2_->SetVisible(true);
    } else {
      selection_handle_2_->SetVisible(false);
    }
  }
}

void TouchSelectionControllerImpl::ClientViewLostFocus() {
  selection_handle_1_->SetVisible(false);
  selection_handle_2_->SetVisible(false);
}

void TouchSelectionControllerImpl::SelectionHandleDragged(
    const gfx::Point& drag_pos) {
  if (client_view_->GetWidget()) {
    DCHECK(dragging_handle_);
    // Find the stationary selection handle.
    SelectionHandleView* fixed_handle = selection_handle_1_.get();
    if (fixed_handle == dragging_handle_)
      fixed_handle = selection_handle_2_.get();

    // Find selection end points in client_view's coordinate system.
    gfx::Point p1(drag_pos.x() + kSelectionHandleRadius, drag_pos.y());
    ConvertPointToClientView(dragging_handle_, &p1);

    gfx::Point p2(kSelectionHandleRadius, 0);
    ConvertPointToClientView(fixed_handle, &p2);

    // Instruct client_view to select the region between p1 and p2. The position
    // of |fixed_handle| is the start and that of |dragging_handle| is the end
    // of selection.
    client_view_->SelectRect(p2, p1);
  }
}

void TouchSelectionControllerImpl::ConvertPointToClientView(
    SelectionHandleView* source, gfx::Point* point) {
  View::ConvertPointToScreen(source, point);
  gfx::Rect r = client_view_->GetWidget()->GetClientAreaScreenBounds();
  point->SetPoint(point->x() - r.x(), point->y() - r.y());
  View::ConvertPointFromWidget(client_view_, point);
}

gfx::Point TouchSelectionControllerImpl::GetSelectionHandle1Position() {
  return selection_handle_1_->GetScreenPosition();
}

gfx::Point TouchSelectionControllerImpl::GetSelectionHandle2Position() {
  return selection_handle_2_->GetScreenPosition();
}

bool TouchSelectionControllerImpl::IsSelectionHandle1Visible() {
  return selection_handle_1_->IsVisible();
}

bool TouchSelectionControllerImpl::IsSelectionHandle2Visible() {
  return selection_handle_2_->IsVisible();
}

TouchSelectionController* TouchSelectionController::create(
    TouchSelectionClientView* client_view) {
  return new TouchSelectionControllerImpl(client_view);
}

}  // namespace views
