// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_NATIVE_TABLE_WIN_H_
#define UI_VIEWS_CONTROLS_TABLE_NATIVE_TABLE_WIN_H_
#pragma once

#include <windows.h>

#include "ui/base/models/table_model.h"
#include "ui/views/controls/native_control_win.h"
#include "ui/views/controls/table/native_table_wrapper.h"

typedef struct tagNMLVCUSTOMDRAW NMLVCUSTOMDRAW;

namespace views {

class TableView2;

// A View that hosts a native Windows table.
class NativeTableWin : public NativeControlWin, public NativeTableWrapper {
 public:
  explicit NativeTableWin(TableView2* table);
  virtual ~NativeTableWin();

  // NativeTableWrapper implementation:
  virtual int GetRowCount() const;
  virtual View* GetView();
  virtual void SetFocus();
  virtual gfx::NativeView GetTestingHandle() const;
  virtual void InsertColumn(const ui::TableColumn& column, int index);
  virtual void RemoveColumn(int index);
  virtual int GetColumnWidth(int column_index) const;
  virtual void SetColumnWidth(int column_index, int width);
  virtual int GetSelectedRowCount() const;
  virtual int GetFirstSelectedRow() const;
  virtual int GetFirstFocusedRow() const;
  virtual void ClearSelection();
  virtual void ClearRowFocus();
  virtual void SetSelectedState(int model_row, bool state);
  virtual void SetFocusState(int model_row, bool state);
  virtual bool IsRowSelected(int model_row) const;
  virtual bool IsRowFocused(int model_row) const;
  virtual void OnRowsChanged(int start, int length);
  virtual void OnRowsAdded(int start, int length);
  virtual void OnRowsRemoved(int start, int length);
  virtual gfx::Rect GetBounds() const;

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from NativeControlWin:
  virtual bool ProcessMessage(UINT message,
                              WPARAM w_param,
                              LPARAM l_param,
                              LRESULT* result);

 protected:
  virtual void CreateNativeControl();

 private:
  // Makes |model_row| the only selected and focused row.
  void Select(int model_row);

  // Notification from the ListView that the selected state of an item has
  // changed.
  virtual void OnSelectedStateChanged();

  // Notification from the ListView that the used double clicked the table.
  virtual void OnDoubleClick();

  // Notification from the ListView that the user middle clicked the table.
  virtual void OnMiddleClick();

  // Overridden from NativeControl. Notifies the observer.
  virtual bool OnKeyDown(ui::KeyboardCode virtual_keycode);

  // Custom drawing of our icons.
  LRESULT OnCustomDraw(NMLVCUSTOMDRAW* draw_info);

  void UpdateListViewCache(int start, int length, bool add);

  void UpdateContentOffset();

  // Window procedure of the list view class. We subclass the list view to
  // ignore WM_ERASEBKGND, which gives smoother painting during resizing.
  static LRESULT CALLBACK TableWndProc(HWND window,
                                       UINT message,
                                       WPARAM w_param,
                                       LPARAM l_param);

  // Window procedure of the header class. We subclass the header of the table
  // to disable resizing of columns.
  static LRESULT CALLBACK TableHeaderWndProc(HWND window,
                                             UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param);

  // If true, any events that would normally be propagated to the observer
  // are ignored. For example, if this is true and the selection changes in
  // the listview, the observer is not notified.
  bool ignore_listview_change_;

  // The Table we are bound to.
  TableView2* table_;

  // The Y offset from the top of the table to the actual content (passed the
  // header if any).
  int content_offset_;

  // The list view's header original proc handler. It is required when
  // subclassing.
  WNDPROC header_original_handler_;

  // Window procedure of the listview before we subclassed it.
  WNDPROC original_handler_;

  // Size (width and height) of images.
  static const int kImageSize;

  DISALLOW_COPY_AND_ASSIGN(NativeTableWin);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_NATIVE_TABLE_WIN_H_
