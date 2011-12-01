// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_controller_impl.h"

#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// Constants defining the visual attributes of selection handles
const int kSelectionHandleRadius = 10;
const int kSelectionHandleCursorHeight = 10;
const int kSelectionHandleAlpha = 0x7F;
const SkColor kSelectionHandleColor =
    SkColorSetA(SK_ColorBLUE, kSelectionHandleAlpha);

// The minimum selection size to trigger selection controller.
const int kMinSelectionSize = 4;

const int kContextMenuCommands[] = {IDS_APP_CUT,
                                    IDS_APP_COPY,
// TODO(varunjain): PASTE is acting funny due to some gtk clipboard issue.
// Uncomment the following when that is fixed.
//                                    IDS_APP_PASTE,
                                    IDS_APP_DELETE,
                                    IDS_APP_SELECT_ALL};
const int kContextMenuPadding = 2;
const int kContextMenuTimoutMs = 1000;
const int kContextMenuVerticalOffset = 25;

// Convenience struct to represent a circle shape.
struct Circle {
  int radius;
  gfx::Point center;
  SkColor color;
};

// Creates a widget to host SelectionHandleView.
views::Widget* CreateTouchSelectionPopupWidget() {
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
  canvas->GetSkCanvas()->drawPath(path, paint);
}

// The points may not match exactly, since the selection range computation may
// introduce some floating point errors. So check for a minimum size to decide
// whether or not there is any selection.
bool IsEmptySelection(const gfx::Point& p1, const gfx::Point& p2) {
  int delta_x = p2.x() - p1.x();
  int delta_y = p2.y() - p1.y();
  return (abs(delta_x) < kMinSelectionSize && abs(delta_y) < kMinSelectionSize);
}

}  // namespace

namespace views {

// A View that displays the text selection handle.
class TouchSelectionControllerImpl::SelectionHandleView : public View {
 public:
  SelectionHandleView(TouchSelectionControllerImpl* controller)
      : controller_(controller) {
    widget_.reset(CreateTouchSelectionPopupWidget());
    widget_->SetContentsView(this);
    widget_->SetAlwaysOnTop(true);

    // We are owned by the TouchSelectionController.
    set_parent_owned(false);
  }

  virtual ~SelectionHandleView() {
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

class ContextMenuButtonBackground : public Background {
 public:
  ContextMenuButtonBackground() {}

  virtual void Paint(gfx::Canvas* canvas, View* view) const OVERRIDE {
    CustomButton::ButtonState state = static_cast<CustomButton*>(view)->state();
    SkColor background_color, border_color;
    if (state == CustomButton::BS_NORMAL) {
      background_color = SkColorSetARGB(102, 255, 255, 255);
      border_color = SkColorSetARGB(36, 0, 0, 0);
    } else {
      background_color = SkColorSetARGB(13, 0, 0, 0);
      border_color = SkColorSetARGB(72, 0, 0, 0);
    }
    int w = view->width();
    int h = view->height();
    canvas->FillRect(background_color, gfx::Rect(1, 1, w - 2, h - 2));
    canvas->FillRect(border_color, gfx::Rect(2, 0, w - 4, 1));
    canvas->FillRect(border_color, gfx::Rect(1, 1, 1, 1));
    canvas->FillRect(border_color, gfx::Rect(0, 2, 1, h - 4));
    canvas->FillRect(border_color, gfx::Rect(1, h - 2, 1, 1));
    canvas->FillRect(border_color, gfx::Rect(2, h - 1, w - 4, 1));
    canvas->FillRect(border_color, gfx::Rect(w - 2, 1, 1, 1));
    canvas->FillRect(border_color, gfx::Rect(w - 1, 2, 1, h - 4));
    canvas->FillRect(border_color, gfx::Rect(w - 2, h - 2, 1, 1));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextMenuButtonBackground);
};

// A View that displays the touch context menu.
class TouchSelectionControllerImpl::TouchContextMenuView
    : public ButtonListener,
      public View {
 public:
  TouchContextMenuView(TouchSelectionControllerImpl* controller)
      : controller_(controller) {
    widget_.reset(CreateTouchSelectionPopupWidget());
    widget_->SetContentsView(this);
    widget_->SetAlwaysOnTop(true);

    // We are owned by the TouchSelectionController.
    set_parent_owned(false);
    SetLayoutManager(new BoxLayout(BoxLayout::kHorizontal, kContextMenuPadding,
        kContextMenuPadding, kContextMenuPadding));
  }

  virtual ~TouchContextMenuView() {
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

  void SetScreenPosition(const gfx::Point& position) {
    RefreshButtonsAndSetWidgetPosition(position);
  }

  gfx::Point GetScreenPosition() {
    return widget_->GetClientAreaScreenBounds().origin();
  }

  void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    // TODO(varunjain): the following color scheme is copied from
    // menu_scroll_view_container.cc. Figure out how to consolidate the two
    // pieces of code.
#if defined(OS_CHROMEOS)
    static const SkColor kGradientColors[2] = {
        SK_ColorWHITE,
        SkColorSetRGB(0xF0, 0xF0, 0xF0)
    };

    static const SkScalar kGradientPoints[2] = {
        SkIntToScalar(0),
        SkIntToScalar(1)
    };

    SkPoint points[2];
    points[0].set(SkIntToScalar(0), SkIntToScalar(0));
    points[1].set(SkIntToScalar(0), SkIntToScalar(height()));

    SkShader* shader = SkGradientShader::CreateLinear(points,
        kGradientColors, kGradientPoints, arraysize(kGradientPoints),
        SkShader::kRepeat_TileMode);
    DCHECK(shader);

    SkPaint paint;
    paint.setShader(shader);
    shader->unref();

    paint.setStyle(SkPaint::kFill_Style);
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);

    canvas->DrawRectInt(0, 0, width(), height(), paint);
#else
    // This is the same as COLOR_TOOLBAR.
    canvas->GetSkCanvas()->drawColor(SkColorSetRGB(210, 225, 246),
                                     SkXfermode::kSrc_Mode);
#endif
  }

  // ButtonListener
  virtual void ButtonPressed(Button* sender, const views::Event& event) {
    controller_->ExecuteCommand(sender->tag());
  }

 private:
  // Queries the client view for what elements to show in the menu and sizes
  // the menu appropriately.
  void RefreshButtonsAndSetWidgetPosition(const gfx::Point& position) {
    RemoveAllChildViews(true);
    int total_width = 0;
    int height = 0;
    for (size_t i = 0; i < arraysize(kContextMenuCommands); i++) {
      int command_id = kContextMenuCommands[i];
      if (controller_->IsCommandIdEnabled(command_id)) {
        TextButton* button = new TextButton(
            this, l10n_util::GetStringUTF16(command_id));
        button->set_focusable(true);
        button->set_request_focus_on_press(false);
        button->set_prefix_type(TextButton::PREFIX_HIDE);
        button->SetEnabledColor(MenuConfig::instance().text_color);
        button->set_background(new ContextMenuButtonBackground());
        button->set_alignment(TextButton::ALIGN_CENTER);
        button->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
            ui::ResourceBundle::LargeFont));
        button->set_tag(command_id);
        AddChildView(button);
        gfx::Size button_size = button->GetPreferredSize();
        total_width += button_size.width() + kContextMenuPadding;
        if (height < button_size.height())
          height = button_size.height();
      }
    }
    gfx::Rect widget_bounds(position.x() - total_width / 2,
                            position.y() - height,
                            total_width,
                            height);
    gfx::Rect monitor_bounds =
        gfx::Screen::GetMonitorAreaNearestPoint(position);
    widget_->SetBounds(widget_bounds.AdjustToFit(monitor_bounds));
    Layout();
  }

  scoped_ptr<Widget> widget_;
  TouchSelectionControllerImpl* controller_;

  DISALLOW_COPY_AND_ASSIGN(TouchContextMenuView);
};

TouchSelectionControllerImpl::TouchSelectionControllerImpl(
    TouchSelectionClientView* client_view)
    : client_view_(client_view),
      selection_handle_1_(new SelectionHandleView(this)),
      selection_handle_2_(new SelectionHandleView(this)),
      context_menu_(new TouchContextMenuView(this)),
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
    UpdateContextMenu(p1, p2);

    // Check if there is any selection at all.
    if (IsEmptySelection(screen_pos_2, screen_pos_1)) {
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
  HideContextMenu();
}

void TouchSelectionControllerImpl::SelectionHandleDragged(
    const gfx::Point& drag_pos) {
  // We do not want to show the context menu while dragging.
  HideContextMenu();
  context_menu_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kContextMenuTimoutMs),
      this,
      &TouchSelectionControllerImpl::ContextMenuTimerFired);

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

bool TouchSelectionControllerImpl::IsCommandIdEnabled(int command_id) const {
  return client_view_->IsCommandIdEnabled(command_id);
}

void TouchSelectionControllerImpl::ExecuteCommand(int command_id) {
  HideContextMenu();
  client_view_->ExecuteCommand(command_id);
}

void TouchSelectionControllerImpl::ContextMenuTimerFired() {
  // Get selection end points in client_view's space.
  gfx::Point p1(kSelectionHandleRadius, 0);
  ConvertPointToClientView(selection_handle_1_.get(), &p1);
  gfx::Point p2(kSelectionHandleRadius, 0);
  ConvertPointToClientView(selection_handle_2_.get(), &p2);

  // if selection is completely inside the view, we display the context menu
  // in the middle of the end points on the top. Else, we show the menu on the
  // top border of the view in the center.
  gfx::Point menu_pos;
  if (client_view_->bounds().Contains(p1) &&
      client_view_->bounds().Contains(p2)) {
    menu_pos.set_x((p1.x() + p2.x()) / 2);
    menu_pos.set_y(std::min(p1.y(), p2.y()) - kContextMenuVerticalOffset);
  } else {
    menu_pos.set_x(client_view_->x() + client_view_->width() / 2);
    menu_pos.set_y(client_view_->y());
  }

  View::ConvertPointToScreen(client_view_, &menu_pos);

  context_menu_->SetScreenPosition(menu_pos);
  context_menu_->SetVisible(true);
}

void TouchSelectionControllerImpl::UpdateContextMenu(const gfx::Point& p1,
                                                     const gfx::Point& p2) {
  // Hide context menu to be shown when the timer fires.
  HideContextMenu();

  // If there is selection, we restart the context menu timer.
  if (!IsEmptySelection(p1, p2)) {
    context_menu_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kContextMenuTimoutMs),
        this,
        &TouchSelectionControllerImpl::ContextMenuTimerFired);
  }
}

void TouchSelectionControllerImpl::HideContextMenu() {
  context_menu_->SetVisible(false);
  context_menu_timer_.Stop();
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
