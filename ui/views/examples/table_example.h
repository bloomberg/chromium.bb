// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/table_model.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/examples/example_base.h"
#include "views/controls/button/button.h"

namespace views {
class Checkbox;
class Event;
class TableView;
}

namespace examples {

class TableExample : public ExampleBase,
                     public ui::TableModel,
                     public views::TableViewObserver,
                     public views::ButtonListener {
 public:
  explicit TableExample(ExamplesMain* main);
  virtual ~TableExample();

  // ExampleBase:
  virtual void CreateExampleView(views::View* container) OVERRIDE;

  // ui::TableModel:
  virtual int RowCount() OVERRIDE;
  virtual string16 GetText(int row, int column_id) OVERRIDE;
  virtual SkBitmap GetIcon(int row) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;

  // views::TableViewObserver:
  virtual void OnSelectionChanged() OVERRIDE;
  virtual void OnDoubleClick() OVERRIDE;
  virtual void OnMiddleClick() OVERRIDE;
  virtual void OnKeyDown(ui::KeyboardCode virtual_keycode) OVERRIDE;
  virtual void OnTableViewDelete(views::TableView* table_view) OVERRIDE;
  virtual void OnTableView2Delete(views::TableView2* table_view) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

 private:
  // The table to be tested.
  views::TableView* table_;

  views::Checkbox* column1_visible_checkbox_;
  views::Checkbox* column2_visible_checkbox_;
  views::Checkbox* column3_visible_checkbox_;
  views::Checkbox* column4_visible_checkbox_;

  SkBitmap icon1_;
  SkBitmap icon2_;

  DISALLOW_COPY_AND_ASSIGN(TableExample);
};

}  // namespace examples

#endif  // UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_
