// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW2_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW2_H_
#pragma once

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/table/native_table_wrapper.h"
#include "ui/views/controls/table/table_view.h"
#include "views/view.h"

namespace ui {
struct TableColumn;
class TableModel;
}

// A TableView2 is a view that displays multiple rows with any number of
// columns.
// TableView is driven by a TableModel. The model returns the contents
// to display. TableModel also has an Observer which is used to notify
// TableView of changes to the model so that the display may be updated
// appropriately.
//
// TableView2 itself has an observer that is notified when the selection
// changes.
//
// TableView2 is the current port of TableView to use a NativeControl for
// portability.
//
// TODO(jcampan): add sorting.
// TODO(jcampan): add group support.

namespace views {

class ListView;
class ListViewParent;
class TableView;
class TableViewObserver;
class View;

class VIEWS_EXPORT TableView2 : public View, public ui::TableModelObserver {
 public:
  typedef TableSelectionIterator iterator;

  // A helper struct for GetCellColors. Set |color_is_set| to true if color is
  // set.  See OnCustomDraw for more details on why we need this.
  struct ItemColor {
    bool color_is_set;
    SkColor color;
  };

  // Bitmasks of options for creating an instance of the table view.  See
  // comments next to the corresponding members in TableView2 for details
  // (ex. SINGLE_SELECTION -> single_selection_).
  enum Options {
    NONE              = 0,
    SINGLE_SELECTION  = 1 << 0,
    RESIZABLE_COLUMNS = 1 << 1,
    AUTOSIZE_COLUMNS  = 1 << 2,
    HORIZONTAL_LINES  = 1 << 3,
    VERTICAL_LINES    = 1 << 4,
  };

  // Creates a new table using the model and columns specified.
  // The table type applies to the content of the first column (text, icon and
  // text, checkbox and text).
  // When autosize_columns is true, columns always fill the available width. If
  // false, columns are not resized when the table is resized. An extra empty
  // column at the right fills the remaining space.
  // When resizable_columns is true, users can resize columns by dragging the
  // separator on the column header.  NOTE: Right now this is always true.  The
  // code to set it false is still in place to be a base for future, better
  // resizing behavior (see http://b/issue?id=874646 ), but no one uses or
  // tests the case where this flag is false.
  // Note that setting both resizable_columns and autosize_columns to false is
  // probably not a good idea, as there is no way for the user to increase a
  // column's size in that case.
  // |options| is a bitmask of options. See comments at Options.
  TableView2(ui::TableModel* model, const std::vector<ui::TableColumn>& columns,
             TableTypes table_type, int options);
  virtual ~TableView2();

  // Assigns a new model to the table view, detaching the old one if present.
  // If |model| is NULL, the table view cannot be used after this call. This
  // should be called in the containing view's destructor to avoid destruction
  // issues when the model needs to be deleted before the table.
  void SetModel(ui::TableModel* model);
  ui::TableModel* model() const { return model_; }

  // Returns the number of rows in the table.
  int GetRowCount();

  // Returns the number of selected rows.
  int SelectedRowCount();

  // Makes all row not selected.
  void ClearSelection();

  // Makes all row not focused.
  void ClearRowFocus();

  // Returns the index of the first selected row.
  int GetFirstSelectedRow();

  // Returns the index of the first focused row.
  int GetFirstFocusedRow();

  // Selects the specified row, making sure it's visible.
  void SelectRow(int model_row);

  // Sets the focus to the row at the given index.
  void FocusRow(int model_row);

  // Returns true if the row at the specified index is selected.
  bool IsRowSelected(int model_row);

  // Returns true if the row at the specified index has the focus.
  bool IsRowFocused(int model_row);

  // Returns an iterator over the selection. The iterator proceeds from the
  // last index to the first.
  //
  // NOTE: the iterator iterates over the visual order (but returns coordinates
  // in terms of the model).
  iterator SelectionBegin();
  iterator SelectionEnd();

  // ui::TableModelObserver methods.
  virtual void OnModelChanged() OVERRIDE;
  virtual void OnItemsChanged(int start, int length) OVERRIDE;
  virtual void OnItemsAdded(int start, int length) OVERRIDE;
  virtual void OnItemsRemoved(int start, int length) OVERRIDE;

  void SetObserver(TableViewObserver* observer) {
    table_view_observer_ = observer;
  }
  TableViewObserver* observer() const { return table_view_observer_; }

  // Replaces the set of known columns without changing the current visible
  // columns.
  void SetColumns(const std::vector<ui::TableColumn>& columns);
  void AddColumn(const ui::TableColumn& col);
  bool HasColumn(int id);

  // Sets which columns (by id) are displayed.  All transient size and position
  // information is lost.
  void SetVisibleColumns(const std::vector<int>& columns);
  void SetColumnVisibility(int id, bool is_visible);
  bool IsColumnVisible(int id) const;

  ui::TableColumn GetVisibleColumnAt(int index);
  size_t GetVisibleColumnCount();

  // Resets the size of the columns based on the sizes passed to the
  // constructor. Your normally needn't invoked this, it's done for you the
  // first time the TableView is given a valid size.
  void ResetColumnSizes();

  bool single_selection() const {
    return single_selection_;
  }

  TableTypes type() const {
    return table_type_;
  }

  bool resizable_columns() const {
    return resizable_columns_;
  }

  bool autosize_columns() const {
    return autosize_columns_;
  }

  bool horizontal_lines() const {
    return horizontal_lines_;
  }

  bool vertical_lines() const {
    return vertical_lines_;
  }

  virtual void Layout() OVERRIDE;

  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Used by tests.
  virtual gfx::NativeView GetTestingHandle();

 protected:
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;

 private:
  friend class ListViewParent;
  friend class TableSelectionIterator;

  // Used in the constructors.
  void Init(const std::vector<ui::TableColumn>& columns);

  // We need this wrapper to pass the table view to the windows proc handler
  // when subclassing the list view and list view header, as the reinterpret
  // cast from GetWindowLongPtr would break the pointer if it is pointing to a
  // subclass (in the OO sense of TableView).
  struct TableViewWrapper {
    explicit TableViewWrapper(TableView2* view) : table_view(view) {}
    TableView2* table_view;
  };

  // Adds a new column.
  void InsertColumn(const ui::TableColumn& tc, int index);

  // Update headers and internal state after columns have changed
  void OnColumnsChanged();

  ui::TableModel* model_;
  TableTypes table_type_;
  TableViewObserver* table_view_observer_;

  // An ordered list of id's into all_columns_ representing current visible
  // columns.
  std::vector<int> visible_columns_;

  // Mapping of an int id to a TableColumn representing all possible columns.
  std::map<int, ui::TableColumn> all_columns_;

  // Cached value of columns_.size()
  int column_count_;

  // Selection mode.
  bool single_selection_;

  // Whether or not the user can resize columns.
  bool resizable_columns_;

  // Whether or not columns should automatically be resized to fill the
  // the available width when the list view is resized.
  bool autosize_columns_;

  // Whether or not horizontal grid lines should be drawn.
  bool horizontal_lines_;

  // Whether or not vertical grid lines should be drawn.
  bool vertical_lines_;

  // Mappings used when sorted.
  // scoped_array<int> view_to_model_;
  // scoped_array<int> model_to_view_;

  // The object that actually implements the table.
  NativeTableWrapper* native_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(TableView2);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW2_H_
