// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_view_views.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/table/table_header.h"

// Put the tests in the views namespace to make it easier to declare them as
// friend classes.
namespace views {

class TableViewTestHelper {
 public:
  explicit TableViewTestHelper(TableView* table) : table_(table) {}

  std::string GetPaintRegion(const gfx::Rect& bounds) {
    TableView::PaintRegion region(table_->GetPaintRegion(bounds));
    return "rows=" + base::IntToString(region.min_row) + " " +
        base::IntToString(region.max_row) + " cols=" +
        base::IntToString(region.min_column) + " " +
        base::IntToString(region.max_column);
  }

  size_t visible_col_count() {
    return table_->visible_columns().size();
  }

  TableHeader* header() { return table_->header_; }

 private:
  TableView* table_;

  DISALLOW_COPY_AND_ASSIGN(TableViewTestHelper);
};

namespace {

// TestTableModel2 -------------------------------------------------------------

// Trivial TableModel implementation that is backed by a vector of vectors.
// Provides methods for adding/removing/changing the contents that notify the
// observer appropriately.
//
// Initial contents are:
// 0, 1
// 1, 1
// 2, 2
// 3, 0
class TestTableModel2 : public ui::TableModel {
 public:
  TestTableModel2();

  // Adds a new row at index |row| with values |c1_value| and |c2_value|.
  void AddRow(int row, int c1_value, int c2_value);

  // Removes the row at index |row|.
  void RemoveRow(int row);

  // Changes the values of the row at |row|.
  void ChangeRow(int row, int c1_value, int c2_value);

  // ui::TableModel:
  virtual int RowCount() OVERRIDE;
  virtual string16 GetText(int row, int column_id) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;
  virtual int CompareValues(int row1, int row2, int column_id) OVERRIDE;

 private:
  ui::TableModelObserver* observer_;

  // The data.
  std::vector<std::vector<int> > rows_;

  DISALLOW_COPY_AND_ASSIGN(TestTableModel2);
};

TestTableModel2::TestTableModel2() : observer_(NULL) {
  AddRow(0, 0, 1);
  AddRow(1, 1, 1);
  AddRow(2, 2, 2);
  AddRow(3, 3, 0);
}

void TestTableModel2::AddRow(int row, int c1_value, int c2_value) {
  DCHECK(row >= 0 && row <= static_cast<int>(rows_.size()));
  std::vector<int> new_row;
  new_row.push_back(c1_value);
  new_row.push_back(c2_value);
  rows_.insert(rows_.begin() + row, new_row);
  if (observer_)
    observer_->OnItemsAdded(row, 1);
}
void TestTableModel2::RemoveRow(int row) {
  DCHECK(row >= 0 && row <= static_cast<int>(rows_.size()));
  rows_.erase(rows_.begin() + row);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
}

void TestTableModel2::ChangeRow(int row, int c1_value, int c2_value) {
  DCHECK(row >= 0 && row < static_cast<int>(rows_.size()));
  rows_[row][0] = c1_value;
  rows_[row][1] = c2_value;
  if (observer_)
    observer_->OnItemsChanged(row, 1);
}

int TestTableModel2::RowCount() {
  return static_cast<int>(rows_.size());
}

string16 TestTableModel2::GetText(int row, int column_id) {
  return base::IntToString16(rows_[row][column_id]);
}

void TestTableModel2::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

int TestTableModel2::CompareValues(int row1, int row2, int column_id) {
  return rows_[row1][column_id] - rows_[row2][column_id];
}

// Returns the view to model mapping as a string.
std::string GetViewToModelAsString(TableView* table) {
  std::string result;
  for (int i = 0; i < table->RowCount(); ++i) {
    if (i != 0)
      result += " ";
    result += base::IntToString(table->ViewToModel(i));
  }
  return result;
}

// Returns the model to view mapping as a string.
std::string GetModelToViewAsString(TableView* table) {
  std::string result;
  for (int i = 0; i < table->RowCount(); ++i) {
    if (i != 0)
      result += " ";
    result += base::IntToString(table->ModelToView(i));
  }
  return result;
}

}  // namespace

class TableViewTest : public testing::Test {
 public:
  TableViewTest() : table_(NULL) {}

  virtual void SetUp() OVERRIDE {
    model_.reset(new TestTableModel2);
    std::vector<ui::TableColumn> columns(2);
    columns[0].title = ASCIIToUTF16("Title Column 0");
    columns[0].sortable = true;
    columns[1].title = ASCIIToUTF16("Title Column 1");
    columns[1].id = 1;
    columns[1].sortable = true;
    table_ = new TableView(model_.get(), columns, TEXT_ONLY, true, true, true);
    parent_.reset(table_->CreateParentIfNecessary());
    parent_->SetBounds(0, 0, 10000, 10000);
    parent_->Layout();
    helper_.reset(new TableViewTestHelper(table_));
  }

 protected:
  scoped_ptr<TestTableModel2> model_;

  // Owned by |parent_|.
  TableView* table_;

  scoped_ptr<TableViewTestHelper> helper_;

 private:
  scoped_ptr<View> parent_;

  DISALLOW_COPY_AND_ASSIGN(TableViewTest);
};

// Verifies GetPaintRegion.
TEST_F(TableViewTest, GetPaintRegion) {
  // Two columns should be visible.
  EXPECT_EQ(2u, helper_->visible_col_count());

  EXPECT_EQ("rows=0 4 cols=0 2", helper_->GetPaintRegion(table_->bounds()));
  EXPECT_EQ("rows=0 4 cols=0 1",
            helper_->GetPaintRegion(gfx::Rect(0, 0, 1, table_->height())));
}

// Verifies SetColumnVisibility().
TEST_F(TableViewTest, ColumnVisibility) {
  // Two columns should be visible.
  EXPECT_EQ(2u, helper_->visible_col_count());

  // Should do nothing (column already visible).
  table_->SetColumnVisibility(0, true);
  EXPECT_EQ(2u, helper_->visible_col_count());

  // Hide the first column.
  table_->SetColumnVisibility(0, false);
  ASSERT_EQ(1u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->visible_columns()[0].column.id);
  EXPECT_EQ("rows=0 4 cols=0 1", helper_->GetPaintRegion(table_->bounds()));

  // Hide the second column.
  table_->SetColumnVisibility(1, false);
  EXPECT_EQ(0u, helper_->visible_col_count());

  // Show the second column.
  table_->SetColumnVisibility(1, true);
  ASSERT_EQ(1u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->visible_columns()[0].column.id);
  EXPECT_EQ("rows=0 4 cols=0 1", helper_->GetPaintRegion(table_->bounds()));

  // Show the first column.
  table_->SetColumnVisibility(0, true);
  ASSERT_EQ(2u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->visible_columns()[0].column.id);
  EXPECT_EQ(0, table_->visible_columns()[1].column.id);
  EXPECT_EQ("rows=0 4 cols=0 2", helper_->GetPaintRegion(table_->bounds()));
}

// Verifies resizing a column works.
TEST_F(TableViewTest, Resize) {
  const int x = table_->visible_columns()[0].width;
  EXPECT_NE(0, x);
  // Drag the mouse 1 pixel to the left.
  const ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(x, 0),
                               gfx::Point(x, 0), ui::EF_LEFT_MOUSE_BUTTON);
  helper_->header()->OnMousePressed(pressed);
  const ui::MouseEvent dragged(ui::ET_MOUSE_DRAGGED, gfx::Point(x - 1, 0),
                               gfx::Point(x - 1, 0), ui::EF_LEFT_MOUSE_BUTTON);
  helper_->header()->OnMouseDragged(dragged);

  // This should shrink the first column and pull the second column in.
  EXPECT_EQ(x - 1, table_->visible_columns()[0].width);
  EXPECT_EQ(x - 1, table_->visible_columns()[1].x);
}

// Assertions for a sorted table.
TEST_F(TableViewTest, Sort) {
  // Toggle the sort order of the first column, shouldn't change anything.
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));

  // Invert the sort (first column descending).
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 2 1 0", GetViewToModelAsString(table_));
  EXPECT_EQ("3 2 1 0", GetModelToViewAsString(table_));

  // Change cell 0x3 to -1, meaning we have 0, 1, 2, -1 (in the first column).
  model_->ChangeRow(3, -1, 0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("2 1 0 3", GetViewToModelAsString(table_));
  EXPECT_EQ("2 1 0 3", GetModelToViewAsString(table_));

  // Invert sort again (first column ascending).
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 0 1 2", GetViewToModelAsString(table_));
  EXPECT_EQ("1 2 3 0", GetModelToViewAsString(table_));

  // Add a row so that model has 0, 3, 1, 2, -1.
  model_->AddRow(1, 3, 4);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("4 0 2 3 1", GetViewToModelAsString(table_));
  EXPECT_EQ("1 4 2 3 0", GetModelToViewAsString(table_));

  // Delete the first row, ending up with 3, 1, 2, -1.
  model_->RemoveRow(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 1 2 0", GetViewToModelAsString(table_));
  EXPECT_EQ("3 1 2 0", GetModelToViewAsString(table_));
}

}  // namespace views
