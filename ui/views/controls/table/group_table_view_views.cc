// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/group_table_view_views.h"

#include "ui/views/controls/table/group_table_model.h"

namespace views {

const char GroupTableView::kViewClassName[] = "views/GroupTableView";

GroupTableView::GroupTableView(GroupTableModel* model,
                               const std::vector<ui::TableColumn>& columns,
                               TableTypes table_type,
                               bool single_selection,
                               bool resizable_columns,
                               bool autosize_columns,
                               bool draw_group_separators)
    : TableView(model, columns, table_type, false, resizable_columns,
                autosize_columns) {
}

GroupTableView::~GroupTableView() {
}

std::string GroupTableView::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
