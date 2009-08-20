// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/submenu_view.h"

#include "app/gfx/canvas.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_scroll_view_container.h"
#include "views/widget/root_view.h"

#if defined(OS_WIN)
#include "base/win_util.h"
#include "views/widget/widget_win.h"
#endif

// Height of the drop indicator. This should be an even number.
static const int kDropIndicatorHeight = 2;

// Color of the drop indicator.
static const SkColor kDropIndicatorColor = SK_ColorBLACK;

namespace views {

// MenuHostRootView ----------------------------------------------------------

// MenuHostRootView is the RootView of the window showing the menu.
// SubmenuView's scroll view is added as a child of MenuHostRootView.
// MenuHostRootView forwards relevant events to the MenuController.
//
// As all the menu items are owned by the root menu item, care must be taken
// such that when MenuHostRootView is deleted it doesn't delete the menu items.
class MenuHostRootView : public RootView {
 public:
  explicit MenuHostRootView(Widget* widget,
                            SubmenuView* submenu)
      : RootView(widget),
        submenu_(submenu),
        forward_drag_to_menu_controller_(true),
        suspend_events_(false) {
#ifdef DEBUG_MENU
  DLOG(INFO) << " new MenuHostRootView " << this;
#endif
  }

  virtual bool OnMousePressed(const MouseEvent& event) {
    if (suspend_events_)
      return true;

    forward_drag_to_menu_controller_ =
        ((event.x() < 0 || event.y() < 0 || event.x() >= width() ||
          event.y() >= height()) ||
         !RootView::OnMousePressed(event));
    if (forward_drag_to_menu_controller_)
      GetMenuController()->OnMousePressed(submenu_, event);
    return true;
  }

  virtual bool OnMouseDragged(const MouseEvent& event) {
    if (suspend_events_)
      return true;

    if (forward_drag_to_menu_controller_) {
#ifdef DEBUG_MENU
      DLOG(INFO) << " MenuHostRootView::OnMouseDragged source=" << submenu_;
#endif
      GetMenuController()->OnMouseDragged(submenu_, event);
      return true;
    }
    return RootView::OnMouseDragged(event);
  }

  virtual void OnMouseReleased(const MouseEvent& event, bool canceled) {
    if (suspend_events_)
      return;

    RootView::OnMouseReleased(event, canceled);
    if (forward_drag_to_menu_controller_) {
      forward_drag_to_menu_controller_ = false;
      if (canceled) {
        GetMenuController()->Cancel(true);
      } else {
        GetMenuController()->OnMouseReleased(submenu_, event);
      }
    }
  }

  virtual void OnMouseMoved(const MouseEvent& event) {
    if (suspend_events_)
      return;

    RootView::OnMouseMoved(event);
    GetMenuController()->OnMouseMoved(submenu_, event);
  }

  virtual void ProcessOnMouseExited() {
    if (suspend_events_)
      return;

    RootView::ProcessOnMouseExited();
  }

  virtual bool ProcessMouseWheelEvent(const MouseWheelEvent& e) {
    // RootView::ProcessMouseWheelEvent forwards to the focused view. We don't
    // have a focused view, so we need to override this then forward to
    // the menu.
    return submenu_->OnMouseWheel(e);
  }

  void SuspendEvents() {
    suspend_events_ = true;
  }

 private:
  MenuController* GetMenuController() {
    return submenu_->GetMenuItem()->GetMenuController();
  }

  // The SubmenuView we contain.
  SubmenuView* submenu_;

  // Whether mouse dragged/released should be forwarded to the MenuController.
  bool forward_drag_to_menu_controller_;

  // Whether events are suspended. If true, no events are forwarded to the
  // MenuController.
  bool suspend_events_;

  DISALLOW_COPY_AND_ASSIGN(MenuHostRootView);
};

// MenuHost ------------------------------------------------------------------

// MenuHost is the window responsible for showing a single menu.
//
// Similar to MenuHostRootView, care must be taken such that when MenuHost is
// deleted, it doesn't delete the menu items. MenuHost is closed via a
// DelayedClosed, which avoids timing issues with deleting the window while
// capture or events are directed at it.

class MenuHost : public WidgetWin {
 public:
  explicit MenuHost(SubmenuView* submenu)
      : closed_(false),
        submenu_(submenu),
        owns_capture_(false) {
    set_window_style(WS_POPUP);
    set_initial_class_style(
        (win_util::GetWinVersion() < win_util::WINVERSION_XP) ?
        0 : CS_DROPSHADOW);
    is_mouse_down_ =
        ((GetKeyState(VK_LBUTTON) & 0x80) ||
         (GetKeyState(VK_RBUTTON) & 0x80) ||
         (GetKeyState(VK_MBUTTON) & 0x80) ||
         (GetKeyState(VK_XBUTTON1) & 0x80) ||
         (GetKeyState(VK_XBUTTON2) & 0x80));
    // Mouse clicks shouldn't give us focus.
    set_window_ex_style(WS_EX_TOPMOST | WS_EX_NOACTIVATE);
  }

  void Init(HWND parent,
            const gfx::Rect& bounds,
            View* contents_view,
            bool do_capture) {
    WidgetWin::Init(parent, bounds);
    SetContentsView(contents_view);
    // We don't want to take focus away from the hosting window.
    ShowWindow(SW_SHOWNA);
    owns_capture_ = do_capture;
    if (do_capture) {
      SetCapture();
      has_capture_ = true;
#ifdef DEBUG_MENU
      DLOG(INFO) << "Doing capture";
#endif
    }
  }

  virtual void Hide() {
    if (closed_) {
      // We're already closed, nothing to do.
      // This is invoked twice if the first time just hid us, and the second
      // time deleted Closed (deleted) us.
      return;
    }
    // The menus are freed separately, and possibly before the window is closed,
    // remove them so that View doesn't try to access deleted objects.
    static_cast<MenuHostRootView*>(GetRootView())->SuspendEvents();
    GetRootView()->RemoveAllChildViews(false);
    closed_ = true;
    ReleaseCapture();
    WidgetWin::Hide();
  }

  virtual void HideWindow() {
    // Make sure we release capture before hiding.
    ReleaseCapture();
    WidgetWin::Hide();
  }

  virtual void OnCaptureChanged(HWND hwnd) {
    WidgetWin::OnCaptureChanged(hwnd);
    owns_capture_ = false;
#ifdef DEBUG_MENU
    DLOG(INFO) << "Capture changed";
#endif
  }

  void ReleaseCapture() {
    if (owns_capture_) {
#ifdef DEBUG_MENU
      DLOG(INFO) << "released capture";
#endif
      owns_capture_ = false;
      ::ReleaseCapture();
    }
  }

 protected:
  // Overriden to create MenuHostRootView.
  virtual RootView* CreateRootView() {
    return new MenuHostRootView(this, submenu_);
  }

  virtual void OnCancelMode() {
    if (!closed_) {
#ifdef DEBUG_MENU
      DLOG(INFO) << "OnCanceMode, closing menu";
#endif
      submenu_->GetMenuItem()->GetMenuController()->Cancel(true);
    }
  }

  // Overriden to return false, we do NOT want to release capture on mouse
  // release.
  virtual bool ReleaseCaptureOnMouseReleased() {
    return false;
  }

 private:
  // If true, we've been closed.
  bool closed_;

  // If true, we own the capture and need to release it.
  bool owns_capture_;

  // The view we contain.
  SubmenuView* submenu_;

  DISALLOW_COPY_AND_ASSIGN(MenuHost);
};

// SubmenuView ---------------------------------------------------------------

// static
const int SubmenuView::kSubmenuBorderSize = 3;

SubmenuView::SubmenuView(MenuItemView* parent)
    : parent_menu_item_(parent),
      host_(NULL),
      drop_item_(NULL),
      drop_position_(MenuDelegate::DROP_NONE),
      scroll_view_container_(NULL) {
  DCHECK(parent);
  // We'll delete ourselves, otherwise the ScrollView would delete us on close.
  SetParentOwned(false);
}

SubmenuView::~SubmenuView() {
  // The menu may not have been closed yet (it will be hidden, but not
  // necessarily closed).
  Close();

  delete scroll_view_container_;
}

int SubmenuView::GetMenuItemCount() {
  int count = 0;
  for (int i = 0; i < GetChildViewCount(); ++i) {
    if (GetChildViewAt(i)->GetID() == MenuItemView::kMenuItemViewID)
      count++;
  }
  return count;
}

MenuItemView* SubmenuView::GetMenuItemAt(int index) {
  for (int i = 0, count = 0; i < GetChildViewCount(); ++i) {
    if (GetChildViewAt(i)->GetID() == MenuItemView::kMenuItemViewID &&
        count++ == index) {
      return static_cast<MenuItemView*>(GetChildViewAt(i));
    }
  }
  NOTREACHED();
  return NULL;
}

void SubmenuView::Layout() {
  // We're in a ScrollView, and need to set our width/height ourselves.
  View* parent = GetParent();
  if (!parent)
    return;
  SetBounds(x(), y(), parent->width(), GetPreferredSize().height());

  gfx::Insets insets = GetInsets();
  int x = insets.left();
  int y = insets.top();
  int menu_item_width = width() - insets.width();
  for (int i = 0; i < GetChildViewCount(); ++i) {
    View* child = GetChildViewAt(i);
    gfx::Size child_pref_size = child->GetPreferredSize();
    child->SetBounds(x, y, menu_item_width, child_pref_size.height());
    y += child_pref_size.height();
  }
}

gfx::Size SubmenuView::GetPreferredSize() {
  if (GetChildViewCount() == 0)
    return gfx::Size();

  int max_width = 0;
  int height = 0;
  for (int i = 0; i < GetChildViewCount(); ++i) {
    View* child = GetChildViewAt(i);
    gfx::Size child_pref_size = child->GetPreferredSize();
    max_width = std::max(max_width, child_pref_size.width());
    height += child_pref_size.height();
  }
  gfx::Insets insets = GetInsets();
  return gfx::Size(max_width + insets.width(), height + insets.height());
}

void SubmenuView::DidChangeBounds(const gfx::Rect& previous,
                                  const gfx::Rect& current) {
  SchedulePaint();
}

void SubmenuView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);

  if (drop_item_ && drop_position_ != MenuDelegate::DROP_ON)
    PaintDropIndicator(canvas, drop_item_, drop_position_);
}

// TODO(sky): need to add support for new dnd methods for Linux.

bool SubmenuView::CanDrop(const OSExchangeData& data) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->CanDrop(this, data);
}

void SubmenuView::OnDragEntered(const DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  GetMenuItem()->GetMenuController()->OnDragEntered(this, event);
}

int SubmenuView::OnDragUpdated(const DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->OnDragUpdated(this, event);
}

void SubmenuView::OnDragExited() {
  DCHECK(GetMenuItem()->GetMenuController());
  GetMenuItem()->GetMenuController()->OnDragExited(this);
}

int SubmenuView::OnPerformDrop(const DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->OnPerformDrop(this, event);
}

bool SubmenuView::OnMouseWheel(const MouseWheelEvent& e) {
  gfx::Rect vis_bounds = GetVisibleBounds();
  int menu_item_count = GetMenuItemCount();
  if (vis_bounds.height() == height() || !menu_item_count) {
    // All menu items are visible, nothing to scroll.
    return true;
  }

  // Find the index of the first menu item whose y-coordinate is >= visible
  // y-coordinate.
  int first_vis_index = -1;
  for (int i = 0; i < menu_item_count; ++i) {
    MenuItemView* menu_item = GetMenuItemAt(i);
    if (menu_item->y() == vis_bounds.y()) {
      first_vis_index = i;
      break;
    } else if (menu_item->y() > vis_bounds.y()) {
      first_vis_index = std::max(0, i - 1);
      break;
    }
  }
  if (first_vis_index == -1)
    return true;

  // If the first item isn't entirely visible, make it visible, otherwise make
  // the next/previous one entirely visible.
  int delta = abs(e.GetOffset() / WHEEL_DELTA);
  bool scroll_up = (e.GetOffset() > 0);
  while (delta-- > 0) {
    int scroll_amount = 0;
    if (scroll_up) {
      if (GetMenuItemAt(first_vis_index)->y() == vis_bounds.y()) {
        if (first_vis_index != 0) {
          scroll_amount = GetMenuItemAt(first_vis_index - 1)->y() -
                          vis_bounds.y();
          first_vis_index--;
        } else {
          break;
        }
      } else {
        scroll_amount = GetMenuItemAt(first_vis_index)->y() - vis_bounds.y();
      }
    } else {
      if (first_vis_index + 1 == GetMenuItemCount())
        break;
      scroll_amount = GetMenuItemAt(first_vis_index + 1)->y() -
                      vis_bounds.y();
      if (GetMenuItemAt(first_vis_index)->y() == vis_bounds.y())
        first_vis_index++;
    }
    ScrollRectToVisible(0, vis_bounds.y() + scroll_amount, vis_bounds.width(),
                        vis_bounds.height());
    vis_bounds = GetVisibleBounds();
  }

  return true;
}

bool SubmenuView::IsShowing() {
  return host_ && host_->IsVisible();
}

void SubmenuView::ShowAt(HWND parent,
                         const gfx::Rect& bounds,
                         bool do_capture) {
  if (host_) {
    host_->ShowWindow(SW_SHOWNA);
    return;
  }

  host_ = new MenuHost(this);
  // Force construction of the scroll view container.
  GetScrollViewContainer();
  // Make sure the first row is visible.
  ScrollRectToVisible(0, 0, 1, 1);
  host_->Init(parent, bounds, scroll_view_container_, do_capture);
}

void SubmenuView::Close() {
  if (host_) {
    host_->Close();
    host_ = NULL;
  }
}

void SubmenuView::Hide() {
  if (host_)
    host_->HideWindow();
}

void SubmenuView::ReleaseCapture() {
  host_->ReleaseCapture();
}

bool SubmenuView::SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
  return views::FocusManager::IsTabTraversalKeyEvent(e);
}

void SubmenuView::SetDropMenuItem(MenuItemView* item,
                                  MenuDelegate::DropPosition position) {
  if (drop_item_ == item && drop_position_ == position)
    return;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
  drop_item_ = item;
  drop_position_ = position;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
}

bool SubmenuView::GetShowSelection(MenuItemView* item) {
  if (drop_item_ == NULL)
    return true;
  // Something is being dropped on one of this menus items. Show the
  // selection if the drop is on the passed in item and the drop position is
  // ON.
  return (drop_item_ == item && drop_position_ == MenuDelegate::DROP_ON);
}

MenuScrollViewContainer* SubmenuView::GetScrollViewContainer() {
  if (!scroll_view_container_) {
    scroll_view_container_ = new MenuScrollViewContainer(this);
    // Otherwise MenuHost would delete us.
    scroll_view_container_->SetParentOwned(false);
  }
  return scroll_view_container_;
}

gfx::NativeView SubmenuView::native_view() const {
  return host_ ? host_->GetNativeView() : NULL;
}

void SubmenuView::PaintDropIndicator(gfx::Canvas* canvas,
                                     MenuItemView* item,
                                     MenuDelegate::DropPosition position) {
  if (position == MenuDelegate::DROP_NONE)
    return;

  gfx::Rect bounds = CalculateDropIndicatorBounds(item, position);
  canvas->FillRectInt(kDropIndicatorColor, bounds.x(), bounds.y(),
                      bounds.width(), bounds.height());
}

void SubmenuView::SchedulePaintForDropIndicator(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  if (item == NULL)
    return;

  if (position == MenuDelegate::DROP_ON) {
    item->SchedulePaint();
  } else if (position != MenuDelegate::DROP_NONE) {
    gfx::Rect bounds = CalculateDropIndicatorBounds(item, position);
    SchedulePaint(bounds.x(), bounds.y(), bounds.width(), bounds.height());
  }
}

gfx::Rect SubmenuView::CalculateDropIndicatorBounds(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  DCHECK(position != MenuDelegate::DROP_NONE);
  gfx::Rect item_bounds = item->bounds();
  switch (position) {
    case MenuDelegate::DROP_BEFORE:
      item_bounds.Offset(0, -kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    case MenuDelegate::DROP_AFTER:
      item_bounds.Offset(0, item_bounds.height() - kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    default:
      // Don't render anything for on.
      return gfx::Rect();
  }
}

}  // namespace views
