// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_NATIVE_TABLE_GTK_H_
#define UI_VIEWS_CONTROLS_TABLE_NATIVE_TABLE_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/models/table_model.h"
#include "ui/views/controls/table/native_table_wrapper.h"
#include "views/controls/native_control_gtk.h"

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
  virtual int GetRowCount() const OVERRIDE;
  virtual View* GetView() OVERRIDE;
  virtual void SetFocus() OVERRIDE;
  virtual gfx::NativeView GetTestingHandle() const OVERRIDE;
  virtual void InsertColumn(const ui::TableColumn& column, int index) OVERRIDE;
  virtual void RemoveColumn(int index) OVERRIDE;
  virtual int GetColumnWidth(int column_index) const OVERRIDE;
  virtual void SetColumnWidth(int column_index, int width) OVERRIDE;
  virtual int GetSelectedRowCount() const OVERRIDE;
  virtual int GetFirstSelectedRow() const OVERRIDE;
  virtual int GetFirstFocusedRow() const OVERRIDE;
  virtual void ClearSelection() OVERRIDE;
  virtual void ClearRowFocus() OVERRIDE;
  virtual void SetSelectedState(int model_row, bool state) OVERRIDE;
  virtual void SetFocusState(int model_row, bool state) OVERRIDE;
  virtual bool IsRowSelected(int model_row) const OVERRIDE;
  virtual bool IsRowFocused(int model_row) const OVERRIDE;
  virtual void OnRowsChanged(int start, int length) OVERRIDE;
  virtual void OnRowsAdded(int start, int length) OVERRIDE;
  virtual void OnRowsRemoved(int start, int length) OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;

 protected:
  // NativeControlGtk implementation:
  virtual void CreateNativeControl() OVERRIDE;

 private:
  void InsertTextColumn(const ui::TableColumn& column, int index);
  void InsertIconAndTextColumn(const ui::TableColumn& column, int index);

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

#endif  // UI_VIEWS_CONTROLS_TABLE_NATIVE_TABLE_GTK_H_
