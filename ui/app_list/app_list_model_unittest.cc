// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_model.h"

#include <map>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/base/models/list_model_observer.h"

namespace app_list {

namespace {

class TestObserver : public AppListModelObserver,
                     public AppListItemListObserver {
 public:
  TestObserver()
      : status_changed_count_(0),
        items_added_(0),
        items_removed_(0),
        items_moved_(0) {
  }
  virtual ~TestObserver() {
  }

  // AppListModelObserver
  virtual void OnAppListModelStatusChanged() OVERRIDE {
    ++status_changed_count_;
  }

  // AppListItemListObserver
  virtual void OnListItemAdded(size_t index, AppListItemModel* item) OVERRIDE {
    items_added_++;
  }

  virtual void OnListItemRemoved(size_t index,
                                 AppListItemModel* item) OVERRIDE {
    items_removed_++;
  }

  virtual void OnListItemMoved(size_t from_index,
                               size_t to_index,
                               AppListItemModel* item) OVERRIDE {
    items_moved_++;
  }

  int status_changed_count() const { return status_changed_count_; }
  int signin_changed_count() const { return signin_changed_count_; }
  size_t items_added() { return items_added_; }
  size_t items_removed() { return items_removed_; }
  size_t items_moved() { return items_moved_; }

  void ResetCounts() {
    status_changed_count_ = 0;
    signin_changed_count_ = 0;
    items_added_ = 0;
    items_removed_ = 0;
    items_moved_ = 0;
  }

 private:
  int status_changed_count_;
  int signin_changed_count_;
  size_t items_added_;
  size_t items_removed_;
  size_t items_moved_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class AppListModelTest : public testing::Test {
 public:
  AppListModelTest() {}
  virtual ~AppListModelTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    model_.AddObserver(&observer_);
    model_.item_list()->AddObserver(&observer_);
  }
  virtual void TearDown() OVERRIDE {
    model_.RemoveObserver(&observer_);
    model_.item_list()->RemoveObserver(&observer_);
  }

 protected:
  bool ItemObservedByFolder(AppListFolderItem* folder,
                            AppListItemModel* item) {
    return item->observers_.HasObserver(folder);
  }

  test::AppListTestModel model_;
  TestObserver observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListModelTest);
};

TEST_F(AppListModelTest, SetStatus) {
  EXPECT_EQ(AppListModel::STATUS_NORMAL, model_.status());
  model_.SetStatus(AppListModel::STATUS_SYNCING);
  EXPECT_EQ(1, observer_.status_changed_count());
  EXPECT_EQ(AppListModel::STATUS_SYNCING, model_.status());
  model_.SetStatus(AppListModel::STATUS_NORMAL);
  EXPECT_EQ(2, observer_.status_changed_count());
  // Set the same status, no change is expected.
  model_.SetStatus(AppListModel::STATUS_NORMAL);
  EXPECT_EQ(2, observer_.status_changed_count());
}

TEST_F(AppListModelTest, AppsObserver) {
  const size_t num_apps = 2;
  model_.PopulateApps(num_apps);
  EXPECT_EQ(num_apps, observer_.items_added());
}

TEST_F(AppListModelTest, ModelGetItem) {
  const size_t num_apps = 2;
  model_.PopulateApps(num_apps);
  AppListItemModel* item0 = model_.item_list()->item_at(0);
  ASSERT_TRUE(item0);
  EXPECT_EQ(model_.GetItemName(0), item0->id());
  AppListItemModel* item1 = model_.item_list()->item_at(1);
  ASSERT_TRUE(item1);
  EXPECT_EQ(model_.GetItemName(1), item1->id());
}

TEST_F(AppListModelTest, ModelFindItem) {
  const size_t num_apps = 2;
  model_.PopulateApps(num_apps);
  std::string item_name0 = model_.GetItemName(0);
  AppListItemModel* item0 = model_.item_list()->FindItem(item_name0);
  ASSERT_TRUE(item0);
  EXPECT_EQ(item_name0, item0->id());
  std::string item_name1 = model_.GetItemName(1);
  AppListItemModel* item1 = model_.item_list()->FindItem(item_name1);
  ASSERT_TRUE(item1);
  EXPECT_EQ(item_name1, item1->id());
}

TEST_F(AppListModelTest, ModelAddItem) {
  const size_t num_apps = 2;
  model_.PopulateApps(num_apps);
  // Adding another item will add it to the end.
  model_.CreateAndAddItem("Added Item 1");
  ASSERT_EQ(num_apps + 1, model_.item_list()->item_count());
  EXPECT_EQ("Added Item 1", model_.item_list()->item_at(num_apps)->id());
  // Add an item between items 0 and 1.
  AppListItemModel* item0 = model_.item_list()->item_at(0);
  ASSERT_TRUE(item0);
  AppListItemModel* item1 = model_.item_list()->item_at(1);
  ASSERT_TRUE(item1);
  AppListItemModel* item2 = model_.CreateItem("Added Item 2", "Added Item 2");
  model_.item_list()->AddItem(item2);
  model_.item_list()->SetItemPosition(
      item2, item0->position().CreateBetween(item1->position()));
  EXPECT_EQ(num_apps + 2, model_.item_list()->item_count());
  EXPECT_EQ("Added Item 2", model_.item_list()->item_at(1)->id());
}

TEST_F(AppListModelTest, ModelMoveItem) {
  const size_t num_apps = 3;
  model_.PopulateApps(num_apps);
  // Adding another item will add it to the end.
  model_.CreateAndAddItem("Inserted Item");
  ASSERT_EQ(num_apps + 1, model_.item_list()->item_count());
  // Move it to the position 1.
  model_.item_list()->MoveItem(num_apps, 1);
  AppListItemModel* item = model_.item_list()->item_at(1);
  ASSERT_TRUE(item);
  EXPECT_EQ("Inserted Item", item->id());
}

TEST_F(AppListModelTest, ModelRemoveItem) {
  const size_t num_apps = 4;
  model_.PopulateApps(num_apps);
  // Remove an item in the middle.
  model_.item_list()->DeleteItem(model_.GetItemName(1));
  EXPECT_EQ(num_apps - 1, model_.item_list()->item_count());
  EXPECT_EQ(1u, observer_.items_removed());
  // Remove the first item in the list.
  model_.item_list()->DeleteItem(model_.GetItemName(0));
  EXPECT_EQ(num_apps - 2, model_.item_list()->item_count());
  EXPECT_EQ(2u, observer_.items_removed());
  // Remove the last item in the list.
  model_.item_list()->DeleteItem(model_.GetItemName(num_apps - 1));
  EXPECT_EQ(num_apps - 3, model_.item_list()->item_count());
  EXPECT_EQ(3u, observer_.items_removed());
  // Ensure that the first item is the expected one
  AppListItemModel* item0 = model_.item_list()->item_at(0);
  ASSERT_TRUE(item0);
  EXPECT_EQ(model_.GetItemName(2), item0->id());
}

TEST_F(AppListModelTest, ModelRemoveItemByType) {
  const size_t num_apps = 4;
  model_.PopulateApps(num_apps);
  model_.item_list()->AddItem(new AppListFolderItem("folder1"));
  model_.item_list()->AddItem(new AppListFolderItem("folder2"));
  model_.item_list()->DeleteItemsByType(test::AppListTestModel::kAppType);
  EXPECT_EQ(num_apps, observer_.items_removed());
  EXPECT_EQ(2u, model_.item_list()->item_count());
  model_.item_list()->DeleteItemsByType(AppListFolderItem::kAppType);
  EXPECT_EQ(num_apps + 2, observer_.items_removed());
  EXPECT_EQ(0u, model_.item_list()->item_count());
  // Delete all items
  observer_.ResetCounts();
  model_.PopulateApps(num_apps);
  model_.item_list()->AddItem(new AppListFolderItem("folder1"));
  model_.item_list()->AddItem(new AppListFolderItem("folder2"));
  model_.item_list()->DeleteItemsByType(NULL /* all items */);
  EXPECT_EQ(num_apps + 2, observer_.items_removed());
  EXPECT_EQ(0u, model_.item_list()->item_count());
}

TEST_F(AppListModelTest, AppOrder) {
  const size_t num_apps = 5;
  model_.PopulateApps(num_apps);
  // Ensure order is preserved.
  for (size_t i = 1; i < num_apps; ++i) {
    EXPECT_TRUE(model_.item_list()->item_at(i)->position().GreaterThan(
        model_.item_list()->item_at(i - 1)->position()));
  }
  // Move an app
  model_.item_list()->MoveItem(num_apps - 1, 1);
  // Ensure order is preserved.
  for (size_t i = 1; i < num_apps; ++i) {
    EXPECT_TRUE(model_.item_list()->item_at(i)->position().GreaterThan(
        model_.item_list()->item_at(i - 1)->position()));
  }
}

TEST_F(AppListModelTest, FolderItem) {
  scoped_ptr<AppListFolderItem> folder(new AppListFolderItem("folder1"));
  const size_t num_folder_apps = 8;
  const size_t num_observed_apps = 4;
  for (int i = 0; static_cast<size_t>(i) < num_folder_apps; ++i) {
    std::string name = model_.GetItemName(i);
    folder->item_list()->AddItem(model_.CreateItem(name, name));
  }
  // Check that items 0 and 3 are observed.
  EXPECT_TRUE(ItemObservedByFolder(
      folder.get(), folder->item_list()->item_at(0)));
  EXPECT_TRUE(ItemObservedByFolder(
      folder.get(), folder->item_list()->item_at(num_observed_apps - 1)));
  // Check that item 4 is not observed.
  EXPECT_FALSE(ItemObservedByFolder(
      folder.get(), folder->item_list()->item_at(num_observed_apps)));
  folder->item_list()->MoveItem(num_observed_apps, 0);
  // Confirm that everything was moved where expected.
  EXPECT_EQ(model_.GetItemName(num_observed_apps),
            folder->item_list()->item_at(0)->id());
  EXPECT_EQ(model_.GetItemName(0),
            folder->item_list()->item_at(1)->id());
  EXPECT_EQ(model_.GetItemName(num_observed_apps - 1),
            folder->item_list()->item_at(num_observed_apps)->id());
  // Check that items 0 and 3 are observed.
  EXPECT_TRUE(ItemObservedByFolder(
      folder.get(), folder->item_list()->item_at(0)));
  EXPECT_TRUE(ItemObservedByFolder(
      folder.get(), folder->item_list()->item_at(num_observed_apps - 1)));
  // Check that item 4 is not observed.
  EXPECT_FALSE(ItemObservedByFolder(
      folder.get(), folder->item_list()->item_at(num_observed_apps)));
}

}  // namespace app_list
