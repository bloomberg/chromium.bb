// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/table/table_view.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/string_util.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/models/table_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font.h"
#include "ui/gfx/icon_util.h"
#include "views/controls/native/native_view_host.h"
#include "views/controls/table/table_view_observer.h"

using ui::TableColumn;

namespace {

int GetViewIndexFromPoint(HWND window, const gfx::Point& p) {
  LVHITTESTINFO hit_info = {0};
  hit_info.pt.x = p.x();
  hit_info.pt.y = p.y();
  return ListView_HitTest(window, &hit_info);
}

}  // namespace

namespace views {

// Added to column width to prevent truncation.
const int kListViewTextPadding = 15;
// Additional column width necessary if column has icons.
const int kListViewIconWidthAndPadding = 18;

// TableView ------------------------------------------------------------------

// static
const int TableView::kImageSize = 18;

TableView::TableView(TableModel* model,
                     const std::vector<TableColumn>& columns,
                     TableTypes table_type,
                     bool single_selection,
                     bool resizable_columns,
                     bool autosize_columns)
    : model_(model),
      table_view_observer_(NULL),
      visible_columns_(),
      all_columns_(),
      column_count_(static_cast<int>(columns.size())),
      table_type_(table_type),
      single_selection_(single_selection),
      ignore_listview_change_(false),
      custom_colors_enabled_(false),
      autosize_columns_(autosize_columns),
      resizable_columns_(resizable_columns),
      list_view_(NULL),
      header_original_handler_(NULL),
      original_handler_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(table_view_wrapper_(this)),
      custom_cell_font_(NULL),
      content_offset_(0) {
  for (std::vector<TableColumn>::const_iterator i = columns.begin();
      i != columns.end(); ++i) {
    AddColumn(*i);
    visible_columns_.push_back(i->id);
  }
}

TableView::~TableView() {
  if (list_view_) {
    if (model_)
      model_->SetObserver(NULL);
  }
  if (custom_cell_font_)
    DeleteObject(custom_cell_font_);
}

void TableView::SetModel(TableModel* model) {
  if (model == model_)
    return;

  if (list_view_ && model_)
    model_->SetObserver(NULL);
  model_ = model;
  if (list_view_ && model_)
    model_->SetObserver(this);
  if (list_view_)
    OnModelChanged();
}

void TableView::SetSortDescriptors(const SortDescriptors& sort_descriptors) {
  if (!sort_descriptors_.empty()) {
    ResetColumnSortImage(sort_descriptors_[0].column_id,
                         NO_SORT);
  }
  sort_descriptors_ = sort_descriptors;
  if (!sort_descriptors_.empty()) {
    ResetColumnSortImage(
        sort_descriptors_[0].column_id,
        sort_descriptors_[0].ascending ? ASCENDING_SORT : DESCENDING_SORT);
  }
  if (!list_view_)
    return;

  // For some reason we have to turn off/on redraw, otherwise the display
  // isn't updated when done.
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);

  UpdateItemsLParams(0, 0);

  SortItemsAndUpdateMapping();

  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
}

int TableView::RowCount() const {
  if (!list_view_)
    return 0;
  return ListView_GetItemCount(list_view_);
}

int TableView::SelectedRowCount() {
  if (!list_view_)
    return 0;
  return ListView_GetSelectedCount(list_view_);
}

void TableView::Select(int model_row) {
  if (!list_view_)
    return;

  DCHECK(model_row >= 0 && model_row < RowCount());
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  ignore_listview_change_ = true;

  // Unselect everything.
  ListView_SetItemState(list_view_, -1, 0, LVIS_SELECTED);

  // Select the specified item.
  int view_row = ModelToView(model_row);
  ListView_SetItemState(list_view_, view_row, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);

  // Make it visible.
  ListView_EnsureVisible(list_view_, view_row, FALSE);
  ignore_listview_change_ = false;
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
  if (table_view_observer_)
    table_view_observer_->OnSelectionChanged();
}

void TableView::SetSelectedState(int model_row, bool state) {
  if (!list_view_)
    return;

  DCHECK(model_row >= 0 && model_row < RowCount());

  ignore_listview_change_ = true;

  // Select the specified item.
  ListView_SetItemState(list_view_, ModelToView(model_row),
                        state ? LVIS_SELECTED : 0,  LVIS_SELECTED);

  ignore_listview_change_ = false;
}

void TableView::SetFocusOnItem(int model_row) {
  if (!list_view_)
    return;

  DCHECK(model_row >= 0 && model_row < RowCount());

  ignore_listview_change_ = true;

  // Set the focus to the given item.
  ListView_SetItemState(list_view_, ModelToView(model_row), LVIS_FOCUSED,
                        LVIS_FOCUSED);

  ignore_listview_change_ = false;
}

int TableView::FirstSelectedRow() {
  if (!list_view_)
    return -1;

  int view_row = ListView_GetNextItem(list_view_, -1, LVNI_ALL | LVIS_SELECTED);
  return view_row == -1 ? -1 : ViewToModel(view_row);
}

bool TableView::IsItemSelected(int model_row) {
  if (!list_view_)
    return false;

  DCHECK(model_row >= 0 && model_row < RowCount());
  return (ListView_GetItemState(list_view_, ModelToView(model_row),
                                LVIS_SELECTED) == LVIS_SELECTED);
}

bool TableView::ItemHasTheFocus(int model_row) {
  if (!list_view_)
    return false;

  DCHECK(model_row >= 0 && model_row < RowCount());
  return (ListView_GetItemState(list_view_, ModelToView(model_row),
                                LVIS_FOCUSED) == LVIS_FOCUSED);
}

TableView::iterator TableView::SelectionBegin() {
  return TableView::iterator(this, LastSelectedViewIndex());
}

TableView::iterator TableView::SelectionEnd() {
  return TableView::iterator(this, -1);
}

void TableView::OnItemsChanged(int start, int length) {
  if (!list_view_)
    return;

  if (length == -1) {
    DCHECK_GE(start, 0);
    length = model_->RowCount() - start;
  }
  int row_count = RowCount();
  DCHECK(start >= 0 && length > 0 && start + length <= row_count);
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  if (table_type_ == ICON_AND_TEXT) {
    // The redraw event does not include the icon in the clip rect, preventing
    // our icon from being repainted. So far the only way I could find around
    // this is to change the image for the item. Even if the image does not
    // exist, it causes the clip rect to include the icon's bounds so we can
    // paint it in the post paint event.
    LVITEM lv_item;
    memset(&lv_item, 0, sizeof(LVITEM));
    lv_item.mask = LVIF_IMAGE;
    for (int i = start; i < start + length; ++i) {
      // Retrieve the current icon index.
      lv_item.iItem = ModelToView(i);
      BOOL r = ListView_GetItem(list_view_, &lv_item);
      DCHECK(r);
      // Set the current icon index to the other image.
      lv_item.iImage = (lv_item.iImage + 1) % 2;
      DCHECK((lv_item.iImage == 0) || (lv_item.iImage == 1));
      r = ListView_SetItem(list_view_, &lv_item);
      DCHECK(r);
    }
  }
  UpdateListViewCache(start, length, false);
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
}

void TableView::OnModelChanged() {
  if (!list_view_)
    return;

  UpdateGroups();

  int current_row_count = ListView_GetItemCount(list_view_);
  if (current_row_count > 0)
    OnItemsRemoved(0, current_row_count);
  if (model_ && model_->RowCount())
    OnItemsAdded(0, model_->RowCount());
}

void TableView::OnItemsAdded(int start, int length) {
  if (!list_view_)
    return;

  DCHECK(start >= 0 && length > 0 && start <= RowCount());
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  UpdateListViewCache(start, length, true);
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
}

void TableView::OnItemsRemoved(int start, int length) {
  if (!list_view_)
    return;

  if (start < 0 || length < 0 || start + length > RowCount()) {
    NOTREACHED();
    return;
  }

  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);

  bool had_selection = (SelectedRowCount() > 0);
  int old_row_count = RowCount();
  if (start == 0 && length == RowCount()) {
    // Everything was removed.
    ListView_DeleteAllItems(list_view_);
    view_to_model_.reset(NULL);
    model_to_view_.reset(NULL);
  } else {
    // Only a portion of the data was removed.
    if (is_sorted()) {
      int new_row_count = model_->RowCount();
      std::vector<int> view_items_to_remove;
      view_items_to_remove.reserve(length);
      // Iterate through the elements, updating the view_to_model_ mapping
      // as well as collecting the rows that need to be deleted.
      for (int i = 0, removed_count = 0; i < old_row_count; ++i) {
        int model_index = ViewToModel(i);
        if (model_index >= start) {
          if (model_index < start + length) {
            // This item was removed.
            view_items_to_remove.push_back(i);
            model_index = -1;
          } else {
            model_index -= length;
          }
        }
        if (model_index >= 0) {
          view_to_model_[i - static_cast<int>(view_items_to_remove.size())] =
              model_index;
        }
      }

      // Update the model_to_view mapping from the updated view_to_model
      // mapping.
      for (int i = 0; i < new_row_count; ++i)
        model_to_view_[view_to_model_[i]] = i;

      // And finally delete the items. We do this backwards as the items were
      // added ordered smallest to largest.
      for (int i = length - 1; i >= 0; --i)
        ListView_DeleteItem(list_view_, view_items_to_remove[i]);
    } else {
      for (int i = 0; i < length; ++i)
        ListView_DeleteItem(list_view_, start);
    }
  }

  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);

  // If the row count goes to zero and we had a selection LVN_ITEMCHANGED isn't
  // invoked, so we handle it here.
  //
  // When the model is set to NULL all the rows are removed. We don't notify
  // the delegate in this case as setting the model to NULL is usually done as
  // the last step before being deleted and callers shouldn't have to deal with
  // getting a selection change when the model is being reset.
  if (model_ && table_view_observer_ && had_selection && RowCount() == 0)
    table_view_observer_->OnSelectionChanged();
}

void TableView::AddColumn(const TableColumn& col) {
  DCHECK_EQ(0u, all_columns_.count(col.id));
  all_columns_[col.id] = col;
}

void TableView::SetColumns(const std::vector<TableColumn>& columns) {
  // Remove the currently visible columns.
  while (!visible_columns_.empty())
    SetColumnVisibility(visible_columns_.front(), false);

  all_columns_.clear();
  for (std::vector<TableColumn>::const_iterator i = columns.begin();
       i != columns.end(); ++i) {
    AddColumn(*i);
  }

  // Remove any sort descriptors that are no longer valid.
  SortDescriptors sort = sort_descriptors();
  for (SortDescriptors::iterator i = sort.begin(); i != sort.end();) {
    if (all_columns_.count(i->column_id) == 0)
      i = sort.erase(i);
    else
      ++i;
  }
  sort_descriptors_ = sort;
}

void TableView::OnColumnsChanged() {
  column_count_ = static_cast<int>(visible_columns_.size());
  ResetColumnSizes();
}

void TableView::SetColumnVisibility(int id, bool is_visible) {
  bool changed = false;
  for (std::vector<int>::iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    if (*i == id) {
      if (is_visible) {
        // It's already visible, bail out early.
        return;
      } else {
        int index = static_cast<int>(i - visible_columns_.begin());
        // This could be called before the native list view has been created
        // (in CreateNativeControl, called when the view is added to a
        // Widget). In that case since the column is not in
        // visible_columns_ it will not be added later on when it is created.
        if (list_view_)
          SendMessage(list_view_, LVM_DELETECOLUMN, index, 0);
        visible_columns_.erase(i);
        changed = true;
        break;
      }
    }
  }
  if (is_visible) {
    visible_columns_.push_back(id);
    TableColumn& column = all_columns_[id];
    InsertColumn(column, column_count_);
    if (column.min_visible_width == 0) {
      // ListView_GetStringWidth must be padded or else truncation will occur.
      column.min_visible_width = ListView_GetStringWidth(list_view_,
                                                         column.title.c_str()) +
                                 kListViewTextPadding;
    }
    changed = true;
  }
  if (changed)
    OnColumnsChanged();
}

void TableView::SetVisibleColumns(const std::vector<int>& columns) {
  size_t old_count = visible_columns_.size();
  size_t new_count = columns.size();
  // remove the old columns
  if (list_view_) {
    for (std::vector<int>::reverse_iterator i = visible_columns_.rbegin();
         i != visible_columns_.rend(); ++i) {
      int index = static_cast<int>(i - visible_columns_.rend());
      SendMessage(list_view_, LVM_DELETECOLUMN, index, 0);
    }
  }
  visible_columns_ = columns;
  // Insert the new columns.
  if (list_view_) {
    for (std::vector<int>::iterator i = visible_columns_.begin();
         i != visible_columns_.end(); ++i) {
      int index = static_cast<int>(i - visible_columns_.end());
      InsertColumn(all_columns_[*i], index);
    }
  }
  OnColumnsChanged();
}

bool TableView::IsColumnVisible(int id) const {
  for (std::vector<int>::const_iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i)
    if (*i == id) {
      return true;
    }
  return false;
}

const TableColumn& TableView::GetColumnAtPosition(int pos) {
  return all_columns_[visible_columns_[pos]];
}

bool TableView::HasColumn(int id) {
  return all_columns_.count(id) > 0;
}

gfx::Point TableView::GetKeyboardContextMenuLocation() {
  int first_selected = FirstSelectedRow();
  int y = height() / 2;
  if (first_selected != -1) {
    RECT cell_bounds;
    RECT client_rect;
    if (ListView_GetItemRect(GetNativeControlHWND(), first_selected,
                             &cell_bounds, LVIR_BOUNDS) &&
        GetClientRect(GetNativeControlHWND(), &client_rect) &&
        cell_bounds.bottom >= 0 && cell_bounds.bottom < client_rect.bottom) {
      y = cell_bounds.bottom;
    }
  }
  gfx::Point screen_loc(0, y);
  if (base::i18n::IsRTL())
    screen_loc.set_x(width());
  ConvertPointToScreen(this, &screen_loc);
  return screen_loc;
}

void TableView::SetCustomColorsEnabled(bool custom_colors_enabled) {
  custom_colors_enabled_ = custom_colors_enabled;
}

bool TableView::GetCellColors(int model_row,
                              int column,
                              ItemColor* foreground,
                              ItemColor* background,
                              LOGFONT* logfont) {
  return false;
}

// static
LRESULT CALLBACK TableView::TableWndProc(HWND window,
                                         UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  TableView* table_view = reinterpret_cast<TableViewWrapper*>(
      GetWindowLongPtr(window, GWLP_USERDATA))->table_view;

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
          int view_index =
              GetViewIndexFromPoint(window, gfx::Point(table_point));
          if (view_index != -1) {
            int model_index = table_view->ViewToModel(view_index);
            if (!table_view->IsItemSelected(model_index))
              table_view->Select(model_index);
          }
          table_view->OnContextMenu(screen_point);
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

    case WM_PAINT: {
      LRESULT result = CallWindowProc(table_view->original_handler_, window,
                                      message, w_param, l_param);
      table_view->PostPaint();
      return result;
    }

    case WM_KEYDOWN: {
      if (!table_view->single_selection_ && w_param == 'A' &&
          GetKeyState(VK_CONTROL) < 0 && table_view->RowCount() > 0) {
        // Select everything.
        ListView_SetItemState(window, -1, LVIS_SELECTED, LVIS_SELECTED);
        // And make the first row focused.
        ListView_SetItemState(window, 0, LVIS_FOCUSED, LVIS_FOCUSED);
        return 0;
      } else if (w_param == VK_DELETE && table_view->table_view_observer_) {
        table_view->table_view_observer_->OnTableViewDelete(table_view);
        return 0;
      }
      // else case: fall through to default processing.
      break;
    }

    case WM_LBUTTONDBLCLK: {
      if (w_param == MK_LBUTTON)
        table_view->OnDoubleClick();
      return 0;
    }

    case WM_MBUTTONDOWN: {
      if (w_param == MK_MBUTTON) {
        int view_index = GetViewIndexFromPoint(window, gfx::Point(l_param));
        if (view_index != -1) {
          int model_index = table_view->ViewToModel(view_index);
          // Clear all and select the row that was middle clicked.
          table_view->Select(model_index);
          table_view->OnMiddleClick();
        }
      }
      return 0;
    }

    case WM_LBUTTONUP: {
      if (in_mouse_down) {
        in_mouse_down = false;
        ReleaseCapture();
        SetFocus(window);
        if (select_on_mouse_up) {
          int view_index = GetViewIndexFromPoint(window, gfx::Point(l_param));
          if (view_index != -1)
            table_view->Select(table_view->ViewToModel(view_index));
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

        int view_index = GetViewIndexFromPoint(window, gfx::Point(l_param));
        if (view_index != -1) {
          table_view->ignore_listview_change_ = true;
          in_mouse_down = true;
          select_on_mouse_up = false;
          mouse_down_x = GET_X_LPARAM(l_param);
          mouse_down_y = GET_Y_LPARAM(l_param);
          int model_index = table_view->ViewToModel(view_index);
          bool select = true;
          if (w_param & MK_CONTROL) {
            select = false;
            if (!table_view->IsItemSelected(model_index)) {
              if (table_view->single_selection_) {
                // Single selection mode and the row isn't selected, select
                // only it.
                table_view->Select(model_index);
              } else {
                // Not single selection, add this row to the selection.
                table_view->SetSelectedState(model_index, true);
              }
            } else {
              // Remove this row from the selection.
              table_view->SetSelectedState(model_index, false);
            }
            ListView_SetSelectionMark(window, view_index);
          } else if (!table_view->single_selection_ && w_param & MK_SHIFT) {
            int mark_view_index = ListView_GetSelectionMark(window);
            if (mark_view_index != -1) {
              // Unselect everything.
              ListView_SetItemState(window, -1, 0, LVIS_SELECTED);
              select = false;
              if (!table_view->SelectMultiple(view_index, mark_view_index)) {
                // Selection spans group boundary - reset selection to current.
                table_view->SetSelectedState(model_index, true);
                ListView_SetSelectionMark(window, view_index);
              }
            }
          }
          // Make the row the user clicked on the focused row.
          ListView_SetItemState(window, view_index, LVIS_FOCUSED,
                                LVIS_FOCUSED);
          if (select) {
            if (!table_view->IsItemSelected(model_index)) {
              // Clear all.
              ListView_SetItemState(window, -1, 0, LVIS_SELECTED);
              // And select the row the user clicked on.
              table_view->SetSelectedState(model_index, true);
            } else {
              // The item is already selected, don't clear the state right away
              // in case the user drags. Instead wait for mouse up, then only
              // select the row the user clicked on.
              select_on_mouse_up = true;
            }
            ListView_SetSelectionMark(window, view_index);
          }
          table_view->ignore_listview_change_ = false;
          table_view->OnSelectedStateChanged();
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
  DCHECK(table_view->original_handler_);
  return CallWindowProc(table_view->original_handler_, window, message, w_param,
                        l_param);
}

LRESULT CALLBACK TableView::TableHeaderWndProc(HWND window, UINT message,
                                               WPARAM w_param, LPARAM l_param) {
  TableView* table_view = reinterpret_cast<TableViewWrapper*>(
      GetWindowLongPtr(window, GWLP_USERDATA))->table_view;

  switch (message) {
    case WM_SETCURSOR:
      if (!table_view->resizable_columns_)
        // Prevents the cursor from changing to the resize cursor.
        return TRUE;
      break;
    case WM_LBUTTONDBLCLK:
      if (!table_view->resizable_columns_)
        // Prevents the double-click on the column separator from auto-resizing
        // the column.
        return TRUE;
      break;
    default:
      break;
  }
  DCHECK(table_view->header_original_handler_);
  return CallWindowProc(table_view->header_original_handler_,
                        window, message, w_param, l_param);
}

HWND TableView::CreateNativeControl(HWND parent_container) {
  int style = WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS;
  if (single_selection_)
    style |= LVS_SINGLESEL;
  // If there's only one column and the title string is empty, don't show a
  // header.
  if (all_columns_.size() == 1) {
    std::map<int, TableColumn>::const_iterator first =
        all_columns_.begin();
    if (first->second.title.empty())
      style |= LVS_NOCOLUMNHEADER;
  }
  list_view_ = ::CreateWindowEx(WS_EX_CLIENTEDGE | GetAdditionalRTLStyle(),
                                WC_LISTVIEW,
                                L"",
                                style,
                                0, 0, width(), height(),
                                parent_container, NULL, NULL, NULL);

  // Reduce overdraw/flicker artifacts by double buffering.  Support tooltips
  // and display elided items completely on hover (see comments in OnNotify()
  // under LVN_GETINFOTIP).  Make the selection extend across the row.
  ListView_SetExtendedListViewStyle(list_view_,
      LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
  l10n_util::AdjustUIFontForWindow(list_view_);

  // Add the columns.
  for (std::vector<int>::iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    InsertColumn(all_columns_[*i],
                 static_cast<int>(i - visible_columns_.begin()));
  }

  if (model_)
    model_->SetObserver(this);

  UpdateGroups();

  // Set the # of rows.
  if (model_)
    UpdateListViewCache(0, model_->RowCount(), true);

  if (table_type_ == ICON_AND_TEXT) {
    HIMAGELIST image_list =
        ImageList_Create(kImageSize, kImageSize, ILC_COLOR32, 2, 2);
    // We create 2 phony images because we are going to switch images at every
    // refresh in order to force a refresh of the icon area (somehow the clip
    // rect does not include the icon).
    gfx::CanvasSkia canvas(kImageSize, kImageSize, false);
    // Make the background completely transparent.
    canvas.drawColor(SK_ColorBLACK, SkXfermode::kClear_Mode);
    HICON empty_icon =
        IconUtil::CreateHICONFromSkBitmap(canvas.ExtractBitmap());
    ImageList_AddIcon(image_list, empty_icon);
    ImageList_AddIcon(image_list, empty_icon);
    DeleteObject(empty_icon);
    ListView_SetImageList(list_view_, image_list, LVSIL_SMALL);
  }

  if (!resizable_columns_) {
    // To disable the resizing of columns we'll filter the events happening on
    // the header. We also need to intercept the HDM_LAYOUT to size the header
    // for the Chrome headers.
    HWND header = ListView_GetHeader(list_view_);
    DCHECK(header);
    SetWindowLongPtr(header, GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(&table_view_wrapper_));
    header_original_handler_ = ui::SetWindowProc(header,
        &TableView::TableHeaderWndProc);
  }

  SetWindowLongPtr(list_view_, GWLP_USERDATA,
      reinterpret_cast<LONG_PTR>(&table_view_wrapper_));
  original_handler_ =
      ui::SetWindowProc(list_view_, &TableView::TableWndProc);

  // Bug 964884: detach the IME attached to this window.
  // We should attach IMEs only when we need to input CJK strings.
  ::ImmAssociateContextEx(list_view_, NULL, 0);

  UpdateContentOffset();
  column_sizes_valid_ = false;

  return list_view_;
}

void TableView::ToggleSortOrder(int column_id) {
  SortDescriptors sort = sort_descriptors();
  if (!sort.empty() && sort[0].column_id == column_id) {
    sort[0].ascending = !sort[0].ascending;
  } else {
    SortDescriptor descriptor(column_id, true);
    sort.insert(sort.begin(), descriptor);
    if (sort.size() > 2) {
      // Only persist two sort descriptors.
      sort.resize(2);
    }
  }
  SetSortDescriptors(sort);
}

void TableView::UpdateItemsLParams(int start, int length) {
  LVITEM item;
  memset(&item, 0, sizeof(LVITEM));
  item.mask = LVIF_PARAM;
  int row_count = RowCount();
  for (int i = 0; i < row_count; ++i) {
    item.iItem = i;
    int model_index = ViewToModel(i);
    if (length > 0 && model_index >= start)
      model_index += length;
    item.lParam = static_cast<LPARAM>(model_index);
    ListView_SetItem(list_view_, &item);
  }
}

void TableView::SortItemsAndUpdateMapping() {
  if (!is_sorted()) {
    ListView_SortItems(list_view_, &TableView::NaturalSortFunc, this);
    view_to_model_.reset(NULL);
    model_to_view_.reset(NULL);
    return;
  }

  PrepareForSort();

  // Sort the items.
  ListView_SortItems(list_view_, &TableView::SortFunc, this);

  model_->ClearCollator();

  // Update internal mapping to match how items were actually sorted.
  int row_count = RowCount();
  model_to_view_.reset(new int[row_count]);
  view_to_model_.reset(new int[row_count]);
  LVITEM item;
  memset(&item, 0, sizeof(LVITEM));
  item.mask = LVIF_PARAM;
  for (int i = 0; i < row_count; ++i) {
    item.iItem = i;
    ListView_GetItem(list_view_, &item);
    int model_index = static_cast<int>(item.lParam);
    view_to_model_[i] = model_index;
    model_to_view_[model_index] = i;
  }
}

bool TableView::SelectMultiple(int view_index, int mark_view_index) {
  int group_id = 0;
  if (model_->HasGroups()) {
    group_id = model_->GetGroupID(ViewToModel(view_index));
    if (group_id != model_->GetGroupID(ViewToModel(mark_view_index))) {
      // User is trying to do a cross-group selection - bail out.
      return false;
    }
  }

  // Select from mark to mouse down location.
  for (int i = std::min(view_index, mark_view_index),
       max_i = std::max(view_index, mark_view_index); i <= max_i;
       ++i) {
    // Items between the view_index and mark_view_index are not necessarily in
    // the same group, so don't select anything outside the group the user
    // just clicked in.
    if (model_->HasGroups() &&
        model_->GetGroupID(ViewToModel(i)) != group_id) {
      continue;
    }
    SetSelectedState(ViewToModel(i), true);
  }
  return true;
}

// static
int CALLBACK TableView::SortFunc(LPARAM model_index_1_p,
                                 LPARAM model_index_2_p,
                                 LPARAM table_view_param) {
  int model_index_1 = static_cast<int>(model_index_1_p);
  int model_index_2 = static_cast<int>(model_index_2_p);
  TableView* table_view = reinterpret_cast<TableView*>(table_view_param);
  return table_view->CompareRows(model_index_1, model_index_2);
}

// static
int CALLBACK TableView::NaturalSortFunc(LPARAM model_index_1_p,
                                        LPARAM model_index_2_p,
                                        LPARAM table_view_param) {
  return model_index_1_p - model_index_2_p;
}

void TableView::ResetColumnSortImage(int column_id, SortDirection direction) {
  if (!list_view_ || column_id == -1)
    return;

  std::vector<int>::const_iterator i =
      std::find(visible_columns_.begin(), visible_columns_.end(), column_id);
  if (i == visible_columns_.end())
    return;

  HWND header = ListView_GetHeader(list_view_);
  if (!header)
    return;

  int column_index = static_cast<int>(i - visible_columns_.begin());
  HDITEM header_item;
  memset(&header_item, 0, sizeof(header_item));
  header_item.mask = HDI_FORMAT;
  Header_GetItem(header, column_index, &header_item);
  header_item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
  if (direction == ASCENDING_SORT)
    header_item.fmt |= HDF_SORTUP;
  else if (direction == DESCENDING_SORT)
    header_item.fmt |= HDF_SORTDOWN;
  Header_SetItem(header, column_index, &header_item);
}

void TableView::InsertColumn(const TableColumn& tc, int index) {
  if (!list_view_)
    return;

  LVCOLUMN column = { 0 };
  column.mask = LVCF_TEXT|LVCF_FMT;
  column.pszText = const_cast<LPWSTR>(tc.title.c_str());
  switch (tc.alignment) {
      case TableColumn::LEFT:
        column.fmt = LVCFMT_LEFT;
        break;
      case TableColumn::RIGHT:
        column.fmt = LVCFMT_RIGHT;
        break;
      case TableColumn::CENTER:
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
  SendMessage(list_view_, LVM_INSERTCOLUMN, index,
              reinterpret_cast<LPARAM>(&column));
  if (is_sorted() && sort_descriptors_[0].column_id == tc.id) {
    ResetColumnSortImage(
        tc.id,
        sort_descriptors_[0].ascending ? ASCENDING_SORT : DESCENDING_SORT);
  }
}

LRESULT TableView::OnNotify(int w_param, LPNMHDR hdr) {
  if (!model_)
    return 0;

  switch (hdr->code) {
    case NM_CUSTOMDRAW: {
      // Draw notification. dwDragState indicates the current stage of drawing.
      return OnCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW*>(hdr));
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
      if (!resizable_columns_)
        return TRUE;
      break;

    case NM_DBLCLK:
      OnDoubleClick();
      break;

    case LVN_COLUMNCLICK: {
      const TableColumn& column = GetColumnAtPosition(
          reinterpret_cast<NMLISTVIEW*>(hdr)->iSubItem);
      if (column.sortable)
        ToggleSortOrder(column.id);
      break;
    }

    case LVN_MARQUEEBEGIN:  // We don't want the marquee selection.
      return 1;

    case LVN_GETINFOTIP: {
      // This is called when the user hovers items in column zero.
      //   * If the text in this column is not fully visible, the dwFlags field
      //     will be set to 0, and pszText will contain the full text.  If you
      //     return without making any changes, this text will be displayed in a
      //     "labeltip" - a bubble that's overlaid (at the correct alignment!)
      //     on the item.  If you return with a different pszText, it will be
      //     displayed as a tooltip if nonempty.
      //   * Otherwise, dwFlags will be LVGIT_UNFOLDED and pszText will be
      //     empty.  On return, if pszText is nonempty, it will be displayed as
      //     a labeltip if dwFlags has been changed to 0 (even if it bears no
      //     resemblance to the item text), or as a tooltip otherwise.
      //
      // Once the tooltip for an item has been obtained, this will not be called
      // again until the user hovers a different item.  If after that the
      // original item is hovered a second time, this will be called.
      //
      // When the user hovers items in other columns, they will be "unfolded"
      // (displayed as labeltips) when necessary, but this function will never
      // be called.
      //
      // Changing the LVS_EX_INFOTIP extended style to LVS_EX_LABELTIP will
      // cause all of the above to be true except that this function will not be
      // called when dwFlags would be LVGIT_UNFOLDED.  Removing it entirely will
      // disable all of the above behavior.
      NMLVGETINFOTIP* info_tip = reinterpret_cast<NMLVGETINFOTIP*>(hdr);
      std::wstring tooltip =
          model_->GetTooltip(ViewToModel(info_tip->iItem));
      CHECK(info_tip->cchTextMax >= 2);
      if (tooltip.length() >= static_cast<size_t>(info_tip->cchTextMax)) {
        tooltip.erase(info_tip->cchTextMax - 2);  // Ellipsis + '\0'
        const wchar_t kEllipsis = L'\x2026';
        tooltip += kEllipsis;
      }
      if (!tooltip.empty())
        wcscpy_s(info_tip->pszText, tooltip.length() + 1, tooltip.c_str());
      return 1;
    }

    default:
      break;
  }
  return 0;
}

// Returns result, unless ascending is false in which case -result is returned.
static int SwapCompareResult(int result, bool ascending) {
  return ascending ? result : -result;
}

int TableView::CompareRows(int model_row1, int model_row2) {
  if (model_->HasGroups()) {
    // By default ListView sorts the elements regardless of groups. In such
    // a situation the groups display only the items they contain. This results
    // in the visual order differing from the item indices. I could not find
    // a way to iterate over the visual order in this situation. As a workaround
    // this forces the items to be sorted by groups as well, which means the
    // visual order matches the item indices.
    int g1 = model_->GetGroupID(model_row1);
    int g2 = model_->GetGroupID(model_row2);
    if (g1 != g2)
      return g1 - g2;
  }
  int sort_result = model_->CompareValues(
      model_row1, model_row2, sort_descriptors_[0].column_id);
  if (sort_result == 0 && sort_descriptors_.size() > 1 &&
      sort_descriptors_[1].column_id != -1) {
    // Try the secondary sort.
    return SwapCompareResult(
        model_->CompareValues(model_row1, model_row2,
                              sort_descriptors_[1].column_id),
        sort_descriptors_[1].ascending);
  }
  return SwapCompareResult(sort_result, sort_descriptors_[0].ascending);
}

int TableView::GetColumnWidth(int column_id) {
  if (!list_view_)
    return -1;

  std::vector<int>::const_iterator i =
      std::find(visible_columns_.begin(), visible_columns_.end(), column_id);
  if (i == visible_columns_.end())
    return -1;

  return ListView_GetColumnWidth(
      list_view_, static_cast<int>(i - visible_columns_.begin()));
}

void TableView::PaintAltText() {
  if (alt_text_.empty())
    return;

  HDC dc = GetDC(GetNativeControlHWND());
  gfx::Font font = GetAltTextFont();
  gfx::Rect bounds = GetAltTextBounds();
  gfx::CanvasSkia canvas(bounds.width(), bounds.height(), false);
  // Pad by 1 for halo.
  canvas.DrawStringWithHalo(alt_text_, font, SK_ColorDKGRAY, SK_ColorWHITE, 1,
                            1, bounds.width() - 2, bounds.height() - 2,
                            gfx::CanvasSkia::DefaultCanvasTextAlignment());
  canvas.getTopPlatformDevice().drawToHDC(dc, bounds.x(), bounds.y(), NULL);
  ReleaseDC(GetNativeControlHWND(), dc);
}

LRESULT TableView::OnCustomDraw(NMLVCUSTOMDRAW* draw_info) {
  switch (draw_info->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: {
      return CDRF_NOTIFYITEMDRAW;
    }
    case CDDS_ITEMPREPAINT: {
      // The list-view is about to paint an item, tell it we want to
      // notified when it paints every subitem.
      LRESULT r = CDRF_NOTIFYSUBITEMDRAW;
      if (table_type_ == ICON_AND_TEXT)
        r |= CDRF_NOTIFYPOSTPAINT;
      return r;
    }
    case (CDDS_ITEMPREPAINT | CDDS_SUBITEM): {
      // The list-view is painting a subitem. See if the colors should be
      // changed from the default.
      if (custom_colors_enabled_) {
        // At this time, draw_info->clrText and draw_info->clrTextBk are not
        // set.  So we pass in an ItemColor to GetCellColors.  If
        // ItemColor.color_is_set is true, then we use the provided color.
        ItemColor foreground = {0};
        ItemColor background = {0};

        LOGFONT logfont;
        GetObject(GetWindowFont(list_view_), sizeof(logfont), &logfont);

        if (GetCellColors(ViewToModel(
                              static_cast<int>(draw_info->nmcd.dwItemSpec)),
                          draw_info->iSubItem,
                          &foreground,
                          &background,
                          &logfont)) {
          // TODO(tc): Creating/deleting a font for every cell seems like a
          // waste if the font hasn't changed.  Maybe we should use a struct
          // with a bool like we do with colors?
          if (custom_cell_font_)
            DeleteObject(custom_cell_font_);
          l10n_util::AdjustUIFont(&logfont);
          custom_cell_font_ = CreateFontIndirect(&logfont);
          SelectObject(draw_info->nmcd.hdc, custom_cell_font_);
          draw_info->clrText = foreground.color_is_set
                               ? skia::SkColorToCOLORREF(foreground.color)
                               : CLR_DEFAULT;
          draw_info->clrTextBk = background.color_is_set
                                 ? skia::SkColorToCOLORREF(background.color)
                                 : CLR_DEFAULT;
          return CDRF_NEWFONT;
        }
      }
      return CDRF_DODEFAULT;
    }
    case CDDS_ITEMPOSTPAINT: {
      DCHECK((table_type_ == ICON_AND_TEXT) || (ImplementPostPaint()));
      int view_index = static_cast<int>(draw_info->nmcd.dwItemSpec);
      // We get notifications for empty items, just ignore them.
      if (view_index >= model_->RowCount())
        return CDRF_DODEFAULT;
      int model_index = ViewToModel(view_index);
      LRESULT r = CDRF_DODEFAULT;
      // First let's take care of painting the right icon.
      if (table_type_ == ICON_AND_TEXT) {
        SkBitmap image = model_->GetIcon(model_index);
        if (!image.isNull()) {
          // Get the rect that holds the icon.
          RECT icon_rect, client_rect;
          if (ListView_GetItemRect(list_view_, view_index, &icon_rect,
                                   LVIR_ICON) &&
              GetClientRect(list_view_, &client_rect)) {
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
                  list_view_, view_index, LVIS_SELECTED | LVIS_DROPHILITED);
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
              canvas.drawColor(
                  skia::COLORREFToSkColor(GetSysColor(bg_color_index)),
                  SkXfermode::kSrc_Mode);
              // + 1 for padding (we declared the image as 18x18 in the list-
              // view when they are 16x16 so we get an extra pixel of padding).
              canvas.DrawBitmapInt(image, 0, 0,
                                   image.width(), image.height(),
                                   1, 1, kFaviconSize, kFaviconSize, true);

              // Only paint the visible region of the icon.
              RECT to_draw = { intersection.left - icon_rect.left,
                               intersection.top - icon_rect.top,
                               0, 0 };
              to_draw.right = to_draw.left +
                              (intersection.right - intersection.left);
              to_draw.bottom = to_draw.top +
                              (intersection.bottom - intersection.top);
              canvas.getTopPlatformDevice().drawToHDC(draw_info->nmcd.hdc,
                                                      intersection.left,
                                                      intersection.top,
                                                      &to_draw);
              r = CDRF_SKIPDEFAULT;
            }
          }
        }
      }
      if (ImplementPostPaint()) {
        RECT cell_rect;
        if (ListView_GetItemRect(list_view_, view_index, &cell_rect,
                                 LVIR_BOUNDS)) {
          PostPaint(model_index, 0, false, gfx::Rect(cell_rect),
                    draw_info->nmcd.hdc);
          r = CDRF_SKIPDEFAULT;
        }
      }
      return r;
    }
    default:
      return CDRF_DODEFAULT;
  }
}

void TableView::UpdateListViewCache(int start, int length, bool add) {
  ignore_listview_change_ = true;
  UpdateListViewCache0(start, length, add);
  ignore_listview_change_ = false;
}

void TableView::ResetColumnSizes() {
  if (!list_view_)
    return;

  // See comment in TableColumn for what this does.
  int width = this->width();
  RECT native_bounds;
  if (GetClientRect(GetNativeControlHWND(), &native_bounds)) {
    int window_width = native_bounds.right - native_bounds.left;
    if (window_width > 0) {
      // Prefer the bounds of the window over our bounds, which may be
      // different.
      width = window_width;
      // Only set the flag when we know the true width of the table.
      column_sizes_valid_ = true;
    }
  }

  float percent = 0;
  int fixed_width = 0;
  int autosize_width = 0;

  for (std::vector<int>::const_iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    TableColumn& col = all_columns_[*i];
    int col_index = static_cast<int>(i - visible_columns_.begin());
    if (col.width == -1) {
      if (col.percent > 0) {
        percent += col.percent;
      } else {
        autosize_width += col.min_visible_width;
      }
    } else {
      fixed_width += ListView_GetColumnWidth(list_view_, col_index);
    }
  }

  // Now do a pass to set the actual sizes of auto-sized and
  // percent-sized columns.
  int available_width = width - fixed_width - autosize_width;
  for (std::vector<int>::const_iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    TableColumn& col = all_columns_[*i];
    if (col.width == -1) {
      int col_index = static_cast<int>(i - visible_columns_.begin());
      if (col.percent > 0) {
        if (available_width > 0) {
          int col_width =
            static_cast<int>(available_width * (col.percent / percent));
          available_width -= col_width;
          percent -= col.percent;
          ListView_SetColumnWidth(list_view_, col_index, col_width);
        }
      } else {
        int col_width = col.min_visible_width;
        // If no "percent" columns, the last column acts as one, if auto-sized.
        if (percent == 0.f && available_width > 0 &&
            col_index == column_count_ - 1) {
          col_width += available_width;
        }
        ListView_SetColumnWidth(list_view_, col_index, col_width);
      }
    }
  }
}

gfx::Size TableView::GetPreferredSize() {
  return preferred_size_;
}

void TableView::SetPreferredSize(const gfx::Size& size) {
  preferred_size_ = size;
  PreferredSizeChanged();
}

int TableView::ModelToView(int model_index) const {
  if (!model_to_view_.get())
    return model_index;
  DCHECK_GE(model_index, 0) << " negative model_index " << model_index;
  DCHECK_LT(model_index, RowCount()) << " out of bounds model_index " <<
      model_index;
  return model_to_view_[model_index];
}

int TableView::ViewToModel(int view_index) const {
  if (!view_to_model_.get())
    return view_index;
  DCHECK_GE(view_index, 0) << " negative view_index " << view_index;
  DCHECK_LT(view_index, RowCount()) << " out of bounds view_index " <<
      view_index;
  return view_to_model_[view_index];
}

void TableView::SetAltText(const std::wstring& alt_text) {
  if (alt_text == alt_text_)
    return;

  alt_text_ = alt_text;
  if (!GetNativeControlHWND())
    return;

  RECT alt_text_bounds = GetAltTextBounds().ToRECT();
  InvalidateRect(GetNativeControlHWND(), &alt_text_bounds, FALSE);
}

void TableView::UpdateListViewCache0(int start, int length, bool add) {
  if (is_sorted()) {
    if (add)
      UpdateItemsLParams(start, length);
    else
      UpdateItemsLParams(0, 0);
  }

  LVITEM item = {0};
  if (add) {
    const bool has_groups = model_->HasGroups();
    for (int i = start; i < start + length; ++i) {
      item.mask = has_groups ? (LVIF_GROUPID | LVIF_PARAM) : LVIF_PARAM;
      item.iItem = i;
      if (has_groups)
        item.iGroupId = model_->GetGroupID(i);
      if (model_->ShouldIndent(i)) {
        item.mask |= LVIF_INDENT;
        item.iIndent = 1;
      }
      item.lParam = i;
      ListView_InsertItem(list_view_, &item);
    }
  }

  memset(&item, 0, sizeof(LVITEM));
  item.mask =
      (table_type_ == ICON_AND_TEXT) ? (LVIF_IMAGE | LVIF_TEXT) : LVIF_TEXT;
  item.stateMask = 0;
  for (int j = 0; j < column_count_; ++j) {
    TableColumn& col = all_columns_[visible_columns_[j]];
    int max_text_width = ListView_GetStringWidth(list_view_, col.title.c_str());
    for (int i = start; i < start + length; ++i) {
      // Set item.
      item.iItem = add ? i : ModelToView(i);
      item.iSubItem = j;
      std::wstring text = model_->GetText(i, visible_columns_[j]);
      item.pszText = const_cast<LPWSTR>(text.c_str());
      ListView_SetItem(list_view_, &item);

      // Compute width in px, using current font.
      int string_width = ListView_GetStringWidth(list_view_, item.pszText);
      // The width of an icon belongs to the first column.
      if (j == 0 && table_type_ == ICON_AND_TEXT)
        string_width += kListViewIconWidthAndPadding;
      max_text_width = std::max(string_width, max_text_width);
    }

    // ListView_GetStringWidth must be padded or else truncation will occur
    // (MSDN). 15px matches the Win32/LVSCW_AUTOSIZE_USEHEADER behavior.
    max_text_width += kListViewTextPadding;

    // Protect against partial update.
    if (max_text_width > col.min_visible_width ||
        (start == 0 && length == model_->RowCount())) {
      col.min_visible_width = max_text_width;
    }
  }

  if (is_sorted()) {
    // NOTE: As most of our tables are smallish I'm not going to optimize this.
    // If our tables become large and frequently update, then it'll make sense
    // to optimize this.

    SortItemsAndUpdateMapping();
  }
}

void TableView::OnDoubleClick() {
  if (!ignore_listview_change_ && table_view_observer_) {
    table_view_observer_->OnDoubleClick();
  }
}

void TableView::OnMiddleClick() {
  if (!ignore_listview_change_ && table_view_observer_)
    table_view_observer_->OnMiddleClick();
}

void TableView::OnSelectedStateChanged() {
  if (!ignore_listview_change_ && table_view_observer_) {
    table_view_observer_->OnSelectionChanged();
  }
}

bool TableView::OnKeyDown(ui::KeyboardCode virtual_keycode) {
  if (!ignore_listview_change_ && table_view_observer_) {
    table_view_observer_->OnKeyDown(virtual_keycode);
  }
  return false;  // Let the key event be processed as ususal.
}

void TableView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (!list_view_)
    return;
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  Layout();
  if ((autosize_columns_ || !column_sizes_valid_) && width() > 0)
    ResetColumnSizes();
  UpdateContentOffset();
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
}

int TableView::PreviousSelectedViewIndex(int view_index) {
  DCHECK_GE(view_index, 0);
  if (!list_view_ || view_index <= 0)
    return -1;

  int row_count = RowCount();
  if (row_count == 0)
    return -1;  // Empty table, nothing can be selected.

  // For some reason
  // ListView_GetNextItem(list_view_,item, LVNI_SELECTED | LVNI_ABOVE)
  // fails on Vista (always returns -1), so we iterate through the indices.
  view_index = std::min(view_index, row_count);
  while (--view_index >= 0 && !IsItemSelected(ViewToModel(view_index)));
  return view_index;
}

int TableView::LastSelectedViewIndex() {
  return PreviousSelectedViewIndex(RowCount());
}

void TableView::UpdateContentOffset() {
  content_offset_ = 0;

  if (!list_view_)
    return;

  HWND header = ListView_GetHeader(list_view_);
  if (!header)
    return;

  POINT origin = {0, 0};
  MapWindowPoints(header, list_view_, &origin, 1);

  RECT header_bounds;
  GetWindowRect(header, &header_bounds);

  content_offset_ = origin.y + header_bounds.bottom - header_bounds.top;
}

void TableView::UpdateGroups() {
  // Add the groups.
  if (model_ && model_->HasGroups()) {
    ListView_RemoveAllGroups(list_view_);

    // Windows XP seems to disable groups if we remove them, so we
    // re-enable them.
    ListView_EnableGroupView(list_view_, true);

    TableModel::Groups groups = model_->GetGroups();
    LVGROUP group = { 0 };
    group.cbSize = sizeof(LVGROUP);
    group.mask = LVGF_HEADER | LVGF_ALIGN | LVGF_GROUPID;
    group.uAlign = LVGA_HEADER_LEFT;
    for (size_t i = 0; i < groups.size(); ++i) {
      group.pszHeader = const_cast<wchar_t*>(groups[i].title.c_str());
      group.iGroupId = groups[i].id;
      ListView_InsertGroup(list_view_, static_cast<int>(i), &group);
    }
  }
}

gfx::Rect TableView::GetAltTextBounds() {
  static const int kXOffset = 16;
  DCHECK(GetNativeControlHWND());
  RECT client_rect_rect;
  GetClientRect(GetNativeControlHWND(), &client_rect_rect);
  gfx::Rect client_rect(client_rect_rect);
  gfx::Font font = GetAltTextFont();
  // Pad height by 2 for halo.
  return gfx::Rect(kXOffset, content_offset(), client_rect.width() - kXOffset,
                   std::max(kImageSize, font.GetHeight() + 2));
}

gfx::Font TableView::GetAltTextFont() {
  return ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
}

void TableView::VisibilityChanged(View* starting_from, bool is_visible) {
  // GetClientRect as used by ResetColumnSize to obtain the total width
  // available to the columns only works when the native control is visible, so
  // update the column sizes in case we become visible. This depends on
  // VisibilityChanged() being called in post order on the view tree.
  if (is_visible && (autosize_columns_ || !column_sizes_valid_) && width() > 0)
    ResetColumnSizes();
}


//
// TableSelectionIterator
//
TableSelectionIterator::TableSelectionIterator(TableView* view,
                                               int view_index)
    : table_view_(view),
      view_index_(view_index) {
  UpdateModelIndexFromViewIndex();
}

TableSelectionIterator& TableSelectionIterator::operator=(
    const TableSelectionIterator& other) {
  view_index_ = other.view_index_;
  model_index_ = other.model_index_;
  return *this;
}

bool TableSelectionIterator::operator==(const TableSelectionIterator& other) {
  return (other.view_index_ == view_index_);
}

bool TableSelectionIterator::operator!=(const TableSelectionIterator& other) {
  return (other.view_index_ != view_index_);
}

TableSelectionIterator& TableSelectionIterator::operator++() {
  view_index_ = table_view_->PreviousSelectedViewIndex(view_index_);
  UpdateModelIndexFromViewIndex();
  return *this;
}

int TableSelectionIterator::operator*() {
  return model_index_;
}

void TableSelectionIterator::UpdateModelIndexFromViewIndex() {
  if (view_index_ == -1)
    model_index_ = -1;
  else
    model_index_ = table_view_->ViewToModel(view_index_);
}

}  // namespace views
