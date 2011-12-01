// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_H_
#pragma once

#include <map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/views/views_export.h"

#if defined(OS_WIN)
#include <windows.h>

// TODO(port): remove the ifdef when native_control.h is ported.
#include "ui/views/controls/native_control.h"

typedef struct tagNMLVCUSTOMDRAW NMLVCUSTOMDRAW;
#endif  // defined(OS_WIN)

namespace gfx {
class Font;
}

namespace ui {
struct TableColumn;
class TableModel;
}

// A TableView is a view that displays multiple rows with any number of columns.
// TableView is driven by a TableModel. The model returns the contents
// to display. TableModel also has an Observer which is used to notify
// TableView of changes to the model so that the display may be updated
// appropriately.
//
// TableView itself has an observer that is notified when the selection
// changes.
//
// Tables may be sorted either by directly invoking SetSortDescriptors or by
// marking the column as sortable and the user doing a gesture to sort the
// contents. TableView itself maintains the sort so that the underlying model
// isn't effected.
//
// When a table is sorted the model coordinates do not necessarily match the
// view coordinates. All table methods are in terms of the model. If you need to
// convert to view coordinates use model_to_view.
//
// Sorting is done by a locale sensitive string sort. You can customize the
// sort by way of overriding CompareValues.
//
// TableView is a wrapper around the window type ListView in report mode.
namespace views {

class ListView;
class ListViewParent;
class TableView;
class TableViewObserver;

// The cells in the first column of a table can contain:
// - only text
// - a small icon (16x16) and some text
// - a check box and some text
enum TableTypes {
  TEXT_ONLY = 0,
  ICON_AND_TEXT,
  CHECK_BOX_AND_TEXT
};

// Returned from SelectionBegin/SelectionEnd
class VIEWS_EXPORT TableSelectionIterator {
 public:
  TableSelectionIterator(TableView* view, int view_index);
  TableSelectionIterator& operator=(const TableSelectionIterator& other);
  bool operator==(const TableSelectionIterator& other);
  bool operator!=(const TableSelectionIterator& other);
  TableSelectionIterator& operator++();
  int operator*();

 private:
  void UpdateModelIndexFromViewIndex();

  TableView* table_view_;
  int view_index_;

  // The index in terms of the model. This is returned from the * operator. This
  // is cached to avoid dependencies on the view_to_model mapping.
  int model_index_;
};

#if defined(OS_WIN)
// TODO(port): Port TableView.
class VIEWS_EXPORT TableView : public NativeControl,
                               public ui::TableModelObserver {
 public:
  typedef TableSelectionIterator iterator;

  // A helper struct for GetCellColors. Set |color_is_set| to true if color is
  // set.  See OnCustomDraw for more details on why we need this.
  struct ItemColor {
    bool color_is_set;
    SkColor color;
  };

  // Describes a sorted column.
  struct SortDescriptor {
    SortDescriptor() : column_id(-1), ascending(true) {}
    SortDescriptor(int column_id, bool ascending)
        : column_id(column_id),
          ascending(ascending) { }

    // ID of the sorted column.
    int column_id;

    // Is the sort ascending?
    bool ascending;
  };

  typedef std::vector<SortDescriptor> SortDescriptors;

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
  TableView(ui::TableModel* model, const std::vector<ui::TableColumn>& columns,
            TableTypes table_type, bool single_selection,
            bool resizable_columns, bool autosize_columns);
  virtual ~TableView();

  // Assigns a new model to the table view, detaching the old one if present.
  // If |model| is NULL, the table view cannot be used after this call. This
  // should be called in the containing view's destructor to avoid destruction
  // issues when the model needs to be deleted before the table.
  void SetModel(ui::TableModel* model);
  ui::TableModel* model() const { return model_; }

  // Resorts the contents.
  void SetSortDescriptors(const SortDescriptors& sort_descriptors);

  // Current sort.
  const SortDescriptors& sort_descriptors() const { return sort_descriptors_; }

  // Returns the number of rows in the TableView.
  int RowCount() const;

  // Returns the number of selected rows.
  int SelectedRowCount();

  // Selects the specified item, making sure it's visible.
  void Select(int model_row);

  // Sets the selected state of an item (without sending any selection
  // notifications). Note that this routine does NOT set the focus to the
  // item at the given index.
  void SetSelectedState(int model_row, bool state);

  // Sets the focus to the item at the given index.
  void SetFocusOnItem(int model_row);

  // Returns the first selected row in terms of the model.
  int FirstSelectedRow();

  // Returns true if the item at the specified index is selected.
  bool IsItemSelected(int model_row);

  // Returns true if the item at the specified index has the focus.
  bool ItemHasTheFocus(int model_row);

  // Returns an iterator over the selection. The iterator proceeds from the
  // last index to the first.
  //
  // NOTE: the iterator iterates over the visual order (but returns coordinates
  // in terms of the model).
  iterator SelectionBegin();
  iterator SelectionEnd();

  // ui::TableModelObserver methods.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

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

  // Resets the size of the columns based on the sizes passed to the
  // constructor. Your normally needn't invoked this, it's done for you the
  // first time the TableView is given a valid size.
  void ResetColumnSizes();

  // Sometimes we may want to size the TableView to a specific width and
  // height.
  virtual gfx::Size GetPreferredSize();
  void SetPreferredSize(const gfx::Size& size);

  // Is the table sorted?
  bool is_sorted() const { return !sort_descriptors_.empty(); }

  // Maps from the index in terms of the model to that of the view.
  int ModelToView(int model_index) const;

  // Maps from the index in terms of the view to that of the model.
  int ViewToModel(int view_index) const;

  // Sets the text to display on top of the table. This is useful if the table
  // is empty and you want to inform the user why.
  void SetAltText(const string16& alt_text);

 protected:
  // Overriden to return the position of the first selected row.
  virtual gfx::Point GetKeyboardContextMenuLocation() OVERRIDE;

  // Subclasses that want to customize the colors of a particular row/column,
  // must invoke this passing in true. The default value is false, such that
  // GetCellColors is never invoked.
  void SetCustomColorsEnabled(bool custom_colors_enabled);

  // Notification from the ListView that the selected state of an item has
  // changed.
  virtual void OnSelectedStateChanged();

  // Notification from the ListView that the used double clicked the table.
  virtual void OnDoubleClick();

  // Notification from the ListView that the user middle clicked the table.
  virtual void OnMiddleClick();

  // Overridden from NativeControl. Notifies the observer.
  virtual bool OnKeyDown(ui::KeyboardCode virtual_keycode) OVERRIDE;

  // View override.
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

  // Invoked to customize the colors or font at a particular cell. If you
  // change the colors or font, return true. This is only invoked if
  // SetCustomColorsEnabled(true) has been invoked.
  virtual bool GetCellColors(int model_row,
                             int column,
                             ItemColor* foreground,
                             ItemColor* background,
                             LOGFONT* logfont);

  // Subclasses that want to perform some custom painting (on top of the regular
  // list view painting) should return true here and implement the PostPaint
  // method.
  virtual bool ImplementPostPaint() { return false; }
  // Subclasses can implement in this method extra-painting for cells.
  virtual void PostPaint(int model_row, int column, bool selected,
                         const gfx::Rect& bounds, HDC device_context) { }
  virtual void PostPaint() {}

  virtual HWND CreateNativeControl(HWND parent_container);

  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param);

  // Used to sort the two rows. Returns a value < 0, == 0 or > 0 indicating
  // whether the row2 comes before row1, row2 is the same as row1 or row1 comes
  // after row2. This invokes CompareValues on the model with the sorted column.
  virtual int CompareRows(int model_row1, int model_row2);

  // Called before sorting. This does nothing and is intended for subclasses
  // that need to cache state used during sorting.
  virtual void PrepareForSort() {}

  // Returns the width of the specified column by id, or -1 if the column isn't
  // visible.
  int GetColumnWidth(int column_id);

  // Returns the offset from the top of the client area to the start of the
  // content.
  int content_offset() const { return content_offset_; }

  // Draws the alt_text_. Does nothing if there is no alt_text_.
  void PaintAltText();

  // Size (width and height) of images.
  static const int kImageSize;

 private:
  // Direction of a sort.
  enum SortDirection {
    ASCENDING_SORT,
    DESCENDING_SORT,
    NO_SORT
  };

  // We need this wrapper to pass the table view to the windows proc handler
  // when subclassing the list view and list view header, as the reinterpret
  // cast from GetWindowLongPtr would break the pointer if it is pointing to a
  // subclass (in the OO sense of TableView).
  struct TableViewWrapper {
    explicit TableViewWrapper(TableView* view) : table_view(view) { }
    TableView* table_view;
  };

  friend class ListViewParent;
  friend class TableSelectionIterator;
  friend class GroupModelTableViewTest;
  FRIEND_TEST_ALL_PREFIXES(GroupModelTableViewTest, ShiftSelectAcrossGroups);
  FRIEND_TEST_ALL_PREFIXES(GroupModelTableViewTest, ShiftSelectSameGroup);

  LRESULT OnCustomDraw(NMLVCUSTOMDRAW* draw_info);

  // Invoked when the user clicks on a column to toggle the sort order. If
  // column_id is the primary sorted column the direction of the sort is
  // toggled, otherwise column_id is made the primary sorted column.
  void ToggleSortOrder(int column_id);

  // Updates the lparam of each of the list view items to be the model index.
  // If length is > 0, all items with an index >= start get offset by length.
  // This is used during sorting to determine how the items were sorted.
  void UpdateItemsLParams(int start, int length);

  // Does the actual sort and updates the mappings (view_to_model and
  // model_to_view) appropriately.
  void SortItemsAndUpdateMapping();

  // Selects multiple items from the current view row to the marked view row
  // (implements shift-click behavior). |view_index| is the most recent row
  // that the user clicked on, and so there is no guarantee that
  // |view_index| > |mark_view_index| or vice-versa. Returns false if the
  // selection attempt was rejected because it crossed group boundaries.
  bool SelectMultiple(int view_index, int mark_view_index);

  // Method invoked by ListView to compare the two values. Invokes CompareRows.
  static int CALLBACK SortFunc(LPARAM model_index_1_p,
                               LPARAM model_index_2_p,
                               LPARAM table_view_param);

  // Method invoked by ListView when sorting back to natural state. Returns
  // model_index_1_p - model_index_2_p.
  static int CALLBACK NaturalSortFunc(LPARAM model_index_1_p,
                                      LPARAM model_index_2_p,
                                      LPARAM table_view_param);

  // Resets the sort image displayed for the specified column.
  void ResetColumnSortImage(int column_id, SortDirection direction);

  // Adds a new column.
  void InsertColumn(const ui::TableColumn& tc, int index);

  // Update headers and internal state after columns have changed
  void OnColumnsChanged();

  // Updates the ListView with values from the model. See UpdateListViewCache0
  // for a complete description.
  // This turns off redrawing, and invokes UpdateListViewCache0 to do the
  // actual updating.
  void UpdateListViewCache(int start, int length, bool add);

  // Updates ListView with values from the model.
  // If add is true, this adds length items starting at index start.
  // If add is not true, the items are not added, the but the values in the
  // range start - [start + length] are updated from the model.
  void UpdateListViewCache0(int start, int length, bool add);

  // Returns the index of the selected item before |view_index|, or -1 if
  // |view_index| is the first selected item.
  //
  // WARNING: this returns coordinates in terms of the view, NOT the model.
  int PreviousSelectedViewIndex(int view_index);

  // Returns the last selected view index in the table view, or -1 if the table
  // is empty, or nothing is selected.
  //
  // WARNING: this returns coordinates in terms of the view, NOT the model.
  int LastSelectedViewIndex();

  // The TableColumn visible at position pos.
  const ui::TableColumn& GetColumnAtPosition(int pos);

  // Window procedure of the list view class. We subclass the list view to
  // ignore WM_ERASEBKGND, which gives smoother painting during resizing.
  static LRESULT CALLBACK TableWndProc(HWND window,
                                       UINT message,
                                       WPARAM w_param,
                                       LPARAM l_param);

  // Window procedure of the header class. We subclass the header of the table
  // to disable resizing of columns.
  static LRESULT CALLBACK TableHeaderWndProc(HWND window, UINT message,
                                             WPARAM w_param, LPARAM l_param);

  // Updates content_offset_ from the position of the header.
  void UpdateContentOffset();

  // Reloads the groups from the model if there is one and it has groups.
  void UpdateGroups();

  // Returns the bounds of the alt text.
  gfx::Rect GetAltTextBounds();

  // Returns the font used for alt text.
  gfx::Font GetAltTextFont();

  // Overriden in order to update the column sizes, which can only be sized
  // accurately when the native control is available.
  virtual void VisibilityChanged(View* starting_from, bool is_visible);

  ui::TableModel* model_;
  TableTypes table_type_;
  TableViewObserver* table_view_observer_;

  // An ordered list of id's into |all_columns_| representing current visible
  // columns.
  std::vector<int> visible_columns_;

  // Mapping of an int id to a TableColumn representing all possible columns.
  std::map<int, ui::TableColumn> all_columns_;

  // Cached value of columns_.size()
  int column_count_;

  // Selection mode.
  bool single_selection_;

  // If true, any events that would normally be propagated to the observer
  // are ignored. For example, if this is true and the selection changes in
  // the listview, the observer is not notified.
  bool ignore_listview_change_;

  // Reflects the value passed to SetCustomColorsEnabled.
  bool custom_colors_enabled_;

  // Whether or not columns should automatically be resized to fill the
  // the available width when the list view is resized.
  bool autosize_columns_;

  // Whether or not the user can resize columns.
  bool resizable_columns_;

  // Whether the column sizes have been determined.
  bool column_sizes_valid_;

  // NOTE: While this has the name View in it, it's not a view. Rather it's
  // a wrapper around the List-View window.
  HWND list_view_;

  // The list view's header original proc handler. It is required when
  // subclassing.
  WNDPROC header_original_handler_;

  // Window procedure of the listview before we subclassed it.
  WNDPROC original_handler_;

  // A wrapper around 'this' used when "subclassing" the list view and header.
  TableViewWrapper table_view_wrapper_;

  // A custom font we use when overriding the font type for a specific cell.
  HFONT custom_cell_font_;

  // The preferred size of the table view.
  gfx::Size preferred_size_;

  int content_offset_;

  // Current sort.
  SortDescriptors sort_descriptors_;

  // Mappings used when sorted.
  scoped_array<int> view_to_model_;
  scoped_array<int> model_to_view_;

  string16 alt_text_;

  DISALLOW_COPY_AND_ASSIGN(TableView);
};
#endif  // defined(OS_WIN)

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_H_
