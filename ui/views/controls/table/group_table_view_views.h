// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_GROUP_TABLE_VIEW_VIEWS_H_
#define UI_VIEWS_CONTROLS_TABLE_GROUP_TABLE_VIEW_VIEWS_H_
#pragma once

#include "ui/views/controls/table/table_view.h"

namespace views {

class GroupTableModel;

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

  // View overrides:
  virtual std::string GetClassName() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(GroupTableView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_GROUP_TABLE_VIEW_VIEWS_H_
