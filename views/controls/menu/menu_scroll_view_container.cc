// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_scroll_view_container.h"

#if defined(OS_WIN)
#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>
#endif

#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"
#include "views/border.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"

#if defined(OS_WIN)
#include "ui/gfx/native_theme_win.h"
#endif

#if defined(OS_WIN)
using gfx::NativeTheme;
#endif

// Height of the scroll arrow.
// This goes up to 4 with large fonts, but this is close enough for now.
static const int scroll_arrow_height = 3;

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
    return gfx::Size(MenuConfig::instance().scroll_arrow_height * 2 - 1,
                     pref_height_);
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
    return ui::DragDropTypes::DRAG_NONE;
  }

  virtual void OnDragExited() {
    DCHECK(host_->GetMenuItem()->GetMenuController());
    host_->GetMenuItem()->GetMenuController()->OnDragExitedScrollButton(host_);
  }

  virtual int OnPerformDrop(const DropTargetEvent& event) {
    return ui::DragDropTypes::DRAG_NONE;
  }

  virtual void OnPaint(gfx::Canvas* canvas) {
    const MenuConfig& config = MenuConfig::instance();

#if defined(OS_WIN)
    HDC dc = canvas->BeginPlatformPaint();

    // The background.
    RECT item_bounds = { 0, 0, width(), height() };
    NativeTheme::instance()->PaintMenuItemBackground(
        NativeTheme::MENU, dc, MENU_POPUPITEM, MPI_NORMAL, false,
        &item_bounds);
    canvas->EndPlatformPaint();

    SkColor arrow_color = color_utils::GetSysSkColor(COLOR_MENUTEXT);
#else
    SkColor arrow_color = SK_ColorBLACK;
#endif

    // Then the arrow.
    int x = width() / 2;
    int y = (height() - config.scroll_arrow_height) / 2;
    int delta_y = 1;
    if (!is_up_) {
      delta_y = -1;
      y += config.scroll_arrow_height;
    }
    for (int i = 0; i < config.scroll_arrow_height; ++i, --x, y += delta_y)
      canvas->FillRectInt(arrow_color, x, y, (i * 2) + 1, 1);
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

  virtual void ScrollRectToVisible(const gfx::Rect& rect) {
    // NOTE: this assumes we only want to scroll in the y direction.

    // Convert rect.y() to view's coordinates and make sure we don't show past
    // the bottom of the view.
    View* child = GetContents();
    child->SetY(-std::max(0, std::min(
        child->GetPreferredSize().height() - this->height(),
        rect.y() - child->y())));
  }

  // Returns the contents, which is the SubmenuView.
  View* GetContents() {
    return GetChildViewAt(0);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuScrollView);
};

// MenuScrollViewContainer ----------------------------------------------------

MenuScrollViewContainer::MenuScrollViewContainer(SubmenuView* content_view)
    : content_view_(content_view) {
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

void MenuScrollViewContainer::OnPaintBackground(gfx::Canvas* canvas) {
  if (background()) {
    View::OnPaintBackground(canvas);
    return;
  }

#if defined(OS_WIN)
  HDC dc = canvas->BeginPlatformPaint();
  RECT bounds = {0, 0, width(), height()};
  NativeTheme::instance()->PaintMenuBackground(
      NativeTheme::MENU, dc, MENU_POPUPBACKGROUND, 0, &bounds);
  canvas->EndPlatformPaint();
#elif defined(OS_CHROMEOS)
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
  canvas->AsCanvasSkia()->drawColor(SkColorSetRGB(210, 225, 246),
                                    SkXfermode::kSrc_Mode);
#endif
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

gfx::Size MenuScrollViewContainer::GetPreferredSize() {
  gfx::Size prefsize = scroll_view_->GetContents()->GetPreferredSize();
  gfx::Insets insets = GetInsets();
  prefsize.Enlarge(insets.width(), insets.height());
  return prefsize;
}

void MenuScrollViewContainer::GetAccessibleState(
    ui::AccessibleViewState* state) {
  // Get the name from the submenu view.
  content_view_->GetAccessibleState(state);

  // Now change the role.
  state->role = ui::AccessibilityTypes::ROLE_MENUBAR;
  // Some AT (like NVDA) will not process focus events on menu item children
  // unless a parent claims to be focused.
  state->state = ui::AccessibilityTypes::STATE_FOCUSED;
}

void MenuScrollViewContainer::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  gfx::Size content_pref = scroll_view_->GetContents()->GetPreferredSize();
  scroll_up_button_->SetVisible(content_pref.height() > height());
  scroll_down_button_->SetVisible(content_pref.height() > height());
  Layout();
}

}  // namespace views
