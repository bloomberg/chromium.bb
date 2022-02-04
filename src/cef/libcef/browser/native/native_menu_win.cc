// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/native_menu_win.h"

#include "libcef/browser/native/menu_2.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/wrapped_window_proc.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/color/color_provider_manager.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/themed_vector_icon.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_insertion_delegate_win.h"

using ui::NativeTheme;

namespace views {

// The width of an icon, including the pixels between the icon and
// the item label.
static const int kIconWidth = 23;
// Margins between the top of the item and the label.
static const int kItemTopMargin = 3;
// Margins between the bottom of the item and the label.
static const int kItemBottomMargin = 4;
// Margins between the left of the item and the icon.
static const int kItemLeftMargin = 4;
// The width for displaying the sub-menu arrow.
static const int kArrowWidth = 10;

// Horizontal spacing between the end of an item (i.e. an icon or a checkbox)
// and the start of its corresponding text.
constexpr int kItemLabelSpacing = 10;

namespace {

// Draws the top layer of the canvas into the specified HDC. Only works
// with a SkCanvas with a BitmapPlatformDevice. Will create a temporary
// HDC to back the canvas if one doesn't already exist, tearing it down
// before returning. If |src_rect| is null, copies the entire canvas.
// Deleted from skia/ext/platform_canvas.h in https://crbug.com/675977#c13
void DrawToNativeContext(SkCanvas* canvas,
                         HDC destination_hdc,
                         int x,
                         int y,
                         const RECT* src_rect) {
  RECT temp_rect;
  if (!src_rect) {
    temp_rect.left = 0;
    temp_rect.right = canvas->imageInfo().width();
    temp_rect.top = 0;
    temp_rect.bottom = canvas->imageInfo().height();
    src_rect = &temp_rect;
  }
  skia::CopyHDC(skia::GetNativeDrawingContext(canvas), destination_hdc, x, y,
                canvas->imageInfo().isOpaque(), *src_rect,
                canvas->getTotalMatrix());
}

HFONT CreateNativeFont(const gfx::Font& font) {
  // Extracts |fonts| properties.
  const DWORD italic = (font.GetStyle() & gfx::Font::ITALIC) ? TRUE : FALSE;
  const DWORD underline =
      (font.GetStyle() & gfx::Font::UNDERLINE) ? TRUE : FALSE;
  // The font mapper matches its absolute value against the character height of
  // the available fonts.
  const int height = -font.GetFontSize();

  // Select the primary font which forces a mapping to a physical font.
  return ::CreateFont(height, 0, 0, 0, static_cast<int>(font.GetWeight()),
                      italic, underline, FALSE, DEFAULT_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                      DEFAULT_PITCH | FF_DONTCARE,
                      base::UTF8ToWide(font.GetFontName()).c_str());
}

}  // namespace

struct CefNativeMenuWin::ItemData {
  // The Windows API requires that whoever creates the menus must own the
  // strings used for labels, and keep them around for the lifetime of the
  // created menu. So be it.
  std::wstring label;

  // Someone needs to own submenus, it may as well be us.
  std::unique_ptr<Menu2> submenu;

  // We need a pointer back to the containing menu in various circumstances.
  CefNativeMenuWin* native_menu_win;

  // The index of the item within the menu's model.
  int model_index;
};

// Returns the CefNativeMenuWin for a particular HMENU.
static CefNativeMenuWin* GetCefNativeMenuWinFromHMENU(HMENU hmenu) {
  MENUINFO mi = {0};
  mi.cbSize = sizeof(mi);
  mi.fMask = MIM_MENUDATA | MIM_STYLE;
  GetMenuInfo(hmenu, &mi);
  return reinterpret_cast<CefNativeMenuWin*>(mi.dwMenuData);
}

// A window that receives messages from Windows relevant to the native menu
// structure we have constructed in CefNativeMenuWin.
class CefNativeMenuWin::MenuHostWindow {
 public:
  MenuHostWindow() {
    RegisterClass();
    hwnd_ = CreateWindowEx(l10n_util::GetExtendedStyles(), kWindowClassName,
                           L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    gfx::CheckWindowCreated(hwnd_, ::GetLastError());
    gfx::SetWindowUserData(hwnd_, this);
  }

  MenuHostWindow(const MenuHostWindow&) = delete;
  MenuHostWindow& operator=(const MenuHostWindow&) = delete;

  ~MenuHostWindow() { DestroyWindow(hwnd_); }

  HWND hwnd() const { return hwnd_; }

 private:
  static const wchar_t* kWindowClassName;

  void RegisterClass() {
    static bool registered = false;
    if (registered)
      return;

    WNDCLASSEX window_class;
    base::win::InitializeWindowClass(
        kWindowClassName, &base::win::WrappedWindowProc<MenuHostWindowProc>,
        CS_DBLCLKS, 0, 0, NULL, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
        NULL, NULL, NULL, &window_class);
    ATOM clazz = RegisterClassEx(&window_class);
    CHECK(clazz);
    registered = true;
  }

  // Converts the WPARAM value passed to WM_MENUSELECT into an index
  // corresponding to the menu item that was selected.
  int GetMenuItemIndexFromWPARAM(HMENU menu, WPARAM w_param) const {
    int count = GetMenuItemCount(menu);
    // For normal command menu items, Windows passes a command id as the LOWORD
    // of WPARAM for WM_MENUSELECT. We need to walk forward through the menu
    // items to find an item with a matching ID. Ugh!
    for (int i = 0; i < count; ++i) {
      MENUITEMINFO mii = {0};
      mii.cbSize = sizeof(mii);
      mii.fMask = MIIM_ID;
      GetMenuItemInfo(menu, i, MF_BYPOSITION, &mii);
      if (mii.wID == w_param)
        return i;
    }
    // If we didn't find a matching command ID, this means a submenu has been
    // selected instead, and rather than passing a command ID in
    // LOWORD(w_param), Windows has actually passed us a position, so we just
    // return it.
    return w_param;
  }

  CefNativeMenuWin::ItemData* GetItemData(ULONG_PTR item_data) {
    return reinterpret_cast<CefNativeMenuWin::ItemData*>(item_data);
  }

  // Called when the user selects a specific item.
  void OnMenuCommand(int position, HMENU menu) {
    CefNativeMenuWin* menu_win = GetCefNativeMenuWinFromHMENU(menu);
    ui::MenuModel* model = menu_win->model_;
    CefNativeMenuWin* root_menu = menu_win;
    while (root_menu->parent_)
      root_menu = root_menu->parent_;

    // Only notify the model if it didn't already send out notification.
    // See comment in MenuMessageHook for details.
    if (root_menu->menu_action_ == MenuWrapper::MENU_ACTION_NONE)
      model->ActivatedAt(position);
  }

  // Called by Windows to measure the size of an owner-drawn menu item.
  void OnMeasureItem(WPARAM w_param, MEASUREITEMSTRUCT* measure_item_struct) {
    CefNativeMenuWin::ItemData* data =
        GetItemData(measure_item_struct->itemData);
    if (data) {
      gfx::FontList font_list;
      measure_item_struct->itemWidth =
          gfx::GetStringWidth(base::WideToUTF16(data->label), font_list) +
          kIconWidth + kItemLeftMargin + kItemLabelSpacing -
          GetSystemMetrics(SM_CXMENUCHECK);
      if (data->submenu.get())
        measure_item_struct->itemWidth += kArrowWidth;
      // If the label contains an accelerator, make room for tab.
      if (data->label.find(L'\t') != std::wstring::npos)
        measure_item_struct->itemWidth += gfx::GetStringWidth(u" ", font_list);
      measure_item_struct->itemHeight =
          font_list.GetHeight() + kItemBottomMargin + kItemTopMargin;
    } else {
      // Measure separator size.
      measure_item_struct->itemHeight = GetSystemMetrics(SM_CYMENU) / 2;
      measure_item_struct->itemWidth = 0;
    }
  }

  // Called by Windows to paint an owner-drawn menu item.
  void OnDrawItem(UINT w_param, DRAWITEMSTRUCT* draw_item_struct) {
    HDC dc = draw_item_struct->hDC;
    COLORREF prev_bg_color, prev_text_color;

    // Set background color and text color
    if (draw_item_struct->itemState & ODS_SELECTED) {
      prev_bg_color = SetBkColor(dc, GetSysColor(COLOR_HIGHLIGHT));
      prev_text_color = SetTextColor(dc, GetSysColor(COLOR_HIGHLIGHTTEXT));
    } else {
      prev_bg_color = SetBkColor(dc, GetSysColor(COLOR_MENU));
      if (draw_item_struct->itemState & ODS_DISABLED)
        prev_text_color = SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
      else
        prev_text_color = SetTextColor(dc, GetSysColor(COLOR_MENUTEXT));
    }

    if (draw_item_struct->itemData) {
      CefNativeMenuWin::ItemData* data =
          GetItemData(draw_item_struct->itemData);
      // Draw the background.
      HBRUSH hbr = CreateSolidBrush(GetBkColor(dc));
      FillRect(dc, &draw_item_struct->rcItem, hbr);
      DeleteObject(hbr);

      // Draw the label.
      RECT rect = draw_item_struct->rcItem;
      rect.top += kItemTopMargin;
      // Should we add kIconWidth only when icon.width() != 0 ?
      rect.left += kItemLeftMargin + kIconWidth;
      rect.right -= kItemLabelSpacing;
      UINT format = DT_TOP | DT_SINGLELINE;
      // Check whether the mnemonics should be underlined.
      BOOL underline_mnemonics;
      SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &underline_mnemonics, 0);
      if (!underline_mnemonics)
        format |= DT_HIDEPREFIX;
      gfx::FontList font_list;
      HFONT new_font = CreateNativeFont(font_list.GetPrimaryFont());
      HGDIOBJ old_font = SelectObject(dc, new_font);

      // If an accelerator is specified (with a tab delimiting the rest of the
      // label from the accelerator), we have to justify the fist part on the
      // left and the accelerator on the right.
      // TODO(jungshik): This will break in RTL UI. Currently, he/ar use the
      //                 window system UI font and will not hit here.
      std::wstring label = data->label;
      std::wstring accel;
      std::wstring::size_type tab_pos = label.find(L'\t');
      if (tab_pos != std::wstring::npos) {
        accel = label.substr(tab_pos);
        label = label.substr(0, tab_pos);
      }
      DrawTextEx(dc, const_cast<wchar_t*>(label.data()),
                 static_cast<int>(label.size()), &rect, format | DT_LEFT, NULL);
      if (!accel.empty()) {
        DrawTextEx(dc, const_cast<wchar_t*>(accel.data()),
                   static_cast<int>(accel.size()), &rect, format | DT_RIGHT,
                   NULL);
      }
      SelectObject(dc, old_font);
      DeleteObject(new_font);

      ui::MenuModel::ItemType type =
          data->native_menu_win->model_->GetTypeAt(data->model_index);

      // Draw the icon after the label, otherwise it would be covered
      // by the label.
      ui::ImageModel icon =
          data->native_menu_win->model_->GetIconAt(data->model_index);
      if (icon.IsImage() || icon.IsVectorIcon()) {
        ui::NativeTheme* native_theme =
            ui::NativeTheme::GetInstanceForNativeUi();

        auto* color_provider =
            ui::ColorProviderManager::Get().GetColorProviderFor(
                native_theme->GetColorProviderKey(nullptr));

        // We currently don't support items with both icons and checkboxes.
        const gfx::ImageSkia skia_icon =
            icon.IsImage() ? icon.GetImage().AsImageSkia()
                           : ui::ThemedVectorIcon(icon.GetVectorIcon())
                                 .GetImageSkia(color_provider, 16);

        DCHECK(type != ui::MenuModel::TYPE_CHECK);
        std::unique_ptr<SkCanvas> canvas = skia::CreatePlatformCanvas(
            skia_icon.width(), skia_icon.height(), false);
        canvas->drawImage(skia_icon.bitmap()->asImage(), 0, 0);
        DrawToNativeContext(
            canvas.get(), dc, draw_item_struct->rcItem.left + kItemLeftMargin,
            draw_item_struct->rcItem.top +
                (draw_item_struct->rcItem.bottom -
                 draw_item_struct->rcItem.top - skia_icon.height()) /
                    2,
            NULL);
      } else if (type == ui::MenuModel::TYPE_CHECK &&
                 data->native_menu_win->model_->IsItemCheckedAt(
                     data->model_index)) {
        // Manually render a checkbox.
        const MenuConfig& config = MenuConfig::instance();
        NativeTheme::State state;
        if (draw_item_struct->itemState & ODS_DISABLED) {
          state = NativeTheme::kDisabled;
        } else {
          state = draw_item_struct->itemState & ODS_SELECTED
                      ? NativeTheme::kHovered
                      : NativeTheme::kNormal;
        }

        std::unique_ptr<SkCanvas> canvas = skia::CreatePlatformCanvas(
            config.check_width, config.check_height, false);
        cc::SkiaPaintCanvas paint_canvas(canvas.get());

        NativeTheme::ExtraParams extra;
        extra.menu_check.is_radio = false;
        gfx::Rect bounds(0, 0, config.check_width, config.check_height);

        // Draw the background and the check.
        ui::NativeTheme* native_theme =
            ui::NativeTheme::GetInstanceForNativeUi();
        auto* color_provider =
            ui::ColorProviderManager::Get().GetColorProviderFor(
                native_theme->GetColorProviderKey(nullptr));

        native_theme->Paint(&paint_canvas, color_provider,
                            NativeTheme::kMenuCheckBackground, state, bounds,
                            extra);
        native_theme->Paint(&paint_canvas, color_provider,
                            NativeTheme::kMenuCheck, state, bounds, extra);

        // Draw checkbox to menu.
        DrawToNativeContext(
            canvas.get(), dc, draw_item_struct->rcItem.left + kItemLeftMargin,
            draw_item_struct->rcItem.top +
                (draw_item_struct->rcItem.bottom -
                 draw_item_struct->rcItem.top - config.check_height) /
                    2,
            NULL);
      }
    } else {
      // Draw the separator
      draw_item_struct->rcItem.top +=
          (draw_item_struct->rcItem.bottom - draw_item_struct->rcItem.top) / 3;
      DrawEdge(dc, &draw_item_struct->rcItem, EDGE_ETCHED, BF_TOP);
    }

    SetBkColor(dc, prev_bg_color);
    SetTextColor(dc, prev_text_color);
  }

  bool ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT* l_result) {
    switch (message) {
      case WM_MENUCOMMAND:
        OnMenuCommand(w_param, reinterpret_cast<HMENU>(l_param));
        *l_result = 0;
        return true;
      case WM_MENUSELECT:
        *l_result = 0;
        return true;
      case WM_MEASUREITEM:
        OnMeasureItem(w_param, reinterpret_cast<MEASUREITEMSTRUCT*>(l_param));
        *l_result = 0;
        return true;
      case WM_DRAWITEM:
        OnDrawItem(w_param, reinterpret_cast<DRAWITEMSTRUCT*>(l_param));
        *l_result = 0;
        return true;
        // TODO(beng): bring over owner draw from old menu system.
    }
    return false;
  }

  static LRESULT CALLBACK MenuHostWindowProc(HWND window,
                                             UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
    MenuHostWindow* host =
        reinterpret_cast<MenuHostWindow*>(gfx::GetWindowUserData(window));
    // host is null during initial construction.
    LRESULT l_result = 0;
    if (!host || !host->ProcessWindowMessage(window, message, w_param, l_param,
                                             &l_result)) {
      return DefWindowProc(window, message, w_param, l_param);
    }
    return l_result;
  }

  HWND hwnd_;
};

struct CefNativeMenuWin::HighlightedMenuItemInfo {
  HighlightedMenuItemInfo()
      : has_parent(false), has_submenu(false), menu(nullptr), position(-1) {}

  bool has_parent;
  bool has_submenu;

  // The menu and position. These are only set for non-disabled menu items.
  CefNativeMenuWin* menu;
  int position;
};

// static
const wchar_t* CefNativeMenuWin::MenuHostWindow::kWindowClassName =
    L"ViewsMenuHostWindow";

////////////////////////////////////////////////////////////////////////////////
// CefNativeMenuWin, public:

CefNativeMenuWin::CefNativeMenuWin(ui::MenuModel* model, HWND system_menu_for)
    : model_(model),
      menu_(nullptr),
      owner_draw_(l10n_util::NeedOverrideDefaultUIFont(NULL, NULL) &&
                  !system_menu_for),
      system_menu_for_(system_menu_for),
      first_item_index_(0),
      menu_action_(MENU_ACTION_NONE),
      menu_to_select_(nullptr),
      position_to_select_(-1),
      parent_(nullptr),
      destroyed_flag_(nullptr),
      menu_to_select_factory_(this) {}

CefNativeMenuWin::~CefNativeMenuWin() {
  if (destroyed_flag_)
    *destroyed_flag_ = true;
  DestroyMenu(menu_);
}

////////////////////////////////////////////////////////////////////////////////
// CefNativeMenuWin, MenuWrapper implementation:

void CefNativeMenuWin::RunMenuAt(const gfx::Point& point, int alignment) {
  CreateHostWindow();
  UpdateStates();
  UINT flags = TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RECURSE;
  flags |= GetAlignmentFlags(alignment);
  menu_action_ = MENU_ACTION_NONE;

  // Set a hook function so we can listen for keyboard events while the
  // menu is open, and store a pointer to this object in a static
  // variable so the hook has access to it (ugly, but it's the
  // only way).
  open_native_menu_win_ = this;
  HHOOK hhook = SetWindowsHookEx(WH_MSGFILTER, MenuMessageHook,
                                 GetModuleHandle(NULL), ::GetCurrentThreadId());

  // Command dispatch is done through WM_MENUCOMMAND, handled by the host
  // window.
  menu_to_select_ = nullptr;
  position_to_select_ = -1;
  menu_to_select_factory_.InvalidateWeakPtrs();
  bool destroyed = false;
  destroyed_flag_ = &destroyed;
  model_->MenuWillShow();
  TrackPopupMenu(menu_, flags, point.x(), point.y(), 0, host_window_->hwnd(),
                 NULL);
  UnhookWindowsHookEx(hhook);
  open_native_menu_win_ = nullptr;
  if (destroyed)
    return;
  destroyed_flag_ = nullptr;
  if (menu_to_select_) {
    // Folks aren't too happy if we notify immediately. In particular, notifying
    // the delegate can cause destruction leaving the stack in a weird
    // state. Instead post a task, then notify. This mirrors what WM_MENUCOMMAND
    // does.
    menu_to_select_factory_.InvalidateWeakPtrs();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&CefNativeMenuWin::DelayedSelect,
                                  menu_to_select_factory_.GetWeakPtr()));
    menu_action_ = MENU_ACTION_SELECTED;
  }
  // Send MenuWillClose after we schedule the select, otherwise MenuWillClose is
  // processed after the select (MenuWillClose posts a delayed task too).
  model_->MenuWillClose();
}

void CefNativeMenuWin::CancelMenu() {
  EndMenu();
}

void CefNativeMenuWin::Rebuild(MenuInsertionDelegateWin* delegate) {
  ResetNativeMenu();
  items_.clear();

  owner_draw_ = model_->HasIcons() || owner_draw_;
  first_item_index_ = delegate ? delegate->GetInsertionIndex(menu_) : 0;
  for (int menu_index = first_item_index_;
       menu_index < first_item_index_ + model_->GetItemCount(); ++menu_index) {
    int model_index = menu_index - first_item_index_;
    if (model_->GetTypeAt(model_index) == ui::MenuModel::TYPE_SEPARATOR)
      AddSeparatorItemAt(menu_index, model_index);
    else
      AddMenuItemAt(menu_index, model_index);
  }
}

void CefNativeMenuWin::UpdateStates() {
  // A depth-first walk of the menu items, updating states.
  int model_index = 0;
  ItemDataList::const_iterator it;
  for (it = items_.begin(); it != items_.end(); ++it, ++model_index) {
    int menu_index = model_index + first_item_index_;
    SetMenuItemState(menu_index, model_->IsEnabledAt(model_index),
                     model_->IsItemCheckedAt(model_index), false);
    if (model_->IsItemDynamicAt(model_index)) {
      // TODO(atwilson): Update the icon as well (http://crbug.com/66508).
      SetMenuItemLabel(menu_index, model_index,
                       base::UTF16ToWide(model_->GetLabelAt(model_index)));
    }
    Menu2* submenu = (*it)->submenu.get();
    if (submenu)
      submenu->UpdateStates();
  }
}

HMENU CefNativeMenuWin::GetNativeMenu() const {
  return menu_;
}

CefNativeMenuWin::MenuAction CefNativeMenuWin::GetMenuAction() const {
  return menu_action_;
}

void CefNativeMenuWin::SetMinimumWidth(int width) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// CefNativeMenuWin, private:

// static
CefNativeMenuWin* CefNativeMenuWin::open_native_menu_win_ = nullptr;

void CefNativeMenuWin::DelayedSelect() {
  if (menu_to_select_)
    menu_to_select_->model_->ActivatedAt(position_to_select_);
}

// static
bool CefNativeMenuWin::GetHighlightedMenuItemInfo(
    HMENU menu,
    HighlightedMenuItemInfo* info) {
  for (int i = 0; i < ::GetMenuItemCount(menu); i++) {
    UINT state = ::GetMenuState(menu, i, MF_BYPOSITION);
    if (state & MF_HILITE) {
      if (state & MF_POPUP) {
        HMENU submenu = GetSubMenu(menu, i);
        if (GetHighlightedMenuItemInfo(submenu, info))
          info->has_parent = true;
        else
          info->has_submenu = true;
      } else if (!(state & MF_SEPARATOR) && !(state & MF_DISABLED)) {
        info->menu = GetCefNativeMenuWinFromHMENU(menu);
        info->position = i;
      }
      return true;
    }
  }
  return false;
}

// static
LRESULT CALLBACK CefNativeMenuWin::MenuMessageHook(int n_code,
                                                   WPARAM w_param,
                                                   LPARAM l_param) {
  LRESULT result = CallNextHookEx(NULL, n_code, w_param, l_param);

  CefNativeMenuWin* this_ptr = open_native_menu_win_;
  if (!this_ptr)
    return result;

  MSG* msg = reinterpret_cast<MSG*>(l_param);
  if (msg->message == WM_LBUTTONUP || msg->message == WM_RBUTTONUP) {
    HighlightedMenuItemInfo info;
    if (GetHighlightedMenuItemInfo(this_ptr->menu_, &info) && info.menu) {
      // It appears that when running a menu by way of TrackPopupMenu(Ex) win32
      // gets confused if the underlying window paints itself. As its very easy
      // for the underlying window to repaint itself (especially since some menu
      // items trigger painting of the tabstrip on mouse over) we have this
      // workaround. When the mouse is released on a menu item we remember the
      // menu item and end the menu. When the nested message loop returns we
      // schedule a task to notify the model. It's still possible to get a
      // WM_MENUCOMMAND, so we have to be careful that we don't notify the model
      // twice.
      this_ptr->menu_to_select_ = info.menu;
      this_ptr->position_to_select_ = info.position;
      EndMenu();
    }
  } else if (msg->message == WM_KEYDOWN) {
    HighlightedMenuItemInfo info;
    if (GetHighlightedMenuItemInfo(this_ptr->menu_, &info)) {
      if (msg->wParam == VK_LEFT && !info.has_parent) {
        this_ptr->menu_action_ = MENU_ACTION_PREVIOUS;
        ::EndMenu();
      } else if (msg->wParam == VK_RIGHT && !info.has_parent &&
                 !info.has_submenu) {
        this_ptr->menu_action_ = MENU_ACTION_NEXT;
        ::EndMenu();
      }
    }
  }

  return result;
}

bool CefNativeMenuWin::IsSeparatorItemAt(int menu_index) const {
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE;
  GetMenuItemInfo(menu_, menu_index, MF_BYPOSITION, &mii);
  return !!(mii.fType & MF_SEPARATOR);
}

void CefNativeMenuWin::AddMenuItemAt(int menu_index, int model_index) {
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_DATA;
  if (!owner_draw_)
    mii.fType = MFT_STRING;
  else
    mii.fType = MFT_OWNERDRAW;

  std::unique_ptr<ItemData> item_data = std::make_unique<ItemData>();
  item_data->label = std::wstring();
  ui::MenuModel::ItemType type = model_->GetTypeAt(model_index);
  if (type == ui::MenuModel::TYPE_SUBMENU) {
    item_data->submenu.reset(new Menu2(model_->GetSubmenuModelAt(model_index)));
    mii.fMask |= MIIM_SUBMENU;
    mii.hSubMenu = item_data->submenu->GetNativeMenu();
    GetCefNativeMenuWinFromHMENU(mii.hSubMenu)->parent_ = this;
  } else {
    if (type == ui::MenuModel::TYPE_RADIO)
      mii.fType |= MFT_RADIOCHECK;
    mii.wID = model_->GetCommandIdAt(model_index);
  }
  item_data->native_menu_win = this;
  item_data->model_index = model_index;
  mii.dwItemData = reinterpret_cast<ULONG_PTR>(item_data.get());
  items_.insert(items_.begin() + model_index, std::move(item_data));
  UpdateMenuItemInfoForString(
      &mii, model_index, base::UTF16ToWide(model_->GetLabelAt(model_index)));
  InsertMenuItem(menu_, menu_index, TRUE, &mii);
}

void CefNativeMenuWin::AddSeparatorItemAt(int menu_index, int model_index) {
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE;
  mii.fType = MFT_SEPARATOR;
  // Insert a dummy entry into our label list so we can index directly into it
  // using item indices if need be.
  items_.insert(items_.begin() + model_index, std::make_unique<ItemData>());
  InsertMenuItem(menu_, menu_index, TRUE, &mii);
}

void CefNativeMenuWin::SetMenuItemState(int menu_index,
                                        bool enabled,
                                        bool checked,
                                        bool is_default) {
  if (IsSeparatorItemAt(menu_index))
    return;

  UINT state = enabled ? MFS_ENABLED : MFS_DISABLED;
  if (checked)
    state |= MFS_CHECKED;
  if (is_default)
    state |= MFS_DEFAULT;

  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_STATE;
  mii.fState = state;
  SetMenuItemInfo(menu_, menu_index, MF_BYPOSITION, &mii);
}

void CefNativeMenuWin::SetMenuItemLabel(int menu_index,
                                        int model_index,
                                        const std::wstring& label) {
  if (IsSeparatorItemAt(menu_index))
    return;

  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  UpdateMenuItemInfoForString(&mii, model_index, label);
  SetMenuItemInfo(menu_, menu_index, MF_BYPOSITION, &mii);
}

void CefNativeMenuWin::UpdateMenuItemInfoForString(MENUITEMINFO* mii,
                                                   int model_index,
                                                   const std::wstring& label) {
  std::wstring formatted = label;
  ui::MenuModel::ItemType type = model_->GetTypeAt(model_index);
  // Strip out any tabs, otherwise they get interpreted as accelerators and can
  // lead to weird behavior.
  base::ReplaceSubstringsAfterOffset(&formatted, 0, L"\t", L" ");
  if (type != ui::MenuModel::TYPE_SUBMENU) {
    // Add accelerator details to the label if provided.
    ui::Accelerator accelerator(ui::VKEY_UNKNOWN, ui::EF_NONE);
    if (model_->GetAcceleratorAt(model_index, &accelerator)) {
      formatted += L"\t";
      formatted += base::UTF16ToWide(accelerator.GetShortcutText());
    }
  }

  // Update the owned string, since Windows will want us to keep this new
  // version around.
  items_[model_index]->label = formatted;

  // Give Windows a pointer to the label string.
  mii->fMask |= MIIM_STRING;
  mii->dwTypeData = const_cast<wchar_t*>(items_[model_index]->label.c_str());
}

UINT CefNativeMenuWin::GetAlignmentFlags(int alignment) const {
  UINT alignment_flags = TPM_TOPALIGN;
  if (alignment == Menu2::ALIGN_TOPLEFT)
    alignment_flags |= TPM_LEFTALIGN;
  else if (alignment == Menu2::ALIGN_TOPRIGHT)
    alignment_flags |= TPM_RIGHTALIGN;
  return alignment_flags;
}

void CefNativeMenuWin::ResetNativeMenu() {
  if (IsWindow(system_menu_for_)) {
    if (menu_)
      GetSystemMenu(system_menu_for_, TRUE);
    menu_ = GetSystemMenu(system_menu_for_, FALSE);
  } else {
    if (menu_)
      DestroyMenu(menu_);
    menu_ = CreatePopupMenu();
    // Rather than relying on the return value of TrackPopupMenuEx, which is
    // always a command identifier, instead we tell the menu to notify us via
    // our host window and the WM_MENUCOMMAND message.
    MENUINFO mi = {0};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_STYLE | MIM_MENUDATA;
    mi.dwStyle = MNS_NOTIFYBYPOS;
    mi.dwMenuData = reinterpret_cast<ULONG_PTR>(this);
    SetMenuInfo(menu_, &mi);
  }
}

void CefNativeMenuWin::CreateHostWindow() {
  // This only gets called from RunMenuAt, and as such there is only ever one
  // host window per menu hierarchy, no matter how many CefNativeMenuWin objects
  // exist wrapping submenus.
  if (!host_window_.get())
    host_window_.reset(new MenuHostWindow());
}

////////////////////////////////////////////////////////////////////////////////
// MenuWrapper, public:

// static
MenuWrapper* MenuWrapper::CreateWrapper(ui::MenuModel* model) {
  return new CefNativeMenuWin(model, NULL);
}

}  // namespace views
