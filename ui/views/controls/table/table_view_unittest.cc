// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For WinDDK ATL compatibility, these ATL headers must come first.

// TODO(sky): nuke this once TableViewViews stabilizes.

#include "build/build_config.h"

#include <atlbase.h>  // NOLINT
#include <atlwin.h>  // NOLINT
#include <vector>  // NOLINT

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/table_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

// Put the tests in the views namespace to make it easier to declare them as
// friend classes.
namespace views {

// TestTableModel --------------------------------------------------------------

// Trivial TableModel implementation that is backed by a vector of vectors.
// Provides methods for adding/removing/changing the contents that notify the
// observer appropriately.
//
// Initial contents are:
// 0, 1
// 1, 1
// 2, 2
class TestTableModel : public ui::TableModel {
 public:
  TestTableModel();

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

  DISALLOW_COPY_AND_ASSIGN(TestTableModel);
};

// Same behavior as TestTableModel, except even items are in one group, while
// odd items are put in a different group.
class GroupTestTableModel : public TestTableModel {
  virtual bool HasGroups() { return true; }

  virtual Groups GetGroups() {
    Groups groups;
    Group group1, group2;
    group1.title = ASCIIToUTF16("Group 1");
    group1.id = 0;
    group2.title = ASCIIToUTF16("Group 2");
    group2.id = 0;
    groups.push_back(group1);
    groups.push_back(group2);
    return groups;
  }

  // Return group = 0 if row is even, otherwise group = 1.
  virtual int GetGroupID(int row) { return row % 2; }
};

TestTableModel::TestTableModel() : observer_(NULL) {
  AddRow(0, 0, 1);
  AddRow(1, 1, 1);
  AddRow(2, 2, 2);
}

void TestTableModel::AddRow(int row, int c1_value, int c2_value) {
  DCHECK(row >= 0 && row <= static_cast<int>(rows_.size()));
  std::vector<int> new_row;
  new_row.push_back(c1_value);
  new_row.push_back(c2_value);
  rows_.insert(rows_.begin() + row, new_row);
  if (observer_)
    observer_->OnItemsAdded(row, 1);
}
void TestTableModel::RemoveRow(int row) {
  DCHECK(row >= 0 && row <= static_cast<int>(rows_.size()));
  rows_.erase(rows_.begin() + row);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
}

void TestTableModel::ChangeRow(int row, int c1_value, int c2_value) {
  DCHECK(row >= 0 && row < static_cast<int>(rows_.size()));
  rows_[row][0] = c1_value;
  rows_[row][1] = c2_value;
  if (observer_)
    observer_->OnItemsChanged(row, 1);
}

int TestTableModel::RowCount() {
  return static_cast<int>(rows_.size());
}

string16 TestTableModel::GetText(int row, int column_id) {
  return UTF8ToUTF16(base::IntToString(rows_[row][column_id]));
}

void TestTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

int TestTableModel::CompareValues(int row1, int row2, int column_id) {
  return rows_[row1][column_id] - rows_[row2][column_id];
}

// TableViewTest ---------------------------------------------------------------

class TableViewTest : public ViewsTestBase, views::WidgetDelegate {
 public:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  virtual views::View* GetContentsView() OVERRIDE {
    return table_;
  }
  virtual views::Widget* GetWidget() OVERRIDE {
    return table_->GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return table_->GetWidget();
  }

 protected:
  // Creates the model.
  virtual TestTableModel* CreateModel();

  // Verifies the view order matches that of the supplied arguments. The
  // arguments are in terms of the model. For example, values of '1, 0' indicate
  // the model index at row 0 is 1 and the model index at row 1 is 0.
  void VerifyViewOrder(int first, ...);

  // Verifies the selection matches the supplied arguments. The supplied
  // arguments are in terms of this model. This uses the iterator returned by
  // SelectionBegin.
  void VerifySelectedRows(int first, ...);

  // Configures the state for the various multi-selection tests.
  // This selects model rows 0 and 1, and if |sort| is true the first column
  // is sorted in descending order.
  void SetUpMultiSelectTestState(bool sort);

  scoped_ptr<TestTableModel> model_;

  // The table. This is owned by the window.
  TableView* table_;

 private:
  views::Widget* window_;
};

void TableViewTest::SetUp() {
  ViewsTestBase::SetUp();

  model_.reset(CreateModel());
  std::vector<ui::TableColumn> columns;
  columns.resize(2);
  columns[0].id = 0;
  columns[1].id = 1;

  // TODO(erg): This crashes on windows. Try making this derive from ViewsTests.
  table_ = new TableView(model_.get(), columns, views::ICON_AND_TEXT,
                         false, false, false);
  window_ = views::Widget::CreateWindowWithContextAndBounds(
      this, GetContext(), gfx::Rect(100, 100, 512, 512));
}

void TableViewTest::TearDown() {
  window_->Close();

  ViewsTestBase::TearDown();
}

void TableViewTest::VerifyViewOrder(int first, ...) {
  va_list marker;
  va_start(marker, first);
  int value = first;
  int index = 0;
  for (int value = first, index = 0; value != -1; index++) {
    ASSERT_EQ(value, table_->ViewToModel(index));
    value = va_arg(marker, int);
  }
  va_end(marker);
}

void TableViewTest::VerifySelectedRows(int first, ...) {
  va_list marker;
  va_start(marker, first);
  int value = first;
  int index = 0;
  TableView::iterator selection_iterator = table_->SelectionBegin();
  for (int value = first, index = 0; value != -1; index++) {
    ASSERT_TRUE(selection_iterator != table_->SelectionEnd());
    ASSERT_EQ(value, *selection_iterator);
    value = va_arg(marker, int);
    ++selection_iterator;
  }
  ASSERT_TRUE(selection_iterator == table_->SelectionEnd());
  va_end(marker);
}

void TableViewTest::SetUpMultiSelectTestState(bool sort) {
  // Select two rows.
  table_->SetSelectedState(0, true);
  table_->SetSelectedState(1, true);

  VerifySelectedRows(1, 0, -1);
  if (!sort || HasFatalFailure())
    return;

  // Sort by first column descending.
  TableView::SortDescriptors sd;
  sd.push_back(TableView::SortDescriptor(0, false));
  table_->SetSortDescriptors(sd);
  VerifyViewOrder(2, 1, 0, -1);
  if (HasFatalFailure())
    return;

  // Make sure the two rows are sorted.
  // NOTE: the order changed because iteration happens over view indices.
  VerifySelectedRows(0, 1, -1);
}

TestTableModel* TableViewTest::CreateModel() {
  return new TestTableModel();
}

// NullModelTableViewTest ------------------------------------------------------

class NullModelTableViewTest : public TableViewTest {
 protected:
  // Creates the model.
  TestTableModel* CreateModel() {
    return NULL;
  }
};

// GroupModelTableViewTest -----------------------------------------------------
class GroupModelTableViewTest : public TableViewTest {
 protected:
  TestTableModel* CreateModel() {
    return new GroupTestTableModel();
  }
};

// Tests -----------------------------------------------------------------------

// Failing: http://crbug.com/45015
// Tests various sorting permutations.
TEST_F(TableViewTest, DISABLED_Sort) {
  // Sort by first column descending.
  TableView::SortDescriptors sort;
  sort.push_back(TableView::SortDescriptor(0, false));
  table_->SetSortDescriptors(sort);
  VerifyViewOrder(2, 1, 0, -1);
  if (HasFatalFailure())
    return;

  // Sort by second column ascending, first column descending.
  sort.clear();
  sort.push_back(TableView::SortDescriptor(1, true));
  sort.push_back(TableView::SortDescriptor(0, false));
  sort[1].ascending = false;
  table_->SetSortDescriptors(sort);
  VerifyViewOrder(1, 0, 2, -1);
  if (HasFatalFailure())
    return;

  // Clear the sort.
  table_->SetSortDescriptors(TableView::SortDescriptors());
  VerifyViewOrder(0, 1, 2, -1);
  if (HasFatalFailure())
    return;
}

// Failing: http://crbug.com/45015
// Tests changing the model while sorted.
TEST_F(TableViewTest, DISABLED_SortThenChange) {
  // Sort by first column descending.
  TableView::SortDescriptors sort;
  sort.push_back(TableView::SortDescriptor(0, false));
  table_->SetSortDescriptors(sort);
  VerifyViewOrder(2, 1, 0, -1);
  if (HasFatalFailure())
    return;

  model_->ChangeRow(0, 3, 1);
  VerifyViewOrder(0, 2, 1, -1);
}

// Failing: http://crbug.com/45015
// Tests adding to the model while sorted.
TEST_F(TableViewTest, DISABLED_AddToSorted) {
  // Sort by first column descending.
  TableView::SortDescriptors sort;
  sort.push_back(TableView::SortDescriptor(0, false));
  table_->SetSortDescriptors(sort);
  VerifyViewOrder(2, 1, 0, -1);
  if (HasFatalFailure())
    return;

  // Add row so that it occurs first.
  model_->AddRow(0, 5, -1);
  VerifyViewOrder(0, 3, 2, 1, -1);
  if (HasFatalFailure())
    return;

  // Add row so that it occurs last.
  model_->AddRow(0, -1, -1);
  VerifyViewOrder(1, 4, 3, 2, 0, -1);
}

// Failing: http://crbug.com/45015
// Tests selection on sort.
TEST_F(TableViewTest, DISABLED_PersistSelectionOnSort) {
  // Select row 0.
  table_->Select(0);

  // Sort by first column descending.
  TableView::SortDescriptors sort;
  sort.push_back(TableView::SortDescriptor(0, false));
  table_->SetSortDescriptors(sort);
  VerifyViewOrder(2, 1, 0, -1);
  if (HasFatalFailure())
    return;

  // Make sure 0 is still selected.
  EXPECT_EQ(0, table_->FirstSelectedRow());
}

// Failing: http://crbug.com/45015
// Tests selection iterator with sort.
TEST_F(TableViewTest, DISABLED_PersistMultiSelectionOnSort) {
  SetUpMultiSelectTestState(true);
}

// Failing: http://crbug.com/45015
// Tests selection persists after a change when sorted with iterator.
TEST_F(TableViewTest, DISABLED_PersistMultiSelectionOnChangeWithSort) {
  SetUpMultiSelectTestState(true);
  if (HasFatalFailure())
    return;

  model_->ChangeRow(0, 3, 1);

  VerifySelectedRows(1, 0, -1);
}

// Failing: http://crbug.com/45015
// Tests selection persists after a remove when sorted with iterator.
TEST_F(TableViewTest, DISABLED_PersistMultiSelectionOnRemoveWithSort) {
  SetUpMultiSelectTestState(true);
  if (HasFatalFailure())
    return;

  model_->RemoveRow(0);

  VerifySelectedRows(0, -1);
}

// Failing: http://crbug.com/45015
// Tests selection persists after a add when sorted with iterator.
TEST_F(TableViewTest, DISABLED_PersistMultiSelectionOnAddWithSort) {
  SetUpMultiSelectTestState(true);
  if (HasFatalFailure())
    return;

  model_->AddRow(3, 4, 4);

  VerifySelectedRows(0, 1, -1);
}

// Failing: http://crbug.com/45015
// Tests selection persists after a change with iterator.
TEST_F(TableViewTest, DISABLED_PersistMultiSelectionOnChange) {
  SetUpMultiSelectTestState(false);
  if (HasFatalFailure())
    return;

  model_->ChangeRow(0, 3, 1);

  VerifySelectedRows(1, 0, -1);
}

// Failing: http://crbug.com/45015
// Tests selection persists after a remove with iterator.
TEST_F(TableViewTest, DISABLED_PersistMultiSelectionOnRemove) {
  SetUpMultiSelectTestState(false);
  if (HasFatalFailure())
    return;

  model_->RemoveRow(0);

  VerifySelectedRows(0, -1);
}

// Failing: http://crbug.com/45015
// Tests selection persists after a add with iterator.
TEST_F(TableViewTest, DISABLED_PersistMultiSelectionOnAdd) {
  SetUpMultiSelectTestState(false);
  if (HasFatalFailure())
    return;

  model_->AddRow(3, 4, 4);

  VerifySelectedRows(1, 0, -1);
}

TEST_F(GroupModelTableViewTest, IndividualSelectAcrossGroups) {
  table_->SetSelectedState(0, true);
  table_->SetSelectedState(1, true);
  table_->SetSelectedState(2, true);
  VerifySelectedRows(2, 1, 0, -1);
}

TEST_F(GroupModelTableViewTest, ShiftSelectAcrossGroups) {
  table_->SetSelectedState(0, true);
  // Try to select across groups - this should fail.
  ASSERT_FALSE(table_->SelectMultiple(1, 0));
  VerifySelectedRows(0, -1);
}

TEST_F(GroupModelTableViewTest, ShiftSelectSameGroup) {
  table_->SetSelectedState(0, true);
  // Try to select in the same group - this should work but should only select
  // items in the "even" group.
  ASSERT_TRUE(table_->SelectMultiple(2, 0));
  VerifySelectedRows(2, 0, -1);
}

// Crashing: http://crbug.com/45015
TEST_F(NullModelTableViewTest, DISABLED_NullModel) {
  // There's nothing explicit to test. If there is a bug in TableView relating
  // to a NULL model we'll crash.
}

}  // namespace views
