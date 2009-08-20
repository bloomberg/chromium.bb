// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_scroll_view_container.h"

#if defined(OS_WIN)
#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>
#endif

#include "app/gfx/canvas.h"
#include "app/gfx/color_utils.h"
#include "base/gfx/native_theme.h"
#include "views/border.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"

using gfx::NativeTheme;

// Height of the scroll arrow.
// This goes up to 4 with large fonts, but this is close enough for now.
static const int kScrollArrowHeight = 3;

namespace views {

namespace {

// MenuScrollButton ------------------------------------------------------------

// MenuScrollButton is used for the scroll buttons when not all menu items fit
// on screen. MenuScrollButton forwards appropriate events to the
// MenuController.

class MenuScrollButton : public View {
 public:
  explicit MenuScrollButton(SubmenuView* host, bool is_up)
      : host_(host),
        is_up_(is_up),
        // Make our height the same as that of other MenuItemViews.
        pref_height_(MenuItemView::pref_menu_height()) {
  }

  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(kScrollArrowHeight * 2 - 1, pref_height_);
  }

  virtual bool CanDrop(const OSExchangeData& data) {
    DCHECK(host_->GetMenuItem()->GetMenuController());
    return true;  // Always return true so that drop events are targeted to us.
  }

  virtual void OnDragEntered(const DropTargetEvent& event) {
    DCHECK(host_->GetMenuItem()->GetMenuController());
    host_->GetMenuItem()->GetMenuController()->OnDragEnteredScrollButton(
        host_, is_up_);
  }

  virtual int OnDragUpdated(const DropTargetEvent& event) {
    return DragDropTypes::DRAG_NONE;
  }

  virtual void OnDragExited() {
    DCHECK(host_->GetMenuItem()->GetMenuController());
    host_->GetMenuItem()->GetMenuController()->OnDragExitedScrollButton(host_);
  }

  virtual int OnPerformDrop(const DropTargetEvent& event) {
    return DragDropTypes::DRAG_NONE;
  }

  virtual void Paint(gfx::Canvas* canvas) {
    HDC dc = canvas->beginPlatformPaint();

    // The background.
    RECT item_bounds = { 0, 0, width(), height() };
    NativeTheme::instance()->PaintMenuItemBackground(
        NativeTheme::MENU, dc, MENU_POPUPITEM, MPI_NORMAL, false,
        &item_bounds);

    // Then the arrow.
    int x = width() / 2;
    int y = (height() - kScrollArrowHeight) / 2;
    int delta_y = 1;
    if (!is_up_) {
      delta_y = -1;
      y += kScrollArrowHeight;
    }
    SkColor arrow_color = color_utils::GetSysSkColor(COLOR_MENUTEXT);
    for (int i = 0; i < kScrollArrowHeight; ++i, --x, y += delta_y)
      canvas->FillRectInt(arrow_color, x, y, (i * 2) + 1, 1);

    canvas->endPlatformPaint();
  }

 private:
  // SubmenuView we were created for.
  SubmenuView* host_;

  // Direction of the button.
  bool is_up_;

  // Preferred height.
  int pref_height_;

  DISALLOW_COPY_AND_ASSIGN(MenuScrollButton);
};

}  // namespace

// MenuScrollView --------------------------------------------------------------

// MenuScrollView is a viewport for the SubmenuView. It's reason to exist is so
// that ScrollRectToVisible works.
//
// NOTE: It is possible to use ScrollView directly (after making it deal with
// null scrollbars), but clicking on a child of ScrollView forces the window to
// become active, which we don't want. As we really only need a fraction of
// what ScrollView does, so we use a one off variant.

class MenuScrollViewContainer::MenuScrollView : public View {
 public:
  explicit MenuScrollView(View* child) {
    AddChildView(child);
  }

  virtual void ScrollRectToVisible(int x, int y, int width, int height) {
    // NOTE: this assumes we only want to scroll in the y direction.

    View* child = GetContents();
    // Convert y to view's coordinates.
    y -= child->y();
    gfx::Size pref = child->GetPreferredSize();
    // Constrain y to make sure we don't show past the bottom of the view.
    y = std::max(0, std::min(pref.height() - this->height(), y));
    child->SetY(-y);
  }

  // Returns the contents, which is the SubmenuView.
  View* GetContents() {
    return GetChildViewAt(0);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuScrollView);
};

// MenuScrollViewContainer ----------------------------------------------------

MenuScrollViewContainer::MenuScrollViewContainer(SubmenuView* content_view) {
  scroll_up_button_ = new MenuScrollButton(content_view, true);
  scroll_down_button_ = new MenuScrollButton(content_view, false);
  AddChildView(scroll_up_button_);
  AddChildView(scroll_down_button_);

  scroll_view_ = new MenuScrollView(content_view);
  AddChildView(scroll_view_);

  set_border(Border::CreateEmptyBorder(
                 SubmenuView::kSubmenuBorderSize,
                 SubmenuView::kSubmenuBorderSize,
                 SubmenuView::kSubmenuBorderSize,
                 SubmenuView::kSubmenuBorderSize));
}

void MenuScrollViewContainer::Paint(gfx::Canvas* canvas) {
  HDC dc = canvas->beginPlatformPaint();
  RECT bounds = {0, 0, width(), height()};
  NativeTheme::instance()->PaintMenuBackground(
      NativeTheme::MENU, dc, MENU_POPUPBACKGROUND, 0, &bounds);
  canvas->endPlatformPaint();
}

void MenuScrollViewContainer::Layout() {
  gfx::Insets insets = GetInsets();
  int x = insets.left();
  int y = insets.top();
  int width = View::width() - insets.width();
  int content_height = height() - insets.height();
  if (!scroll_up_button_->IsVisible()) {
    scroll_view_->SetBounds(x, y, width, content_height);
    scroll_view_->Layout();
    return;
  }

  gfx::Size pref = scroll_up_button_->GetPreferredSize();
  scroll_up_button_->SetBounds(x, y, width, pref.height());
  content_height -= pref.height();

  const int scroll_view_y = y + pref.height();

  pref = scroll_down_button_->GetPreferredSize();
  scroll_down_button_->SetBounds(x, height() - pref.height() - insets.top(),
                                 width, pref.height());
  content_height -= pref.height();

  scroll_view_->SetBounds(x, scroll_view_y, width, content_height);
  scroll_view_->Layout();
}

void MenuScrollViewContainer::DidChangeBounds(const gfx::Rect& previous,
                                              const gfx::Rect& current) {
  gfx::Size content_pref = scroll_view_->GetContents()->GetPreferredSize();
  scroll_up_button_->SetVisible(content_pref.height() > height());
  scroll_down_button_->SetVisible(content_pref.height() > height());
}

gfx::Size MenuScrollViewContainer::GetPreferredSize() {
  gfx::Size prefsize = scroll_view_->GetContents()->GetPreferredSize();
  gfx::Insets insets = GetInsets();
  prefsize.Enlarge(insets.width(), insets.height());
  return prefsize;
}

}  // namespace views
