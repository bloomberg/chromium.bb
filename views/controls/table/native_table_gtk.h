// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TABLE_NATIVE_TABLE_GTK_H_
#define VIEWS_CONTROLS_TABLE_NATIVE_TABLE_GTK_H_
#pragma once

#include "ui/base/models/table_model.h"
#include "ui/base/gtk/gtk_signal.h"
#include "views/controls/native_control_gtk.h"
#include "views/controls/table/native_table_wrapper.h"

namespace views {

class TableView2;

// A View that hosts a native GTK table.
// TODO: Note that the implementation is still incomplete. What remains to be
// done:
// - support resizable columns
// - support autosize columns
// - implement focus row methods.
class NativeTableGtk : public NativeControlGtk, public NativeTableWrapper {
 public:
  explicit NativeTableGtk(TableView2* table);
  virtual ~NativeTableGtk();

  // NativeTableWrapper implementation:
  virtual int GetRowCount() const;
  virtual View* GetView();
  virtual void SetFocus();
  virtual gfx::NativeView GetTestingHandle() const;
  virtual void InsertColumn(const TableColumn& column, int index);
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

 protected:
  // NativeControlGtk implementation:
  virtual void CreateNativeControl();

 private:
  void InsertTextColumn(const TableColumn& column, int index);
  void InsertIconAndTextColumn(const TableColumn& column, int index);

  // Sets the content of the row pointed to by |iter| in the tree_view_, using
  // the data in the model at row |index|.
  void SetRowData(int index, GtkTreeIter* iter);

  // Handles the "cursor-changed" event.
  CHROMEGTK_CALLBACK_0(NativeTableGtk, void, OnCursorChanged);

  // Returns the icon that should be displayed for the row at |row|.
  GdkPixbuf* GetModelIcon(int row);

  // The Table we are bound to.
  TableView2* table_;

  // The GtkTree model.
  GtkListStore* gtk_model_;

  // The actual tree view.
  GtkTreeView* tree_view_;

  // The selection for tree_view_.
  GtkTreeSelection* tree_selection_;

  // Size (width and height) of images.
  static const int kImageSize;

  DISALLOW_COPY_AND_ASSIGN(NativeTableGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TABLE_NATIVE_TABLE_GTK_H_
