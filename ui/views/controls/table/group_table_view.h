// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_GROUP_TABLE_VIEW_H_
#define UI_VIEWS_CONTROLS_TABLE_GROUP_TABLE_VIEW_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "ui/base/models/table_model.h"
#include "ui/views/controls/table/table_view.h"

namespace views {

struct GroupRange {
  int start;
  int length;
};

// The model driving the GroupTableView.
class GroupTableModel : public ui::TableModel {
 public:
  // Populates the passed range with the first row/last row (included)
  // that this item belongs to.
  virtual void GetGroupRangeForItem(int item, GroupRange* range) = 0;
};

// GroupTableView adds grouping to the TableView class.
// It allows to have groups of rows that act as a single row from the selection
// perspective. Groups are visually separated by a horizontal line.
class VIEWS_EXPORT GroupTableView : public TableView {
 public:
   // The view class name.
   static const char kViewClassName[];

  GroupTableView(GroupTableModel* model,
                 const std::vector<ui::TableColumn>& columns,
                 TableTypes table_type, bool single_selection,
                 bool resizable_columns, bool autosize_columns,
                 bool draw_group_separators);
  virtual ~GroupTableView();

  virtual std::string GetClassName() const;

 protected:
  // Notification from the ListView that the selected state of an item has
  // changed.
  void OnSelectedStateChanged();

  // Extra-painting required to draw the separator line between groups.
  virtual bool ImplementPostPaint() { return true; }
  virtual void PostPaint(int model_row, int column, bool selected,
                         const gfx::Rect& bounds, HDC device_context);

  // In order to make keyboard navigation possible (using the Up and Down
  // keys), we must take action when an arrow key is pressed. The reason we
  // need to process this message has to do with the manner in which the focus
  // needs to be set on a group item when a group is selected.
  virtual bool OnKeyDown(ui::KeyboardCode virtual_keycode);

  // Overriden to make sure rows in the same group stay grouped together.
  virtual int CompareRows(int model_row1, int model_row2);

  // Updates model_index_to_range_start_map_ from the model.
  virtual void PrepareForSort();

 private:
  // Make the selection of group consistent.
  void SyncSelection();

  GroupTableModel* model_;

  // If true, draw separators between groups.
  bool draw_group_separators_;

  // A factory to make the selection consistent among groups.
  base::WeakPtrFactory<GroupTableView> sync_selection_factory_;

  // Maps from model row to start of group.
  std::map<int,int> model_index_to_range_start_map_;

  DISALLOW_COPY_AND_ASSIGN(GroupTableView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_GROUP_TABLE_VIEW_H_
