// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_item_view.h"

#if defined(OS_WIN)
#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>
#endif

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "grit/app_strings.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/submenu_view.h"

#if defined(OS_WIN)
#include "app/l10n_util_win.h"
#include "base/gfx/native_theme.h"
#include "base/win_util.h"
#endif

// Margins between the top of the item and the label.
static const int kItemTopMargin = 3;

// Margins between the bottom of the item and the label.
static const int kItemBottomMargin = 4;

// Margins used if the menu doesn't have icons.
static const int kItemNoIconTopMargin = 1;
static const int kItemNoIconBottomMargin = 3;

// Margins between the left of the item and the icon.
static const int kItemLeftMargin = 4;

// Padding between the label and submenu arrow.
static const int kLabelToArrowPadding = 10;

// Padding between the arrow and the edge.
static const int kArrowToEdgePadding = 5;

// Padding between the icon and label.
static const int kIconToLabelPadding = 8;

// Padding between the gutter and label.
static const int kGutterToLabel = 5;

// Size of the check. This comes from the OS.
static int check_width;
static int check_height;

// Size of the submenu arrow. This comes from the OS.
static int arrow_width;
static int arrow_height;

// Width of the gutter. Only used if render_gutter is true.
static int gutter_width;

// Margins between the right of the item and the label.
static int item_right_margin;

// X-coordinate of where the label starts.
static int label_start;

// Height of the separator.
static int separator_height;

// Whether or not the gutter should be rendered. The gutter is specific to
// Vista.
static bool render_gutter = false;

// Preferred height of menu items. Reset every time a menu is run.
static int pref_menu_h;

// Are mnemonics shown? This is updated before the menus are shown.
static bool show_mnemonics;

using gfx::NativeTheme;

namespace views {

namespace {

// Returns the font menus are to use.
gfx::Font GetMenuFont() {
#if defined(OS_WIN)
  NONCLIENTMETRICS metrics;
  win_util::GetNonClientMetrics(&metrics);

  l10n_util::AdjustUIFont(&(metrics.lfMenuFont));
  HFONT font = CreateFontIndirect(&metrics.lfMenuFont);
  DLOG_ASSERT(font);
  return gfx::Font::CreateFont(font);
#else
  return gfx::Font();
#endif
}

// Calculates all sizes that we can from the OS.
//
// This is invoked prior to Running a menu.
void UpdateMenuPartSizes(bool has_icons) {
#if defined(OS_WIN)
  HDC dc = GetDC(NULL);
  RECT bounds = { 0, 0, 200, 200 };
  SIZE check_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPCHECK, MC_CHECKMARKNORMAL, &bounds,
          TS_TRUE, &check_size) == S_OK) {
    check_width = check_size.cx;
    check_height = check_size.cy;
  } else {
    check_width = GetSystemMetrics(SM_CXMENUCHECK);
    check_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  SIZE arrow_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPSUBMENU, MSM_NORMAL, &bounds,
          TS_TRUE, &arrow_size) == S_OK) {
    arrow_width = arrow_size.cx;
    arrow_height = arrow_size.cy;
  } else {
    // Sadly I didn't see a specify metrics for this.
    arrow_width = GetSystemMetrics(SM_CXMENUCHECK);
    arrow_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  SIZE gutter_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPGUTTER, MSM_NORMAL, &bounds,
          TS_TRUE, &gutter_size) == S_OK) {
    gutter_width = gutter_size.cx;
    render_gutter = true;
  } else {
    gutter_width = 0;
    render_gutter = false;
  }

  SIZE separator_size;
  if (NativeTheme::instance()->GetThemePartSize(
          NativeTheme::MENU, dc, MENU_POPUPSEPARATOR, MSM_NORMAL, &bounds,
          TS_TRUE, &separator_size) == S_OK) {
    separator_height = separator_size.cy;
  } else {
    separator_height = GetSystemMetrics(SM_CYMENU) / 2;
  }

  item_right_margin = kLabelToArrowPadding + arrow_width + kArrowToEdgePadding;

  if (has_icons) {
    label_start = kItemLeftMargin + check_width + kIconToLabelPadding;
  } else {
    // If there are no icons don't pad by the icon to label padding. This
    // makes us look close to system menus.
    label_start = kItemLeftMargin + check_width;
  }
  if (render_gutter)
    label_start += gutter_width + kGutterToLabel;

  ReleaseDC(NULL, dc);

  MenuItemView menu_item(NULL);
  menu_item.SetTitle(L"blah");  // Text doesn't matter here.
  pref_menu_h = menu_item.GetPreferredSize().height();
#endif
}

// Convenience for scrolling the view such that the origin is visible.
static void ScrollToVisible(View* view) {
  view->ScrollRectToVisible(0, 0, view->width(), view->height());
}

// MenuSeparator ---------------------------------------------------------------

// Renders a separator.

class MenuSeparator : public View {
 public:
  MenuSeparator() {
  }

  void Paint(gfx::Canvas* canvas) {
    // The gutter is rendered before the background.
    int start_x = 0;
    int start_y = height() / 3;
    HDC dc = canvas->beginPlatformPaint();
    if (render_gutter) {
      // If render_gutter is true, we're on Vista and need to render the
      // gutter, then indent the separator from the gutter.
      RECT gutter_bounds = { label_start - kGutterToLabel - gutter_width, 0, 0,
                              height() };
      gutter_bounds.right = gutter_bounds.left + gutter_width;
      NativeTheme::instance()->PaintMenuGutter(dc, MENU_POPUPGUTTER, MPI_NORMAL,
                                               &gutter_bounds);
      start_x = gutter_bounds.left + gutter_width;
      start_y = 0;
    }
    RECT separator_bounds = { start_x, start_y, width(), height() };
    NativeTheme::instance()->PaintMenuSeparator(dc, MENU_POPUPSEPARATOR,
                                                MPI_NORMAL, &separator_bounds);
    canvas->endPlatformPaint();
  }

  gfx::Size GetPreferredSize() {
    return gfx::Size(10,  // Just in case we're the only item in a menu.
                     separator_height);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuSeparator);
};

// EmptyMenuMenuItem ---------------------------------------------------------

// EmptyMenuMenuItem is used when a menu has no menu items. EmptyMenuMenuItem
// is itself a MenuItemView, but it uses a different ID so that it isn't
// identified as a MenuItemView.

class EmptyMenuMenuItem : public MenuItemView {
 public:
  explicit EmptyMenuMenuItem(MenuItemView* parent) :
      MenuItemView(parent, 0, NORMAL) {
    SetTitle(l10n_util::GetString(IDS_APP_MENU_EMPTY_SUBMENU));
    // Set this so that we're not identified as a normal menu item.
    SetID(kEmptyMenuItemViewID);
    SetEnabled(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyMenuMenuItem);
};

}  // namespace

// MenuItemView ---------------------------------------------------------------

//  static
const int MenuItemView::kMenuItemViewID = 1001;

// static
const int MenuItemView::kEmptyMenuItemViewID =
    MenuItemView::kMenuItemViewID + 1;

// static
bool MenuItemView::allow_task_nesting_during_run_ = false;

MenuItemView::MenuItemView(MenuDelegate* delegate) {
  // NOTE: don't check the delegate for NULL, UpdateMenuPartSizes supplies a
  // NULL delegate.
  Init(NULL, 0, SUBMENU, delegate);
}

MenuItemView::~MenuItemView() {
  if (controller_) {
    // We're currently showing.

    // We can't delete ourselves while we're blocking.
    DCHECK(!controller_->IsBlockingRun());

    // Invoking Cancel is going to call us back and notify the delegate.
    // Notifying the delegate from the destructor can be problematic. To avoid
    // this the delegate is set to NULL.
    delegate_ = NULL;

    controller_->Cancel(true);
  }
  delete submenu_;
}

// static
int MenuItemView::pref_menu_height() {
  return pref_menu_h;
}

void MenuItemView::RunMenuAt(HWND parent,
                             const gfx::Rect& bounds,
                             AnchorPosition anchor,
                             bool has_mnemonics) {
  PrepareForRun(has_mnemonics);

  int mouse_event_flags;

  MenuController* controller = MenuController::GetActiveInstance();
  if (controller && !controller->IsBlockingRun()) {
    // A menu is already showing, but it isn't a blocking menu. Cancel it.
    // We can get here during drag and drop if the user right clicks on the
    // menu quickly after the drop.
    controller->Cancel(true);
    controller = NULL;
  }
  bool owns_controller = false;
  if (!controller) {
    // No menus are showing, show one.
    controller = new MenuController(true);
    MenuController::SetActiveInstance(controller);
    owns_controller = true;
  } else {
    // A menu is already showing, use the same controller.

    // Don't support blocking from within non-blocking.
    DCHECK(controller->IsBlockingRun());
  }

  controller_ = controller;

  // Run the loop.
  MenuItemView* result =
      controller->Run(parent, this, bounds, anchor, &mouse_event_flags);

  RemoveEmptyMenus();

  controller_ = NULL;

  if (owns_controller) {
    // We created the controller and need to delete it.
    if (MenuController::GetActiveInstance() == controller)
      MenuController::SetActiveInstance(NULL);
    delete controller;
  }
  // Make sure all the windows we created to show the menus have been
  // destroyed.
  DestroyAllMenuHosts();
  if (result && delegate_)
    delegate_->ExecuteCommand(result->GetCommand(), mouse_event_flags);
}

void MenuItemView::RunMenuForDropAt(HWND parent,
                                    const gfx::Rect& bounds,
                                    AnchorPosition anchor) {
  PrepareForRun(false);

  // If there is a menu, hide it so that only one menu is shown during dnd.
  MenuController* current_controller = MenuController::GetActiveInstance();
  if (current_controller) {
    current_controller->Cancel(true);
  }

  // Always create a new controller for non-blocking.
  controller_ = new MenuController(false);

  // Set the instance, that way it can be canceled by another menu.
  MenuController::SetActiveInstance(controller_);

  controller_->Run(parent, this, bounds, anchor, NULL);
}

void MenuItemView::Cancel() {
  if (controller_ && !canceled_) {
    canceled_ = true;
    controller_->Cancel(true);
  }
}

SubmenuView* MenuItemView::CreateSubmenu() {
  if (!submenu_)
    submenu_ = new SubmenuView(this);
  return submenu_;
}

void MenuItemView::SetSelected(bool selected) {
  selected_ = selected;
  SchedulePaint();
}

void MenuItemView::SetIcon(const SkBitmap& icon, int item_id) {
  MenuItemView* item = GetDescendantByID(item_id);
  DCHECK(item);
  item->SetIcon(icon);
}

void MenuItemView::SetIcon(const SkBitmap& icon) {
  icon_ = icon;
  SchedulePaint();
}

void MenuItemView::Paint(gfx::Canvas* canvas) {
  Paint(canvas, false);
}

gfx::Size MenuItemView::GetPreferredSize() {
  gfx::Font& font = GetRootMenuItem()->font_;
  return gfx::Size(
      font.GetStringWidth(title_) + label_start + item_right_margin,
      font.height() + GetBottomMargin() + GetTopMargin());
}

MenuController* MenuItemView::GetMenuController() {
  return GetRootMenuItem()->controller_;
}

MenuDelegate* MenuItemView::GetDelegate() {
  return GetRootMenuItem()->delegate_;
}

MenuItemView* MenuItemView::GetRootMenuItem() {
  MenuItemView* item = this;
  while (item) {
    MenuItemView* parent = item->GetParentMenuItem();
    if (!parent)
      return item;
    item = parent;
  }
  NOTREACHED();
  return NULL;
}

wchar_t MenuItemView::GetMnemonic() {
  if (!has_mnemonics_)
    return 0;

  const std::wstring& title = GetTitle();
  size_t index = 0;
  do {
    index = title.find('&', index);
    if (index != std::wstring::npos) {
      if (index + 1 != title.size() && title[index + 1] != '&')
        return title[index + 1];
      index++;
    }
  } while (index != std::wstring::npos);
  return 0;
}

MenuItemView::MenuItemView(MenuItemView* parent,
                           int command,
                           MenuItemView::Type type) {
  Init(parent, command, type, NULL);
}

void MenuItemView::Init(MenuItemView* parent,
                        int command,
                        MenuItemView::Type type,
                        MenuDelegate* delegate) {
  delegate_ = delegate;
  controller_ = NULL;
  canceled_ = false;
  parent_menu_item_ = parent;
  type_ = type;
  selected_ = false;
  command_ = command;
  submenu_ = NULL;
  // Assign our ID, this allows SubmenuItemView to find MenuItemViews.
  SetID(kMenuItemViewID);
  has_icons_ = false;

  MenuDelegate* root_delegate = GetDelegate();
  if (root_delegate)
    SetEnabled(root_delegate->IsCommandEnabled(command));
}

MenuItemView* MenuItemView::AppendMenuItemInternal(int item_id,
                                                   const std::wstring& label,
                                                   const SkBitmap& icon,
                                                   Type type) {
  if (!submenu_)
    CreateSubmenu();
  if (type == SEPARATOR) {
    submenu_->AddChildView(new MenuSeparator());
    return NULL;
  }
  MenuItemView* item = new MenuItemView(this, item_id, type);
  if (label.empty() && GetDelegate())
    item->SetTitle(GetDelegate()->GetLabel(item_id));
  else
    item->SetTitle(label);
  item->SetIcon(icon);
  if (type == SUBMENU)
    item->CreateSubmenu();
  submenu_->AddChildView(item);
  return item;
}

MenuItemView* MenuItemView::GetDescendantByID(int id) {
  if (GetCommand() == id)
    return this;
  if (!HasSubmenu())
    return NULL;
  for (int i = 0; i < GetSubmenu()->GetChildViewCount(); ++i) {
    View* child = GetSubmenu()->GetChildViewAt(i);
    if (child->GetID() == MenuItemView::kMenuItemViewID) {
      MenuItemView* result = static_cast<MenuItemView*>(child)->
          GetDescendantByID(id);
      if (result)
        return result;
    }
  }
  return NULL;
}

void MenuItemView::DropMenuClosed(bool notify_delegate) {
  DCHECK(controller_);
  DCHECK(!controller_->IsBlockingRun());
  if (MenuController::GetActiveInstance() == controller_)
    MenuController::SetActiveInstance(NULL);
  delete controller_;
  controller_ = NULL;

  RemoveEmptyMenus();

  if (notify_delegate && delegate_) {
    // Our delegate is null when invoked from the destructor.
    delegate_->DropMenuClosed(this);
  }
  // WARNING: its possible the delegate deleted us at this point.
}

void MenuItemView::PrepareForRun(bool has_mnemonics) {
  // Currently we only support showing the root.
  DCHECK(!parent_menu_item_);

  // Don't invoke run from within run on the same menu.
  DCHECK(!controller_);

  // Force us to have a submenu.
  CreateSubmenu();

  canceled_ = false;

  has_mnemonics_ = has_mnemonics;

  AddEmptyMenus();

  if (!MenuController::GetActiveInstance()) {
    // Only update the menu size if there are no menus showing, otherwise
    // things may shift around.
    UpdateMenuPartSizes(has_icons_);
  }

  font_ = GetMenuFont();

  BOOL show_cues;
  show_mnemonics =
      (SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &show_cues, 0) &&
       show_cues == TRUE);
}

int MenuItemView::GetDrawStringFlags() {
  int flags = 0;
  if (UILayoutIsRightToLeft())
    flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
  else
    flags |= gfx::Canvas::TEXT_ALIGN_LEFT;

  if (has_mnemonics_) {
    if (show_mnemonics)
      flags |= gfx::Canvas::SHOW_PREFIX;
    else
      flags |= gfx::Canvas::HIDE_PREFIX;
  }
  return flags;
}

void MenuItemView::AddEmptyMenus() {
  DCHECK(HasSubmenu());
  if (submenu_->GetChildViewCount() == 0) {
    submenu_->AddChildView(0, new EmptyMenuMenuItem(this));
  } else {
    for (int i = 0, item_count = submenu_->GetMenuItemCount(); i < item_count;
         ++i) {
      MenuItemView* child = submenu_->GetMenuItemAt(i);
      if (child->HasSubmenu())
        child->AddEmptyMenus();
    }
  }
}

void MenuItemView::RemoveEmptyMenus() {
  DCHECK(HasSubmenu());
  // Iterate backwards as we may end up removing views, which alters the child
  // view count.
  for (int i = submenu_->GetChildViewCount() - 1; i >= 0; --i) {
    View* child = submenu_->GetChildViewAt(i);
    if (child->GetID() == MenuItemView::kMenuItemViewID) {
      MenuItemView* menu_item = static_cast<MenuItemView*>(child);
      if (menu_item->HasSubmenu())
        menu_item->RemoveEmptyMenus();
    } else if (child->GetID() == EmptyMenuMenuItem::kEmptyMenuItemViewID) {
      submenu_->RemoveChildView(child);
    }
  }
}

void MenuItemView::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(MirroredLeftPointForRect(*rect));
}

void MenuItemView::Paint(gfx::Canvas* canvas, bool for_drag) {
  bool render_selection =
      (!for_drag && IsSelected() &&
       parent_menu_item_->GetSubmenu()->GetShowSelection(this));
  int state = render_selection ? MPI_HOT :
                                 (IsEnabled() ? MPI_NORMAL : MPI_DISABLED);
  HDC dc = canvas->beginPlatformPaint();

  // The gutter is rendered before the background.
  if (render_gutter && !for_drag) {
    gfx::Rect gutter_bounds(label_start - kGutterToLabel - gutter_width, 0,
                            gutter_width, height());
    AdjustBoundsForRTLUI(&gutter_bounds);
    RECT gutter_rect = gutter_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuGutter(dc, MENU_POPUPGUTTER, MPI_NORMAL,
                                             &gutter_rect);
  }

  // Render the background.
  if (!for_drag) {
    gfx::Rect item_bounds(0, 0, width(), height());
    AdjustBoundsForRTLUI(&item_bounds);
    RECT item_rect = item_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuItemBackground(
        NativeTheme::MENU, dc, MENU_POPUPITEM, state, render_selection,
        &item_rect);
  }

  int icon_x = kItemLeftMargin;
  int top_margin = GetTopMargin();
  int bottom_margin = GetBottomMargin();
  int icon_y = top_margin + (height() - kItemTopMargin -
                             bottom_margin - check_height) / 2;
  int icon_height = check_height;
  int icon_width = check_width;

  if (type_ == CHECKBOX && GetDelegate()->IsItemChecked(GetCommand())) {
    // Draw the check background.
    gfx::Rect check_bg_bounds(0, 0, icon_x + icon_width, height());
    const int bg_state = IsEnabled() ? MCB_NORMAL : MCB_DISABLED;
    AdjustBoundsForRTLUI(&check_bg_bounds);
    RECT check_bg_rect = check_bg_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuCheckBackground(
        NativeTheme::MENU, dc, MENU_POPUPCHECKBACKGROUND, bg_state,
        &check_bg_rect);

    // And the check.
    gfx::Rect check_bounds(icon_x, icon_y, icon_width, icon_height);
    const int check_state = IsEnabled() ? MC_CHECKMARKNORMAL :
                                          MC_CHECKMARKDISABLED;
    AdjustBoundsForRTLUI(&check_bounds);
    RECT check_rect = check_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuCheck(
        NativeTheme::MENU, dc, MENU_POPUPCHECK, check_state, &check_rect,
        render_selection);
  }

  // Render the foreground.
  // Menu color is specific to Vista, fallback to classic colors if can't
  // get color.
  int default_sys_color = render_selection ? COLOR_HIGHLIGHTTEXT :
      (IsEnabled() ? COLOR_MENUTEXT : COLOR_GRAYTEXT);
  SkColor fg_color = NativeTheme::instance()->GetThemeColorWithDefault(
      NativeTheme::MENU, MENU_POPUPITEM, state, TMT_TEXTCOLOR,
      default_sys_color);
  int width = this->width() - item_right_margin - label_start;
  gfx::Font& font = GetRootMenuItem()->font_;
  gfx::Rect text_bounds(label_start, top_margin, width, font.height());
  text_bounds.set_x(MirroredLeftPointForRect(text_bounds));
  if (for_drag) {
    // With different themes, it's difficult to tell what the correct foreground
    // and background colors are for the text to draw the correct halo. Instead,
    // just draw black on white, which will look good in most cases.
    canvas->DrawStringWithHalo(GetTitle(), font, 0x00000000, 0xFFFFFFFF,
                               text_bounds.x(), text_bounds.y(),
                               text_bounds.width(), text_bounds.height(),
                               GetRootMenuItem()->GetDrawStringFlags());
  } else {
    canvas->DrawStringInt(GetTitle(), font, fg_color,
                          text_bounds.x(), text_bounds.y(), text_bounds.width(),
                          text_bounds.height(),
                          GetRootMenuItem()->GetDrawStringFlags());
  }

  if (icon_.width() > 0) {
    gfx::Rect icon_bounds(kItemLeftMargin,
                          top_margin + (height() - top_margin -
                          bottom_margin - icon_.height()) / 2,
                          icon_.width(),
                          icon_.height());
    icon_bounds.set_x(MirroredLeftPointForRect(icon_bounds));
    canvas->DrawBitmapInt(icon_, icon_bounds.x(), icon_bounds.y());
  }

  if (HasSubmenu()) {
    int state_id = IsEnabled() ? MSM_NORMAL : MSM_DISABLED;
    gfx::Rect arrow_bounds(this->width() - item_right_margin + kLabelToArrowPadding,
                           0, arrow_width, height());
    AdjustBoundsForRTLUI(&arrow_bounds);

    // If our sub menus open from right to left (which is the case when the
    // locale is RTL) then we should make sure the menu arrow points to the
    // right direction.
    NativeTheme::MenuArrowDirection arrow_direction;
    if (UILayoutIsRightToLeft())
      arrow_direction = NativeTheme::LEFT_POINTING_ARROW;
    else
      arrow_direction = NativeTheme::RIGHT_POINTING_ARROW;

    RECT arrow_rect = arrow_bounds.ToRECT();
    NativeTheme::instance()->PaintMenuArrow(
        NativeTheme::MENU, dc, MENU_POPUPSUBMENU, state_id, &arrow_rect,
        arrow_direction, render_selection);
  }
  canvas->endPlatformPaint();
}

void MenuItemView::DestroyAllMenuHosts() {
  if (!HasSubmenu())
    return;

  submenu_->Close();
  for (int i = 0, item_count = submenu_->GetMenuItemCount(); i < item_count;
       ++i) {
    submenu_->GetMenuItemAt(i)->DestroyAllMenuHosts();
  }
}

int MenuItemView::GetTopMargin() {
  MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_ ? kItemTopMargin : kItemNoIconTopMargin;
}

int MenuItemView::GetBottomMargin() {
  MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_ ?
      kItemBottomMargin : kItemNoIconBottomMargin;
}

}  // namespace views
