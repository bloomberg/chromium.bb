// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_GROUP_TABLE_MODEL_H_
#define UI_VIEWS_CONTROLS_TABLE_GROUP_TABLE_MODEL_H_
#pragma once

#include "ui/base/models/table_model.h"

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

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_GROUP_TABLE_MODEL_H_
