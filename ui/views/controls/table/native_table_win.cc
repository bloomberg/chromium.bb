// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/native_table_win.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>

#include "base/logging.h"
#include "base/win/scoped_gdi_object.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/models/table_model.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/icon_util.h"
#include "ui/views/controls/table/table_view2.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/widget/widget.h"

namespace views {

// Added to column width to prevent truncation.
const int kListViewTextPadding = 15;
// Additional column width necessary if column has icons.
const int kListViewIconWidthAndPadding = 18;

// static
const int NativeTableWin::kImageSize = 18;

////////////////////////////////////////////////////////////////////////////////
// NativeTableWin, public:

NativeTableWin::NativeTableWin(TableView2* table)
    : ignore_listview_change_(false),
      table_(table),
      content_offset_(0),
      header_original_handler_(NULL),
      original_handler_(NULL) {
  // Associates the actual HWND with the table so the table is the one
  // considered as having the focus (not the wrapper) when the HWND is
  // focused directly (with a click for example).
  set_focus_view(table);
}

NativeTableWin::~NativeTableWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTableWin, NativeTableWrapper implementation:

int NativeTableWin::GetRowCount() const {
  if (!native_view())
    return 0;
  return ListView_GetItemCount(native_view());
}

void NativeTableWin::InsertColumn(const ui::TableColumn& tc, int index) {
  if (!native_view())
    return;

  LVCOLUMN column = { 0 };
  column.mask = LVCF_TEXT|LVCF_FMT;
  column.pszText = const_cast<LPWSTR>(tc.title.c_str());
  switch (tc.alignment) {
      case ui::TableColumn::LEFT:
        column.fmt = LVCFMT_LEFT;
        break;
      case ui::TableColumn::RIGHT:
        column.fmt = LVCFMT_RIGHT;
        break;
      case ui::TableColumn::CENTER:
        column.fmt = LVCFMT_CENTER;
        break;
      default:
        NOTREACHED();
  }
  if (tc.width != -1) {
    column.mask |= LVCF_WIDTH;
    column.cx = tc.width;
  }
  column.mask |= LVCF_SUBITEM;
  // Sub-items are 1s indexed.
  column.iSubItem = index + 1;
  SendMessage(native_view(), LVM_INSERTCOLUMN, index,
              reinterpret_cast<LPARAM>(&column));
}

void NativeTableWin::RemoveColumn(int index) {
  if (!native_view())
    return;
  SendMessage(native_view(), LVM_DELETECOLUMN, index, 0);
  if (table_->model()->RowCount() > 0)
    OnRowsChanged(0, table_->model()->RowCount() - 1);
}

View* NativeTableWin::GetView() {
  return this;
}

void NativeTableWin::SetFocus() {
  // Focus the associated HWND.
  OnFocus();
}

gfx::NativeView NativeTableWin::GetTestingHandle() const {
  return native_view();
}

int NativeTableWin::GetColumnWidth(int column_index) const {
  if (!native_view())
    return 0;
  return ListView_GetColumnWidth(native_view(), column_index);
}

void NativeTableWin::SetColumnWidth(int column_index, int width) {
  if (!native_view())
    return;
  ListView_SetColumnWidth(native_view(), column_index, width);
}

int NativeTableWin::GetSelectedRowCount() const {
  if (!native_view())
    return 0;
  return ListView_GetSelectedCount(native_view());
}

int NativeTableWin::GetFirstSelectedRow() const {
  if (!native_view())
    return -1;
  return ListView_GetNextItem(native_view(), -1, LVNI_ALL | LVIS_SELECTED);
}

int NativeTableWin::GetFirstFocusedRow() const {
  if (!native_view())
    return -1;
  return ListView_GetNextItem(native_view(), -1, LVNI_ALL | LVIS_FOCUSED);
}

void NativeTableWin::ClearSelection() {
  if (native_view())
    ListView_SetItemState(native_view(), -1, 0, LVIS_SELECTED);
}

void NativeTableWin::ClearRowFocus() {
  if (native_view())
    ListView_SetItemState(native_view(), -1, 0, LVIS_FOCUSED);
}

void NativeTableWin::SetSelectedState(int model_row, bool state) {
  if (!native_view())
    return;

  SendMessage(native_view(), WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  ListView_SetItemState(native_view(), model_row,
                        state ? LVIS_SELECTED : 0, LVIS_SELECTED);
  // Make the selected row visible.
  ListView_EnsureVisible(native_view(), model_row, FALSE);
  SendMessage(native_view(), WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
}

void NativeTableWin::SetFocusState(int model_row, bool state) {
  if (!native_view())
    return;
  ListView_SetItemState(native_view(), model_row,
                        state ? LVIS_FOCUSED : 0, LVIS_FOCUSED)
}

bool NativeTableWin::IsRowSelected(int model_row) const {
  if (!native_view())
    return false;
  return ListView_GetItemState(native_view(), model_row, LVIS_SELECTED) ==
      LVIS_SELECTED;
}

bool NativeTableWin::IsRowFocused(int model_row) const {
  if (!native_view())
    return false;
  return ListView_GetItemState(native_view(), model_row, LVIS_FOCUSED) ==
      LVIS_FOCUSED;
}

void NativeTableWin::OnRowsChanged(int start, int length) {
  if (!native_view())
    return;

  SendMessage(native_view(), WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  UpdateListViewCache(start, length, false);
  SendMessage(native_view(), WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
}

void NativeTableWin::OnRowsAdded(int start, int length) {
  if (!native_view())
    return;

  SendMessage(native_view(), WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  UpdateListViewCache(start, length, true);
  SendMessage(native_view(), WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
}

void NativeTableWin::OnRowsRemoved(int start, int length) {
  if (!native_view())
    return;

  SendMessage(native_view(), WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);

  bool had_selection = (GetSelectedRowCount() > 0);
  int old_row_count = GetRowCount();
  if (start == 0 && length == GetRowCount()) {
    // Everything was removed.
    ListView_DeleteAllItems(native_view());
  } else {
    for (int i = 0; i < length; ++i)
      ListView_DeleteItem(native_view(), start);
  }

  SendMessage(native_view(), WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);

  // If the row count goes to zero and we had a selection LVN_ITEMCHANGED isn't
  // invoked, so we handle it here.
  //
  // When the model is set to NULL all the rows are removed. We don't notify
  // the delegate in this case as setting the model to NULL is usually done as
  // the last step before being deleted and callers shouldn't have to deal with
  // getting a selection change when the model is being reset.
  if (table_->model() && table_->observer() && had_selection &&
      GetRowCount() == 0) {
    table_->observer()->OnSelectionChanged();
  }
}

gfx::Rect NativeTableWin::GetBounds() const {
  RECT native_bounds;
  if (!native_view() || GetClientRect(native_view(), &native_bounds))
    return gfx::Rect();
  return gfx::Rect(native_bounds);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTableWin, View overrides:

gfx::Size NativeTableWin::GetPreferredSize() {
  SIZE sz = {0};
  SendMessage(native_view(), BCM_GETIDEALSIZE, 0,
              reinterpret_cast<LPARAM>(&sz));

  return gfx::Size(sz.cx, sz.cy);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTableWin, NativeControlWin overrides:

bool NativeTableWin::ProcessMessage(UINT message, WPARAM w_param,
                                    LPARAM l_param, LRESULT* result) {
  if (message == WM_NOTIFY) {
    LPNMHDR hdr = reinterpret_cast<LPNMHDR>(l_param);
    switch (hdr->code) {
      case NM_CUSTOMDRAW: {
        // Draw notification. dwDragState indicates the current stage of
        // drawing.
        *result = OnCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW*>(hdr));
        return true;
      }

      case LVN_ITEMCHANGED: {
        // Notification that the state of an item has changed. The state
        // includes such things as whether the item is selected.
        NMLISTVIEW* state_change = reinterpret_cast<NMLISTVIEW*>(hdr);
        if ((state_change->uChanged & LVIF_STATE) != 0) {
          if ((state_change->uOldState & LVIS_SELECTED) !=
              (state_change->uNewState & LVIS_SELECTED)) {
            // Selected state of the item changed.
            OnSelectedStateChanged();
          }
        }
        break;
      }

      case HDN_BEGINTRACKW:
      case HDN_BEGINTRACKA:
        // Prevent clicks so columns cannot be resized.
        if (!table_->resizable_columns())
          return true;
        break;

      case NM_DBLCLK:
        OnDoubleClick();
        break;

      case LVN_COLUMNCLICK: {
        const ui::TableColumn& column = table_->GetVisibleColumnAt(
            reinterpret_cast<NMLISTVIEW*>(hdr)->iSubItem);
        break;
      }

      case LVN_MARQUEEBEGIN:  // We don't want the marquee selection.
        return true;

      case LVN_GETINFOTIP: {
        // This is called when the user hovers items in column zero.
        //   * If the text in this column is not fully visible, the dwFlags
        //     field will be set to 0, and pszText will contain the full text.
        //     If you return without making any changes, this text will be
        //     displayed in a "labeltip" - a bubble that's overlaid (at the
        //     correct alignment!) on the item.  If you return with a different
        //     pszText, it will be displayed as a tooltip if nonempty.
        //   * Otherwise, dwFlags will be LVGIT_UNFOLDED and pszText will be
        //     empty.  On return, if pszText is nonempty, it will be displayed
        //     as a labeltip if dwFlags has been changed to 0 (even if it bears
        //     no resemblance to the item text), or as a tooltip otherwise.
        //
        // Once the tooltip for an item has been obtained, this will not be
        // called again until the user hovers a different item.  If after that
        // the original item is hovered a second time, this will be called.
        //
        // When the user hovers items in other columns, they will be "unfolded"
        // (displayed as labeltips) when necessary, but this function will
        // never be called.
        //
        // Changing the LVS_EX_INFOTIP extended style to LVS_EX_LABELTIP will
        // cause all of the above to be true except that this function will not
        // be called when dwFlags would be LVGIT_UNFOLDED.  Removing it entirely
        // will disable all of the above behavior.
        NMLVGETINFOTIP* info_tip = reinterpret_cast<NMLVGETINFOTIP*>(hdr);
        string16 tooltip = table_->model()->GetTooltip(info_tip->iItem);
        CHECK_GE(info_tip->cchTextMax, 2);
        if (tooltip.length() >= static_cast<size_t>(info_tip->cchTextMax)) {
          // Elide the tooltip if necessary.
          tooltip.erase(info_tip->cchTextMax - 2);  // Ellipsis + '\0'
          const wchar_t kEllipsis = L'\x2026';
          tooltip += kEllipsis;
        }
        if (!tooltip.empty())
          wcscpy_s(info_tip->pszText, tooltip.length() + 1, tooltip.c_str());
        return true;
      }

      default:
        break;
    }
  }

  return NativeControlWin::ProcessMessage(message, w_param, l_param, result);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTableWin, protected:

void NativeTableWin::CreateNativeControl() {
  int style = WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS;
  if (table_->single_selection())
    style |= LVS_SINGLESEL;
  // If there's only one column and the title string is empty, don't show a
  // header.
  if (table_->GetVisibleColumnCount() == 1U) {
    if (table_->GetVisibleColumnAt(1).title.empty())
      style |= LVS_NOCOLUMNHEADER;
  }
  HWND hwnd = ::CreateWindowEx(WS_EX_CLIENTEDGE | GetAdditionalRTLStyle(),
                               WC_LISTVIEW,
                               L"",
                               style,
                               0, 0, width(), height(),
                               table_->GetWidget()->GetNativeView(),
                               NULL, NULL, NULL);
  ui::CheckWindowCreated(hwnd);

  // Reduce overdraw/flicker artifacts by double buffering.  Support tooltips
  // and display elided items completely on hover (see comments in OnNotify()
  // under LVN_GETINFOTIP).  Make the selection extend across the row.
  ListView_SetExtendedListViewStyle(hwnd,
      LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
  l10n_util::AdjustUIFontForWindow(hwnd);

  NativeControlCreated(hwnd);
  // native_view() is now valid.

  // Add the columns.
  for (size_t i = 0; i < table_->GetVisibleColumnCount(); ++i)
    InsertColumn(table_->GetVisibleColumnAt(i), i);

  if (table_->model())
    UpdateListViewCache(0, table_->model()->RowCount(), true);

  if (table_->type() == ICON_AND_TEXT) {
    HIMAGELIST image_list =
        ImageList_Create(kImageSize, kImageSize, ILC_COLOR32, 2, 2);
    // We create 2 phony images because we are going to switch images at every
    // refresh in order to force a refresh of the icon area (somehow the clip
    // rect does not include the icon).
    gfx::CanvasSkia canvas(kImageSize, kImageSize, false);
    // Make the background completely transparent.
    canvas.sk_canvas()->drawColor(SK_ColorBLACK, SkXfermode::kClear_Mode);
    {
      base::win::ScopedHICON empty_icon(
          IconUtil::CreateHICONFromSkBitmap(canvas.ExtractBitmap()));
      ImageList_AddIcon(image_list, empty_icon);
      ImageList_AddIcon(image_list, empty_icon);
    }
    ListView_SetImageList(native_view(), image_list, LVSIL_SMALL);
  }

  if (!table_->resizable_columns()) {
    // To disable the resizing of columns we'll filter the events happening on
    // the header. We also need to intercept the HDM_LAYOUT to size the header
    // for the Chrome headers.
    HWND header = ListView_GetHeader(native_view());
    DCHECK(header);
    SetWindowLongPtr(header, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    header_original_handler_ =
        ui::SetWindowProc(header, &NativeTableWin::TableHeaderWndProc);
  }

  SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  original_handler_ =
      ui::SetWindowProc(hwnd, &NativeTableWin::TableWndProc);

  // Bug 964884: detach the IME attached to this window.
  // We should attach IMEs only when we need to input CJK strings.
  ::ImmAssociateContextEx(hwnd, NULL, 0);

  UpdateContentOffset();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTableWin, private:

void NativeTableWin::Select(int model_row) {
  if (!native_view())
    return;

  DCHECK(model_row >= 0 && model_row < table_->model()->RowCount());
  SendMessage(native_view(), WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  ignore_listview_change_ = true;

  // Unselect everything.
  ClearSelection();

  // Select the specified item.
  SetSelectedState(model_row, true);
  SetFocusState(model_row, true);

  // Make it visible.
  ListView_EnsureVisible(native_view(), model_row, FALSE);
  ignore_listview_change_ = false;
  SendMessage(native_view(), WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
  if (table_->observer())
    table_->observer()->OnSelectionChanged();
}

void NativeTableWin::OnSelectedStateChanged() {
  if (!ignore_listview_change_ && table_->observer())
    table_->observer()->OnSelectionChanged();
}

void NativeTableWin::OnDoubleClick() {
  if (!ignore_listview_change_ && table_->observer())
    table_->observer()->OnDoubleClick();
}

void NativeTableWin::OnMiddleClick() {
  if (!ignore_listview_change_ && table_->observer())
    table_->observer()->OnMiddleClick();
}

bool NativeTableWin::OnKeyDown(ui::KeyboardCode virtual_keycode) {
  if (!ignore_listview_change_ && table_->observer())
    table_->observer()->OnKeyDown(virtual_keycode);
  return false;  // Let the key event be processed as ususal.
}

LRESULT NativeTableWin::OnCustomDraw(NMLVCUSTOMDRAW* draw_info) {
  switch (draw_info->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: {
      return CDRF_NOTIFYITEMDRAW;
    }
    case CDDS_ITEMPREPAINT: {
      // The list-view is about to paint an item, tell it we want to
      // notified when it paints every subitem.
      LRESULT r = CDRF_NOTIFYSUBITEMDRAW;
      if (table_->type() == ICON_AND_TEXT)
        r |= CDRF_NOTIFYPOSTPAINT;
      return r;
    }
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
      // TODO(jcampan): implement custom colors and fonts.
      return CDRF_DODEFAULT;
    }
    case CDDS_ITEMPOSTPAINT: {
      DCHECK(table_->type() == ICON_AND_TEXT);
      int view_index = static_cast<int>(draw_info->nmcd.dwItemSpec);
      // We get notifications for empty items, just ignore them.
      if (view_index >= table_->model()->RowCount())
        return CDRF_DODEFAULT;
      LRESULT r = CDRF_DODEFAULT;
      // First let's take care of painting the right icon.
      if (table_->type() == ICON_AND_TEXT) {
        SkBitmap image = table_->model()->GetIcon(view_index);
        if (!image.isNull()) {
          // Get the rect that holds the icon.
          RECT icon_rect, client_rect;
          if (ListView_GetItemRect(native_view(), view_index, &icon_rect,
                                   LVIR_ICON) &&
              GetClientRect(native_view(), &client_rect)) {
            RECT intersection;
            // Client rect includes the header but we need to make sure we don't
            // paint into it.
            client_rect.top += content_offset_;
            // Make sure the region need to paint is visible.
            if (IntersectRect(&intersection, &icon_rect, &client_rect)) {
              gfx::CanvasSkia canvas(icon_rect.right - icon_rect.left,
                                     icon_rect.bottom - icon_rect.top, false);

              // It seems the state in nmcd.uItemState is not correct.
              // We'll retrieve it explicitly.
              int selected = ListView_GetItemState(
                  native_view(), view_index, LVIS_SELECTED | LVIS_DROPHILITED);
              bool drop_highlight = ((selected & LVIS_DROPHILITED) != 0);
              int bg_color_index;
              if (!IsEnabled())
                bg_color_index = COLOR_3DFACE;
              else if (drop_highlight)
                bg_color_index = COLOR_HIGHLIGHT;
              else if (selected)
                bg_color_index = HasFocus() ? COLOR_HIGHLIGHT : COLOR_3DFACE;
              else
                bg_color_index = COLOR_WINDOW;
              // NOTE: This may be invoked without the ListView filling in the
              // background (or rather windows paints background, then invokes
              // this twice). As such, we always fill in the background.
              canvas.sk_canvas()->drawColor(
                  skia::COLORREFToSkColor(GetSysColor(bg_color_index)),
                  SkXfermode::kSrc_Mode);
              // + 1 for padding (we declared the image as 18x18 in the list-
              // view when they are 16x16 so we get an extra pixel of padding).
              canvas.DrawBitmapInt(image, 0, 0,
                                   image.width(), image.height(),
                                   1, 1,
                                   gfx::kFaviconSize, gfx::kFaviconSize, true);

              // Only paint the visible region of the icon.
              RECT to_draw = { intersection.left - icon_rect.left,
                               intersection.top - icon_rect.top,
                               0, 0 };
              to_draw.right = to_draw.left +
                              (intersection.right - intersection.left);
              to_draw.bottom = to_draw.top +
                              (intersection.bottom - intersection.top);
              skia::DrawToNativeContext(canvas.sk_canvas(), draw_info->nmcd.hdc,
                                        intersection.left, intersection.top,
                                        &to_draw);
              r = CDRF_SKIPDEFAULT;
            }
          }
        }
      }
      return r;
    }
    default:
      return CDRF_DODEFAULT;
  }
}

void NativeTableWin::UpdateListViewCache(int start, int length, bool add) {
  LVITEM item = {0};
  int start_column = 0;
  int max_row = start + length;
  if (add) {
    item.mask |= LVIF_PARAM;
    for (int i = start; i < max_row; ++i) {
      item.iItem = i;
      item.lParam = i;
      ignore_listview_change_ = true;
      ListView_InsertItem(native_view(), &item);
      ignore_listview_change_ = false;
    }
  }

  memset(&item, 0, sizeof(LVITEM));
  item.stateMask = 0;
  item.mask = LVIF_TEXT;
  if (table_->type() == ICON_AND_TEXT)
    item.mask |= LVIF_IMAGE;

  for (size_t j = start_column; j < table_->GetVisibleColumnCount(); ++j) {
    ui::TableColumn col = table_->GetVisibleColumnAt(j);
    int max_text_width = ListView_GetStringWidth(native_view(),
                                                 col.title.c_str());
    for (int i = start; i < max_row; ++i) {
      item.iItem = i;
      item.iSubItem = j;
      string16 text = table_->model()->GetText(i, col.id);
      item.pszText = const_cast<LPWSTR>(text.c_str());
      item.iImage = 0;
      ListView_SetItem(native_view(), &item);

      // Compute width in px, using current font.
      int string_width = ListView_GetStringWidth(native_view(), item.pszText);
      // The width of an icon belongs to the first column.
      if (j == 0 && table_->type() == ICON_AND_TEXT)
        string_width += kListViewIconWidthAndPadding;
      max_text_width = std::max(string_width, max_text_width);
    }

    // ListView_GetStringWidth must be padded or else truncation will occur
    // (MSDN). 15px matches the Win32/LVSCW_AUTOSIZE_USEHEADER behavior.
    max_text_width += kListViewTextPadding;

    // Protect against partial update.
    if (max_text_width > col.min_visible_width ||
        (start == 0 && length == table_->model()->RowCount())) {
      col.min_visible_width = max_text_width;
    }
  }
}

void NativeTableWin::UpdateContentOffset() {
  content_offset_ = 0;

  if (!native_view())
    return;

  HWND header = ListView_GetHeader(native_view());
  if (!header)
    return;

  POINT origin = {0, 0};
  MapWindowPoints(header, native_view(), &origin, 1);

  RECT header_bounds;
  GetWindowRect(header, &header_bounds);

  content_offset_ = origin.y + header_bounds.bottom - header_bounds.top;
}

static int GetViewIndexFromMouseEvent(HWND window, LPARAM l_param) {
  int x = GET_X_LPARAM(l_param);
  int y = GET_Y_LPARAM(l_param);
  LVHITTESTINFO hit_info = {0};
  hit_info.pt.x = x;
  hit_info.pt.y = y;
  return ListView_HitTest(window, &hit_info);
}

// static
LRESULT CALLBACK NativeTableWin::TableWndProc(HWND window,
                                              UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param) {
  NativeTableWin* native_table_win = reinterpret_cast<NativeTableWin*>(
      GetWindowLongPtr(window, GWLP_USERDATA));
  TableView2* table = native_table_win->table_;

  // Is the mouse down on the table?
  static bool in_mouse_down = false;
  // Should we select on mouse up?
  static bool select_on_mouse_up = false;

  // If the mouse is down, this is the location of the mouse down message.
  static int mouse_down_x, mouse_down_y;

  switch (message) {
    case WM_CONTEXTMENU: {
      // This addresses two problems seen with context menus in right to left
      // locales:
      // 1. The mouse coordinates in the l_param were occasionally wrong in
      //    weird ways. This is most often seen when right clicking on the
      //    list-view twice in a row.
      // 2. Right clicking on the icon would show the scrollbar menu.
      //
      // As a work around this uses the position of the cursor and ignores
      // the position supplied in the l_param.
      if (base::i18n::IsRTL() &&
          (GET_X_LPARAM(l_param) != -1 || GET_Y_LPARAM(l_param) != -1)) {
        POINT screen_point;
        GetCursorPos(&screen_point);
        POINT table_point = screen_point;
        RECT client_rect;
        if (ScreenToClient(window, &table_point) &&
            GetClientRect(window, &client_rect) &&
            PtInRect(&client_rect, table_point)) {
          // The point is over the client area of the table, handle it ourself.
          // But first select the row if it isn't already selected.
          LVHITTESTINFO hit_info = {0};
          hit_info.pt.x = table_point.x;
          hit_info.pt.y = table_point.y;
          int view_index = ListView_HitTest(window, &hit_info);
          // TODO(jcampan): fix code below
          // if (view_index != -1 && table->IsItemSelected(view_index))
          //  table->Select(view_index);
          // table->OnContextMenu(screen_point);
          return 0;  // So that default processing doesn't occur.
        }
      }
      // else case: default handling is fine, so break and let the default
      // handler service the request (which will likely calls us back with
      // OnContextMenu).
      break;
    }

    case WM_CANCELMODE: {
      if (in_mouse_down) {
        in_mouse_down = false;
        return 0;
      }
      break;
    }

    case WM_ERASEBKGND:
      // We make WM_ERASEBKGND do nothing (returning 1 indicates we handled
      // the request). We do this so that the table view doesn't flicker during
      // resizing.
      return 1;

    case WM_KEYDOWN: {
      if (!table->single_selection() && w_param == 'A' &&
          GetKeyState(VK_CONTROL) < 0 && table->model()->RowCount() > 0) {
        // Select everything.
        ListView_SetItemState(window, -1, LVIS_SELECTED, LVIS_SELECTED);
        // And make the first row focused.
        ListView_SetItemState(window, 0, LVIS_FOCUSED, LVIS_FOCUSED);
        return 0;
      } else if (w_param == VK_DELETE && table->observer()) {
        table->observer()->OnTableView2Delete(table);
        return 0;
      }
      // else case: fall through to default processing.
      break;
    }

    case WM_LBUTTONDBLCLK: {
      if (w_param == MK_LBUTTON)
        native_table_win->OnDoubleClick();
      return 0;
    }

    case WM_MBUTTONDOWN: {
      if (w_param == MK_MBUTTON) {
        int view_index = GetViewIndexFromMouseEvent(window, l_param);
        if (view_index != -1) {
          // Clear all and select the row that was middle clicked.
          native_table_win->Select(view_index);
          native_table_win->OnMiddleClick();
        }
      }
      return 0;
    }

    case WM_LBUTTONUP: {
      if (in_mouse_down) {
        in_mouse_down = false;
        ReleaseCapture();
        ::SetFocus(window);
        if (select_on_mouse_up) {
          int view_index = GetViewIndexFromMouseEvent(window, l_param);
          if (view_index != -1)
            native_table_win->Select(view_index);
        }
        return 0;
      }
      break;
    }

    case WM_LBUTTONDOWN: {
      // ListView treats clicking on an area outside the text of a column as
      // drag to select. This is confusing when the selection is shown across
      // the whole row. For this reason we override the default handling for
      // mouse down/move/up and treat the whole row as draggable. That is, no
      // matter where you click in the row we'll attempt to start dragging.
      //
      // Only do custom mouse handling if no other mouse buttons are down.
      if ((w_param | (MK_LBUTTON | MK_CONTROL | MK_SHIFT)) ==
          (MK_LBUTTON | MK_CONTROL | MK_SHIFT)) {
        if (in_mouse_down)
          return 0;

        int view_index = GetViewIndexFromMouseEvent(window, l_param);
        if (view_index != -1) {
          native_table_win->ignore_listview_change_ = true;
          in_mouse_down = true;
          select_on_mouse_up = false;
          mouse_down_x = GET_X_LPARAM(l_param);
          mouse_down_y = GET_Y_LPARAM(l_param);
          bool select = true;
          if (w_param & MK_CONTROL) {
            select = false;
            if (!native_table_win->IsRowSelected(view_index)) {
              if (table->single_selection()) {
                // Single selection mode and the row isn't selected, select
                // only it.
                native_table_win->Select(view_index);
              } else {
                // Not single selection, add this row to the selection.
                native_table_win->SetSelectedState(view_index, true);
              }
            } else {
              // Remove this row from the selection.
              native_table_win->SetSelectedState(view_index, false);
            }
            ListView_SetSelectionMark(window, view_index);
          } else if (!table->single_selection() && w_param & MK_SHIFT) {
            int mark_view_index = ListView_GetSelectionMark(window);
            if (mark_view_index != -1) {
              // Unselect everything.
              ListView_SetItemState(window, -1, 0, LVIS_SELECTED);
              select = false;

              // Select from mark to mouse down location.
              for (int i = std::min(view_index, mark_view_index),
                   max_i = std::max(view_index, mark_view_index); i <= max_i;
                   ++i) {
                native_table_win->SetSelectedState(i, true);
              }
            }
          }
          // Make the row the user clicked on the focused row.
          ListView_SetItemState(window, view_index, LVIS_FOCUSED,
                                LVIS_FOCUSED);
          if (select) {
            if (!native_table_win->IsRowSelected(view_index)) {
              // Clear all.
              ListView_SetItemState(window, -1, 0, LVIS_SELECTED);
              // And select the row the user clicked on.
              native_table_win->SetSelectedState(view_index, true);
            } else {
              // The item is already selected, don't clear the state right away
              // in case the user drags. Instead wait for mouse up, then only
              // select the row the user clicked on.
              select_on_mouse_up = true;
            }
            ListView_SetSelectionMark(window, view_index);
          }
          native_table_win->ignore_listview_change_ = false;
          table->observer()->OnSelectionChanged();
          SetCapture(window);
          return 0;
        }
        // else case, continue on to default handler
      }
      break;
    }

    case WM_MOUSEMOVE: {
      if (in_mouse_down) {
        int x = GET_X_LPARAM(l_param);
        int y = GET_Y_LPARAM(l_param);
        if (View::ExceededDragThreshold(x - mouse_down_x, y - mouse_down_y)) {
          // We're about to start drag and drop, which results in no mouse up.
          // Release capture and reset state.
          ReleaseCapture();
          in_mouse_down = false;

          NMLISTVIEW details;
          memset(&details, 0, sizeof(details));
          details.hdr.code = LVN_BEGINDRAG;
          SendMessage(::GetParent(window), WM_NOTIFY, 0,
                      reinterpret_cast<LPARAM>(&details));
        }
        return 0;
      }
      break;
    }

    default:
      break;
  }

  DCHECK(native_table_win->original_handler_);
  return CallWindowProc(native_table_win->original_handler_, window, message,
                        w_param, l_param);
}

LRESULT CALLBACK NativeTableWin::TableHeaderWndProc(HWND window, UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  NativeTableWin* native_table_win = reinterpret_cast<NativeTableWin*>(
      GetWindowLongPtr(window, GWLP_USERDATA));

  switch (message) {
    case WM_SETCURSOR:
      if (!native_table_win->table_->resizable_columns())
        // Prevents the cursor from changing to the resize cursor.
        return TRUE;
      break;
    // TODO(jcampan): we should also block single click messages on the
    // separator as right now columns can still be resized.
    case WM_LBUTTONDBLCLK:
      if (!native_table_win->table_->resizable_columns())
        // Prevents the double-click on the column separator from auto-resizing
        // the column.
        return TRUE;
      break;
    default:
      break;
  }
  DCHECK(native_table_win->header_original_handler_);
  return CallWindowProc(native_table_win->header_original_handler_,
                        window, message, w_param, l_param);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTableWrapper, public:

// static
NativeTableWrapper* NativeTableWrapper::CreateNativeWrapper(TableView2* table) {
  return new NativeTableWin(table);
}

}  // namespace views
