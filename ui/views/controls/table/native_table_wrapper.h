// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_NATIVE_TABLE_WRAPPER_H_
#define UI_VIEWS_CONTROLS_TABLE_NATIVE_TABLE_WRAPPER_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace ui {
struct TableColumn;
}

namespace views {

class TableView2;
class View;

// An interface implemented by an object that provides a platform-native
// table.
class NativeTableWrapper {
 public:
  // Returns the number of rows in the table.
  virtual int GetRowCount() const = 0;

  // Inserts/removes a column at the specified index.
  virtual void InsertColumn(const ui::TableColumn& column, int index) = 0;
  virtual void RemoveColumn(int index) = 0;

  // Returns the number of rows that are selected.
  virtual int GetSelectedRowCount() const = 0;

  // Returns the first row that is selected/focused in terms of the model.
  virtual int GetFirstSelectedRow() const = 0;
  virtual int GetFirstFocusedRow() const = 0;

  // Unselect all rows.
  virtual void ClearSelection() = 0;

  virtual void ClearRowFocus() = 0;

  virtual void SetSelectedState(int model_row, bool state) = 0;

  virtual void SetFocusState(int model_row, bool state) = 0;

  // Returns true if the row at the specified index is selected.
  virtual bool IsRowSelected(int model_row) const = 0;

  // Returns true if the row at the specified index has the focus.
  virtual bool IsRowFocused(int model_row) const = 0;

  // Retrieves the views::View that hosts the native control.
  virtual View* GetView() = 0;

  // Sets the focus to the table.
  virtual void SetFocus() = 0;

  // Returns a handle to the underlying native view for testing.
  virtual gfx::NativeView GetTestingHandle() const = 0;

  // Gets/sets the columns width.
  virtual int GetColumnWidth(int column_index) const = 0;
  virtual void SetColumnWidth(int column_index, int width) = 0;

  // Called by the table view to indicate that some rows have changed, been
  // added or been removed.
  virtual void OnRowsChanged(int start, int length) = 0;
  virtual void OnRowsAdded(int start, int length) = 0;
  virtual void OnRowsRemoved(int start, int length) = 0;

  // Returns the bounds of the native table.
  virtual gfx::Rect GetBounds() const = 0;

  // Creates an appropriate NativeTableWrapper for the platform.
  static NativeTableWrapper* CreateNativeWrapper(TableView2* table);

 protected:
  virtual ~NativeTableWrapper() {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_NATIVE_TABLE_WRAPPER_H_
