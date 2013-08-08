// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_controller_impl.h"

#include "base/time/time.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/corewm/shadow_types.h"
#include "ui/views/widget/widget.h"

namespace {

// Constants defining the visual attributes of selection handles
const int kSelectionHandleLineWidth = 1;
const SkColor kSelectionHandleLineColor =
    SkColorSetRGB(0x42, 0x81, 0xf4);

// When a handle is dragged, the drag position reported to the client view is
// offset vertically to represent the cursor position. This constant specifies
// the offset in  pixels above the "O" (see pic below). This is required because
// say if this is zero, that means the drag position we report is the point
// right above the "O" or the bottom most point of the cursor "|". In that case,
// a vertical movement of even one pixel will make the handle jump to the line
// below it. So when the user just starts dragging, the handle will jump to the
// next line if the user makes any vertical movement. It is correct but
// looks/feels weird. So we have this non-zero offset to prevent this jumping.
//
// Editing handle widget showing the difference between the position of the
// ET_GESTURE_SCROLL_UPDATE event and the drag position reported to the client:
//                                  _____
//                                 |  |<-|---- Drag position reported to client
//                              _  |  O  |
//          Vertical Padding __|   |   <-|---- ET_GESTURE_SCROLL_UPDATE position
//                             |_  |_____|<--- Editing handle widget
//
//                                 | |
//                                  T
//                          Horizontal Padding
//
const int kSelectionHandleVerticalDragOffset = 5;

// Padding around the selection handle defining the area that will be included
// in the touch target to make dragging the handle easier (see pic above).
const int kSelectionHandleHorizPadding = 10;
const int kSelectionHandleVertPadding = 20;

// The minimum selection size to trigger selection controller.
// TODO(varunjain): Figure out if this is really required and get rid of it if
// it isnt.
const int kMinSelectionSize = 1;

const int kContextMenuTimoutMs = 200;

// Creates a widget to host SelectionHandleView.
views::Widget* CreateTouchSelectionPopupWidget(
    gfx::NativeView context,
    views::WidgetDelegate* widget_delegate) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_TOOLTIP);
  params.can_activate = false;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = context;
  params.delegate = widget_delegate;
  widget->Init(params);
#if defined(USE_AURA)
  SetShadowType(widget->GetNativeView(), views::corewm::SHADOW_TYPE_NONE);
#endif
  return widget;
}

gfx::Image* GetHandleImage() {
  static gfx::Image* handle_image = NULL;
  if (!handle_image) {
    handle_image = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TEXT_SELECTION_HANDLE);
  }
  return handle_image;
}

gfx::Size GetHandleImageSize() {
  return GetHandleImage()->Size();
}

// The points may not match exactly, since the selection range computation may
// introduce some floating point errors. So check for a minimum size to decide
// whether or not there is any selection.
bool IsEmptySelection(const gfx::Point& p1, const gfx::Point& p2) {
  int delta_x = p2.x() - p1.x();
  int delta_y = p2.y() - p1.y();
  return (abs(delta_x) < kMinSelectionSize && abs(delta_y) < kMinSelectionSize);
}

// Cannot use gfx::UnionRect since it does not work for empty rects.
gfx::Rect Union(const gfx::Rect& r1, const gfx::Rect& r2) {
  int rx = std::min(r1.x(), r2.x());
  int ry = std::min(r1.y(), r2.y());
  int rr = std::max(r1.right(), r2.right());
  int rb = std::max(r1.bottom(), r2.bottom());

  return gfx::Rect(rx, ry, rr - rx, rb - ry);
}

// Convenience method to convert a |rect| from screen to the |client|'s
// coordinate system.
// Note that this is not quite correct because it does not take into account
// transforms such as rotation and scaling. This should be in TouchEditable.
// TODO(varunjain): Fix this.
gfx::Rect ConvertFromScreen(ui::TouchEditable* client, const gfx::Rect& rect) {
  gfx::Point origin = rect.origin();
  client->ConvertPointFromScreen(&origin);
  return gfx::Rect(origin, rect.size());
}

}  // namespace

namespace views {

// A View that displays the text selection handle.
class TouchSelectionControllerImpl::EditingHandleView
    : public views::WidgetDelegateView {
 public:
  explicit EditingHandleView(TouchSelectionControllerImpl* controller,
                             gfx::NativeView context)
      : controller_(controller),
        drag_offset_(0),
        draw_invisible_(false) {
    widget_.reset(CreateTouchSelectionPopupWidget(context, this));
    widget_->SetContentsView(this);
    widget_->SetAlwaysOnTop(true);

    // We are owned by the TouchSelectionController.
    set_owned_by_client();
  }

  virtual ~EditingHandleView() {
  }

  int cursor_height() const { return selection_rect_.height(); }

  // Current selection end point that this handle marks in screen coordinates.
  const gfx::Rect selection_rect() const { return selection_rect_; }

  // Overridden from views::WidgetDelegateView:
  virtual bool WidgetHasHitTestMask() const OVERRIDE {
    return true;
  }

  virtual void GetWidgetHitTestMask(gfx::Path* mask) const OVERRIDE {
    gfx::Size image_size = GetHandleImageSize();
    mask->addRect(SkIntToScalar(0), SkIntToScalar(cursor_height()),
        SkIntToScalar(image_size.width()) + 2 * kSelectionHandleHorizPadding,
        SkIntToScalar(cursor_height() + image_size.height() +
            kSelectionHandleVertPadding));
  }

  virtual void DeleteDelegate() OVERRIDE {
    // We are owned and deleted by TouchSelectionController.
  }

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    if (draw_invisible_)
      return;
    gfx::Size image_size = GetHandleImageSize();
    int cursor_pos_x = image_size.width() / 2 - kSelectionHandleLineWidth +
        kSelectionHandleHorizPadding;

    // Draw the cursor line.
    canvas->FillRect(
        gfx::Rect(cursor_pos_x, 0,
                  2 * kSelectionHandleLineWidth + 1, cursor_height()),
        kSelectionHandleLineColor);

    // Draw the handle image.
    canvas->DrawImageInt(*GetHandleImage()->ToImageSkia(),
        kSelectionHandleHorizPadding, cursor_height());
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    event->SetHandled();
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
        widget_->SetCapture(this);
        controller_->SetDraggingHandle(this);
        drag_offset_ = event->y() - cursor_height() -
            kSelectionHandleVerticalDragOffset;
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE: {
        gfx::Point drag_pos(event->location().x(),
            event->location().y() - drag_offset_);
        controller_->SelectionHandleDragged(drag_pos);
        break;
      }
      case ui::ET_GESTURE_SCROLL_END:
      case ui::ET_SCROLL_FLING_START:
        widget_->ReleaseCapture();
        controller_->SetDraggingHandle(NULL);
        break;
      default:
        break;
    }
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
    gfx::Size image_size = GetHandleImageSize();
    return gfx::Size(image_size.width() + 2 * kSelectionHandleHorizPadding,
        image_size.height() + cursor_height() + kSelectionHandleVertPadding);
  }

  bool IsWidgetVisible() const {
    return widget_->IsVisible();
  }

  void SetSelectionRectInScreen(const gfx::Rect& rect) {
    gfx::Size image_size = GetHandleImageSize();
    selection_rect_ = rect;
    gfx::Rect widget_bounds(
        rect.x() - image_size.width() / 2 - kSelectionHandleHorizPadding,
        rect.y(),
        image_size.width() + 2 * kSelectionHandleHorizPadding,
        rect.height() + image_size.height() + kSelectionHandleVertPadding);
    widget_->SetBounds(widget_bounds);
  }

  gfx::Point GetScreenPosition() {
    return widget_->GetClientAreaBoundsInScreen().origin();
  }

  void SetDrawInvisible(bool draw_invisible) {
    if (draw_invisible_ == draw_invisible)
      return;
    draw_invisible_ = draw_invisible;
    SchedulePaint();
  }

 private:
  scoped_ptr<Widget> widget_;
  TouchSelectionControllerImpl* controller_;
  gfx::Rect selection_rect_;
  int drag_offset_;

  // If set to true, the handle will not draw anything, hence providing an empty
  // widget. We need this because we may want to stop showing the handle while
  // it is being dragged. Since it is being dragged, we cannot destroy the
  // handle.
  bool draw_invisible_;

  DISALLOW_COPY_AND_ASSIGN(EditingHandleView);
};

TouchSelectionControllerImpl::TouchSelectionControllerImpl(
    ui::TouchEditable* client_view)
    : client_view_(client_view),
      client_widget_(NULL),
      selection_handle_1_(new EditingHandleView(this,
                          client_view->GetNativeView())),
      selection_handle_2_(new EditingHandleView(this,
                          client_view->GetNativeView())),
      cursor_handle_(new EditingHandleView(this,
                     client_view->GetNativeView())),
      context_menu_(NULL),
      dragging_handle_(NULL) {
  client_widget_ = Widget::GetTopLevelWidgetForNativeView(
      client_view_->GetNativeView());
  if (client_widget_)
    client_widget_->AddObserver(this);
}

TouchSelectionControllerImpl::~TouchSelectionControllerImpl() {
  HideContextMenu();
  if (client_widget_)
    client_widget_->RemoveObserver(this);
}

void TouchSelectionControllerImpl::SelectionChanged() {
  gfx::Rect r1, r2;
  client_view_->GetSelectionEndPoints(&r1, &r2);
  gfx::Point screen_pos_1(r1.origin());
  client_view_->ConvertPointToScreen(&screen_pos_1);
  gfx::Point screen_pos_2(r2.origin());
  client_view_->ConvertPointToScreen(&screen_pos_2);
  gfx::Rect screen_rect_1(screen_pos_1, r1.size());
  gfx::Rect screen_rect_2(screen_pos_2, r2.size());
  if (screen_rect_1 == selection_end_point_1 &&
      screen_rect_2 == selection_end_point_2)
    return;

  selection_end_point_1 = screen_rect_1;
  selection_end_point_2 = screen_rect_2;

  if (client_view_->DrawsHandles()) {
    UpdateContextMenu(r1.origin(), r2.origin());
    return;
  }
  if (dragging_handle_) {
    // We need to reposition only the selection handle that is being dragged.
    // The other handle stays the same. Also, the selection handle being dragged
    // will always be at the end of selection, while the other handle will be at
    // the start.
    dragging_handle_->SetSelectionRectInScreen(screen_rect_2);

    // Temporary fix for selection handle going outside a window. On a webpage,
    // the page should scroll if the selection handle is dragged outside the
    // window. That does not happen currently. So we just hide the handle for
    // now.
    // TODO(varunjain): Fix this: crbug.com/269003
    dragging_handle_->SetDrawInvisible(!client_view_->GetBounds().Contains(r2));

    if (dragging_handle_ != cursor_handle_.get()) {
      // The non-dragging-handle might have recently become visible.
      EditingHandleView* non_dragging_handle =
          dragging_handle_ == selection_handle_1_.get()?
              selection_handle_2_.get() : selection_handle_1_.get();
      non_dragging_handle->SetSelectionRectInScreen(screen_rect_1);
      non_dragging_handle->SetVisible(client_view_->GetBounds().Contains(r1));
    }
  } else {
    UpdateContextMenu(r1.origin(), r2.origin());

    // Check if there is any selection at all.
    if (IsEmptySelection(screen_pos_2, screen_pos_1)) {
      selection_handle_1_->SetVisible(false);
      selection_handle_2_->SetVisible(false);
      cursor_handle_->SetSelectionRectInScreen(screen_rect_1);
      cursor_handle_->SetVisible(true);
      return;
    }

    cursor_handle_->SetVisible(false);
    selection_handle_1_->SetSelectionRectInScreen(screen_rect_1);
    selection_handle_1_->SetVisible(client_view_->GetBounds().Contains(r1));

    selection_handle_2_->SetSelectionRectInScreen(screen_rect_2);
    selection_handle_2_->SetVisible(client_view_->GetBounds().Contains(r2));
  }
}

bool TouchSelectionControllerImpl::IsHandleDragInProgress() {
  return !!dragging_handle_;
}

void TouchSelectionControllerImpl::SetDraggingHandle(
    EditingHandleView* handle) {
  dragging_handle_ = handle;
  if (dragging_handle_)
    HideContextMenu();
  else
    StartContextMenuTimer();
}

void TouchSelectionControllerImpl::SelectionHandleDragged(
    const gfx::Point& drag_pos) {
  // We do not want to show the context menu while dragging.
  HideContextMenu();

  DCHECK(dragging_handle_);
  gfx::Point drag_pos_in_client = drag_pos;
  ConvertPointToClientView(dragging_handle_, &drag_pos_in_client);

  if (dragging_handle_ == cursor_handle_.get()) {
    client_view_->MoveCaretTo(drag_pos_in_client);
    return;
  }

  // Find the stationary selection handle.
  EditingHandleView* fixed_handle = selection_handle_1_.get();
  if (fixed_handle == dragging_handle_)
    fixed_handle = selection_handle_2_.get();

  // Find selection end points in client_view's coordinate system.
  gfx::Point p2 = fixed_handle->selection_rect().origin();
  p2.Offset(0, fixed_handle->cursor_height() / 2);
  client_view_->ConvertPointFromScreen(&p2);

  // Instruct client_view to select the region between p1 and p2. The position
  // of |fixed_handle| is the start and that of |dragging_handle| is the end
  // of selection.
  client_view_->SelectRect(p2, drag_pos_in_client);
}

void TouchSelectionControllerImpl::ConvertPointToClientView(
    EditingHandleView* source, gfx::Point* point) {
  View::ConvertPointToScreen(source, point);
  client_view_->ConvertPointFromScreen(point);
}

bool TouchSelectionControllerImpl::IsCommandIdEnabled(int command_id) const {
  return client_view_->IsCommandIdEnabled(command_id);
}

void TouchSelectionControllerImpl::ExecuteCommand(int command_id,
                                                  int event_flags) {
  HideContextMenu();
  client_view_->ExecuteCommand(command_id, event_flags);
}

void TouchSelectionControllerImpl::OpenContextMenu() {
  // Context menu should appear centered on top of the selected region.
  gfx::Point anchor(context_menu_->anchor_rect().CenterPoint().x(),
                    context_menu_->anchor_rect().y());
  HideContextMenu();
  client_view_->OpenContextMenu(anchor);
}

void TouchSelectionControllerImpl::OnMenuClosed(TouchEditingMenuView* menu) {
  if (menu == context_menu_)
    context_menu_ = NULL;
}

void TouchSelectionControllerImpl::OnWidgetClosing(Widget* widget) {
  DCHECK_EQ(client_widget_, widget);
  client_widget_ = NULL;
}

void TouchSelectionControllerImpl::OnWidgetBoundsChanged(
    Widget* widget,
    const gfx::Rect& new_bounds) {
  DCHECK_EQ(client_widget_, widget);
  HideContextMenu();
  SelectionChanged();
}

void TouchSelectionControllerImpl::ContextMenuTimerFired() {
  // Get selection end points in client_view's space.
  gfx::Rect end_rect_1_in_screen;
  gfx::Rect end_rect_2_in_screen;
  if (cursor_handle_->IsWidgetVisible()) {
    end_rect_1_in_screen = cursor_handle_->selection_rect();
    end_rect_2_in_screen = end_rect_1_in_screen;
  } else {
    end_rect_1_in_screen = selection_handle_1_->selection_rect();
    end_rect_2_in_screen = selection_handle_2_->selection_rect();
  }

  // Convert from screen to client.
  gfx::Rect end_rect_1(ConvertFromScreen(client_view_, end_rect_1_in_screen));
  gfx::Rect end_rect_2(ConvertFromScreen(client_view_, end_rect_2_in_screen));

  // if selection is completely inside the view, we display the context menu
  // in the middle of the end points on the top. Else, we show it above the
  // visible handle. If no handle is visible, we do not show the menu.
  gfx::Rect menu_anchor;
  gfx::Rect client_bounds = client_view_->GetBounds();
  if (client_bounds.Contains(end_rect_1) &&
      client_bounds.Contains(end_rect_2))
    menu_anchor = Union(end_rect_1_in_screen,end_rect_2_in_screen);
  else if (client_bounds.Contains(end_rect_1))
    menu_anchor = end_rect_1_in_screen;
  else if (client_bounds.Contains(end_rect_2))
    menu_anchor = end_rect_2_in_screen;
  else
    return;

  DCHECK(!context_menu_);
  context_menu_ = TouchEditingMenuView::Create(this, menu_anchor,
                                               client_view_->GetNativeView());
}

void TouchSelectionControllerImpl::StartContextMenuTimer() {
  if (context_menu_timer_.IsRunning())
    return;
  context_menu_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kContextMenuTimoutMs),
      this,
      &TouchSelectionControllerImpl::ContextMenuTimerFired);
}

void TouchSelectionControllerImpl::UpdateContextMenu(const gfx::Point& p1,
                                                     const gfx::Point& p2) {
  // Hide context menu to be shown when the timer fires.
  HideContextMenu();
  StartContextMenuTimer();
}

void TouchSelectionControllerImpl::HideContextMenu() {
  if (context_menu_)
    context_menu_->Close();
  context_menu_ = NULL;
  context_menu_timer_.Stop();
}

gfx::Point TouchSelectionControllerImpl::GetSelectionHandle1Position() {
  return selection_handle_1_->GetScreenPosition();
}

gfx::Point TouchSelectionControllerImpl::GetSelectionHandle2Position() {
  return selection_handle_2_->GetScreenPosition();
}

gfx::Point TouchSelectionControllerImpl::GetCursorHandlePosition() {
  return cursor_handle_->GetScreenPosition();
}

bool TouchSelectionControllerImpl::IsSelectionHandle1Visible() {
  return selection_handle_1_->visible();
}

bool TouchSelectionControllerImpl::IsSelectionHandle2Visible() {
  return selection_handle_2_->visible();
}

bool TouchSelectionControllerImpl::IsCursorHandleVisible() {
  return cursor_handle_->visible();
}

ViewsTouchSelectionControllerFactory::ViewsTouchSelectionControllerFactory() {
}

ui::TouchSelectionController* ViewsTouchSelectionControllerFactory::create(
    ui::TouchEditable* client_view) {
  if (switches::IsTouchEditingEnabled())
    return new views::TouchSelectionControllerImpl(client_view);
  return NULL;
}

}  // namespace views
