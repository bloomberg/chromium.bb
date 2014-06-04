// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_model.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/base/models/list_model_observer.h"

namespace app_list {

namespace {

class TestObserver : public AppListModelObserver {
 public:
  TestObserver()
      : status_changed_count_(0),
        items_added_(0),
        items_removed_(0),
        items_updated_(0) {
  }
  virtual ~TestObserver() {
  }

  // AppListModelObserver
  virtual void OnAppListModelStatusChanged() OVERRIDE {
    ++status_changed_count_;
  }

  virtual void OnAppListItemAdded(AppListItem* item) OVERRIDE {
    items_added_++;
  }

  virtual void OnAppListItemWillBeDeleted(AppListItem* item) OVERRIDE {
    items_removed_++;
  }

  virtual void OnAppListItemUpdated(AppListItem* item) OVERRIDE {
    items_updated_++;
  }

  int status_changed_count() const { return status_changed_count_; }
  size_t items_added() { return items_added_; }
  size_t items_removed() { return items_removed_; }
  size_t items_updated() { return items_updated_; }

  void ResetCounts() {
    status_changed_count_ = 0;
    items_added_ = 0;
    items_removed_ = 0;
    items_updated_ = 0;
  }

 private:
  int status_changed_count_;
  size_t items_added_;
  size_t items_removed_;
  size_t items_updated_;

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
  }
  virtual void TearDown() OVERRIDE {
    model_.RemoveObserver(&observer_);
  }

 protected:
  bool ItemObservedByFolder(AppListFolderItem* folder,
                            AppListItem* item) {
    return item->observers_.HasObserver(folder);
  }

  std::string GetItemListContents(AppListItemList* item_list) {
    std::string s;
    for (size_t i = 0; i < item_list->item_count(); ++i) {
      if (i != 0)
        s += ",";
      s += item_list->item_at(i)->id();
    }
    return s;
  }

  std::string GetModelContents() {
    return GetItemListContents(model_.top_level_item_list());
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
  AppListItem* item0 = model_.top_level_item_list()->item_at(0);
  ASSERT_TRUE(item0);
  EXPECT_EQ(model_.GetItemName(0), item0->id());
  AppListItem* item1 = model_.top_level_item_list()->item_at(1);
  ASSERT_TRUE(item1);
  EXPECT_EQ(model_.GetItemName(1), item1->id());
}

TEST_F(AppListModelTest, ModelFindItem) {
  const size_t num_apps = 2;
  model_.PopulateApps(num_apps);
  std::string item_name0 = model_.GetItemName(0);
  AppListItem* item0 = model_.FindItem(item_name0);
  ASSERT_TRUE(item0);
  EXPECT_EQ(item_name0, item0->id());
  std::string item_name1 = model_.GetItemName(1);
  AppListItem* item1 = model_.FindItem(item_name1);
  ASSERT_TRUE(item1);
  EXPECT_EQ(item_name1, item1->id());
}

TEST_F(AppListModelTest, SetItemPosition) {
  const size_t num_apps = 2;
  model_.PopulateApps(num_apps);
  // Adding another item will add it to the end.
  model_.CreateAndAddItem("Added Item 1");
  ASSERT_EQ(num_apps + 1, model_.top_level_item_list()->item_count());
  EXPECT_EQ("Added Item 1",
            model_.top_level_item_list()->item_at(num_apps)->id());
  // Add an item between items 0 and 1.
  AppListItem* item0 = model_.top_level_item_list()->item_at(0);
  ASSERT_TRUE(item0);
  AppListItem* item1 = model_.top_level_item_list()->item_at(1);
  ASSERT_TRUE(item1);
  AppListItem* item2 = model_.CreateItem("Added Item 2");
  model_.AddItem(item2);
  EXPECT_EQ("Item 0,Item 1,Added Item 1,Added Item 2", GetModelContents());
  model_.SetItemPosition(
      item2, item0->position().CreateBetween(item1->position()));
  EXPECT_EQ(num_apps + 2, model_.top_level_item_list()->item_count());
  EXPECT_EQ(num_apps + 2, observer_.items_added());
  EXPECT_EQ("Item 0,Added Item 2,Item 1,Added Item 1", GetModelContents());
}

TEST_F(AppListModelTest, ModelMoveItem) {
  const size_t num_apps = 3;
  model_.PopulateApps(num_apps);
  // Adding another item will add it to the end.
  model_.CreateAndAddItem("Inserted Item");
  ASSERT_EQ(num_apps + 1, model_.top_level_item_list()->item_count());
  // Move it to the position 1.
  observer_.ResetCounts();
  model_.top_level_item_list()->MoveItem(num_apps, 1);
  EXPECT_EQ(1u, observer_.items_updated());
  EXPECT_EQ("Item 0,Inserted Item,Item 1,Item 2", GetModelContents());
}

TEST_F(AppListModelTest, ModelRemoveItem) {
  const size_t num_apps = 4;
  model_.PopulateApps(num_apps);
  // Remove an item in the middle.
  model_.DeleteItem(model_.GetItemName(1));
  EXPECT_EQ(num_apps - 1, model_.top_level_item_list()->item_count());
  EXPECT_EQ(1u, observer_.items_removed());
  EXPECT_EQ("Item 0,Item 2,Item 3", GetModelContents());
  // Remove the first item in the list.
  model_.DeleteItem(model_.GetItemName(0));
  EXPECT_EQ(num_apps - 2, model_.top_level_item_list()->item_count());
  EXPECT_EQ(2u, observer_.items_removed());
  EXPECT_EQ("Item 2,Item 3", GetModelContents());
  // Remove the last item in the list.
  model_.DeleteItem(model_.GetItemName(num_apps - 1));
  EXPECT_EQ(num_apps - 3, model_.top_level_item_list()->item_count());
  EXPECT_EQ(3u, observer_.items_removed());
  EXPECT_EQ("Item 2", GetModelContents());
}

TEST_F(AppListModelTest, AppOrder) {
  const size_t num_apps = 5;
  model_.PopulateApps(num_apps);
  // Ensure order is preserved.
  for (size_t i = 1; i < num_apps; ++i) {
    EXPECT_TRUE(
        model_.top_level_item_list()->item_at(i)->position().GreaterThan(
            model_.top_level_item_list()->item_at(i - 1)->position()));
  }
  // Move an app
  model_.top_level_item_list()->MoveItem(num_apps - 1, 1);
  // Ensure order is preserved.
  for (size_t i = 1; i < num_apps; ++i) {
    EXPECT_TRUE(
        model_.top_level_item_list()->item_at(i)->position().GreaterThan(
            model_.top_level_item_list()->item_at(i - 1)->position()));
  }
}

class AppListModelFolderTest : public AppListModelTest {
 public:
  AppListModelFolderTest() {
    model_.SetFoldersEnabled(true);
  }
  virtual ~AppListModelFolderTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    AppListModelTest::SetUp();
  }
  virtual void TearDown() OVERRIDE {
    AppListModelTest::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListModelFolderTest);
};

TEST_F(AppListModelFolderTest, FolderItem) {
  AppListFolderItem* folder =
      new AppListFolderItem("folder1", AppListFolderItem::FOLDER_TYPE_NORMAL);
  const size_t num_folder_apps = 8;
  const size_t num_observed_apps = 4;
  model_.AddItem(folder);
  for (int i = 0; static_cast<size_t>(i) < num_folder_apps; ++i) {
    std::string name = model_.GetItemName(i);
    model_.AddItemToFolder(model_.CreateItem(name), folder->id());
  }
  ASSERT_EQ(num_folder_apps, folder->item_list()->item_count());
  // Check that items 0 and 3 are observed.
  EXPECT_TRUE(ItemObservedByFolder(
      folder, folder->item_list()->item_at(0)));
  EXPECT_TRUE(ItemObservedByFolder(
      folder, folder->item_list()->item_at(num_observed_apps - 1)));
  // Check that item 4 is not observed.
  EXPECT_FALSE(ItemObservedByFolder(
      folder, folder->item_list()->item_at(num_observed_apps)));
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
      folder, folder->item_list()->item_at(0)));
  EXPECT_TRUE(ItemObservedByFolder(
      folder, folder->item_list()->item_at(num_observed_apps - 1)));
  // Check that item 4 is not observed.
  EXPECT_FALSE(ItemObservedByFolder(
      folder, folder->item_list()->item_at(num_observed_apps)));
}

TEST_F(AppListModelFolderTest, MergeItems) {
  model_.PopulateApps(3);
  ASSERT_EQ(3u, model_.top_level_item_list()->item_count());
  AppListItem* item0 = model_.top_level_item_list()->item_at(0);
  AppListItem* item1 = model_.top_level_item_list()->item_at(1);
  AppListItem* item2 = model_.top_level_item_list()->item_at(2);

  // Merge two items.
  std::string folder1_id = model_.MergeItems(item0->id(), item1->id());
  ASSERT_EQ(2u, model_.top_level_item_list()->item_count());  // Folder + 1 item
  AppListFolderItem* folder1_item = model_.FindFolderItem(folder1_id);
  ASSERT_TRUE(folder1_item);
  EXPECT_EQ("Item 0,Item 1", GetItemListContents(folder1_item->item_list()));

  // Merge an item from the new folder into the third item.
  std::string folder2_id = model_.MergeItems(item2->id(), item1->id());
  ASSERT_EQ(2u, model_.top_level_item_list()->item_count());  // 2 folders
  AppListFolderItem* folder2_item = model_.FindFolderItem(folder2_id);
  EXPECT_EQ("Item 0", GetItemListContents(folder1_item->item_list()));
  EXPECT_EQ("Item 2,Item 1", GetItemListContents(folder2_item->item_list()));

  // Merge the remaining item to the new folder, ensure it is added to the end.
  std::string folder_id = model_.MergeItems(folder2_id, item0->id());
  EXPECT_EQ(folder2_id, folder_id);
  EXPECT_EQ("Item 2,Item 1,Item 0",
            GetItemListContents(folder2_item->item_list()));

  // The empty folder should be deleted.
  folder1_item = model_.FindFolderItem(folder1_id);
  EXPECT_FALSE(folder1_item);
}

TEST_F(AppListModelFolderTest, AddItemToFolder) {
  AppListFolderItem* folder =
      new AppListFolderItem("folder1", AppListFolderItem::FOLDER_TYPE_NORMAL);
  model_.AddItem(folder);
  AppListItem* item0 = new AppListItem("Item 0");
  model_.AddItemToFolder(item0, folder->id());
  ASSERT_EQ(1u, model_.top_level_item_list()->item_count());
  AppListFolderItem* folder_item = model_.FindFolderItem(folder->id());
  ASSERT_TRUE(folder_item);
  ASSERT_EQ(1u, folder_item->item_list()->item_count());
  EXPECT_EQ(item0, folder_item->item_list()->item_at(0));
  EXPECT_EQ(folder->id(), item0->folder_id());
}

TEST_F(AppListModelFolderTest, MoveItemToFolder) {
  AppListFolderItem* folder =
      new AppListFolderItem("folder1", AppListFolderItem::FOLDER_TYPE_NORMAL);
  model_.AddItem(folder);
  AppListItem* item0 = new AppListItem("Item 0");
  AppListItem* item1 = new AppListItem("Item 1");
  model_.AddItem(item0);
  model_.AddItem(item1);
  ASSERT_EQ(3u, model_.top_level_item_list()->item_count());
  // Move item0 and item1 to folder.
  std::string folder_id = folder->id();
  model_.MoveItemToFolder(item0, folder_id);
  model_.MoveItemToFolder(item1, folder_id);
  AppListFolderItem* folder_item = model_.FindFolderItem(folder_id);
  ASSERT_TRUE(folder_item);
  EXPECT_EQ(folder_id, item0->folder_id());
  EXPECT_EQ(folder_id, item1->folder_id());
  EXPECT_EQ("Item 0,Item 1", GetItemListContents(folder_item->item_list()));
  // Move item0 out of folder.
  model_.MoveItemToFolder(item0, "");
  EXPECT_EQ("", item0->folder_id());
  folder_item = model_.FindFolderItem(folder_id);
  ASSERT_TRUE(folder_item);
  // Move item1 out of folder, folder should be deleted.
  model_.MoveItemToFolder(item1, "");
  EXPECT_EQ("", item1->folder_id());
  folder_item = model_.FindFolderItem(folder_id);
  EXPECT_FALSE(folder_item);
}

TEST_F(AppListModelFolderTest, MoveItemToFolderAt) {
  model_.AddItem(new AppListItem("Item 0"));
  model_.AddItem(new AppListItem("Item 1"));
  AppListFolderItem* folder1 = static_cast<AppListFolderItem*>(model_.AddItem(
      new AppListFolderItem("folder1", AppListFolderItem::FOLDER_TYPE_NORMAL)));
  model_.AddItem(new AppListItem("Item 2"));
  model_.AddItem(new AppListItem("Item 3"));
  ASSERT_EQ(5u, model_.top_level_item_list()->item_count());
  EXPECT_EQ("Item 0,Item 1,folder1,Item 2,Item 3", GetModelContents());
  // Move Item 1 to folder1, then Item 2 before Item 1.
  model_.MoveItemToFolderAt(model_.top_level_item_list()->item_at(1),
                            folder1->id(),
                            syncer::StringOrdinal());
  EXPECT_EQ("Item 0,folder1,Item 2,Item 3", GetModelContents());
  model_.MoveItemToFolderAt(model_.top_level_item_list()->item_at(2),
                            folder1->id(),
                            folder1->item_list()->item_at(0)->position());
  EXPECT_EQ("Item 2,Item 1", GetItemListContents(folder1->item_list()));
  EXPECT_EQ("Item 0,folder1,Item 3", GetModelContents());
  // Move Item 2 out of folder to before folder.
  model_.MoveItemToFolderAt(
      folder1->item_list()->item_at(0), "", folder1->position());
  EXPECT_EQ("Item 0,Item 2,folder1,Item 3", GetModelContents());
  // Move remaining folder item, (Item 1) out of folder to folder position.
  ASSERT_EQ(1u, folder1->item_list()->item_count());
  model_.MoveItemToFolderAt(
      folder1->item_list()->item_at(0), "", folder1->position());
  EXPECT_EQ("Item 0,Item 2,Item 1,Item 3", GetModelContents());
}

TEST_F(AppListModelFolderTest, MoveItemFromFolderToFolder) {
  AppListFolderItem* folder0 =
      new AppListFolderItem("folder0", AppListFolderItem::FOLDER_TYPE_NORMAL);
  AppListFolderItem* folder1 =
      new AppListFolderItem("folder1", AppListFolderItem::FOLDER_TYPE_NORMAL);
  model_.AddItem(folder0);
  model_.AddItem(folder1);
  EXPECT_EQ("folder0,folder1", GetModelContents());
  AppListItem* item0 = new AppListItem("Item 0");
  AppListItem* item1 = new AppListItem("Item 1");
  model_.AddItemToFolder(item0, folder0->id());
  model_.AddItemToFolder(item1, folder0->id());
  EXPECT_EQ(folder0->id(), item0->folder_id());
  EXPECT_EQ(folder0->id(), item1->folder_id());
  EXPECT_EQ("Item 0,Item 1", GetItemListContents(folder0->item_list()));

  // Move item0 from folder0 to folder1.
  model_.MoveItemToFolder(item0, folder1->id());
  ASSERT_EQ(1u, folder0->item_list()->item_count());
  ASSERT_EQ(1u, folder1->item_list()->item_count());
  EXPECT_EQ(folder1->id(), item0->folder_id());
  EXPECT_EQ("Item 1", GetItemListContents(folder0->item_list()));
  EXPECT_EQ("Item 0", GetItemListContents(folder1->item_list()));

  // Move item1 from folder0 to folder1. folder0 should get deleted.
  model_.MoveItemToFolder(item1, folder1->id());
  ASSERT_EQ(1u, model_.top_level_item_list()->item_count());
  ASSERT_EQ(2u, folder1->item_list()->item_count());
  EXPECT_EQ(folder1->id(), item1->folder_id());
  EXPECT_EQ("Item 0,Item 1", GetItemListContents(folder1->item_list()));

  // Move item1 to a non-existant folder2 which should get created.
  model_.MoveItemToFolder(item1, "folder2");
  ASSERT_EQ(2u, model_.top_level_item_list()->item_count());
  ASSERT_EQ(1u, folder1->item_list()->item_count());
  EXPECT_EQ("folder2", item1->folder_id());
  AppListFolderItem* folder2 = model_.FindFolderItem("folder2");
  ASSERT_TRUE(folder2);
}

TEST_F(AppListModelFolderTest, FindItemInFolder) {
  AppListFolderItem* folder =
      new AppListFolderItem("folder1", AppListFolderItem::FOLDER_TYPE_NORMAL);
  EXPECT_TRUE(folder);
  model_.AddItem(folder);
  std::string folder_id = folder->id();
  AppListItem* item0 = new AppListItem("Item 0");
  model_.AddItemToFolder(item0, folder_id);
  AppListItem* found_item = model_.FindItem(item0->id());
  ASSERT_EQ(item0, found_item);
  EXPECT_EQ(folder_id, found_item->folder_id());
}

TEST_F(AppListModelFolderTest, OemFolder) {
  AppListFolderItem* folder =
      new AppListFolderItem("folder1", AppListFolderItem::FOLDER_TYPE_OEM);
  model_.AddItem(folder);
  std::string folder_id = folder->id();

  // Should not be able to move to an OEM folder with MergeItems.
  AppListItem* item0 = new AppListItem("Item 0");
  model_.AddItem(item0);
  syncer::StringOrdinal item0_pos = item0->position();
  std::string new_folder = model_.MergeItems(folder_id, item0->id());
  EXPECT_EQ("", new_folder);
  EXPECT_EQ("", item0->folder_id());
  EXPECT_TRUE(item0->position().Equals(item0_pos));

  // Should not be able to move from an OEM folder with MoveItemToFolderAt.
  AppListItem* item1 = new AppListItem("Item 1");
  model_.AddItemToFolder(item1, folder_id);
  syncer::StringOrdinal item1_pos = item1->position();
  bool move_res = model_.MoveItemToFolderAt(item1, "", syncer::StringOrdinal());
  EXPECT_FALSE(move_res);
  EXPECT_TRUE(item1->position().Equals(item1_pos));
}

TEST_F(AppListModelFolderTest, DisableFolders) {
  // Set up a folder with two items and an OEM folder with one item.
  AppListFolderItem* folder =
      new AppListFolderItem("folder1", AppListFolderItem::FOLDER_TYPE_NORMAL);
  model_.AddItem(folder);
  std::string folder_id = folder->id();
  AppListItem* item0 = new AppListItem("Item 0");
  model_.AddItemToFolder(item0, folder_id);
  AppListItem* item1 = new AppListItem("Item 1");
  model_.AddItemToFolder(item1, folder_id);
  AppListFolderItem* folder_item = model_.FindFolderItem(folder_id);
  ASSERT_TRUE(folder_item);
  EXPECT_EQ(2u, folder_item->item_list()->item_count());
  AppListFolderItem* oem_folder =
      new AppListFolderItem("oem_folder", AppListFolderItem::FOLDER_TYPE_OEM);
  model_.AddItem(oem_folder);
  AppListItem* oem_item = new AppListItem("OEM Item");
  std::string oem_folder_id = oem_folder->id();
  model_.AddItemToFolder(oem_item, oem_folder_id);
  EXPECT_EQ("folder1,oem_folder", GetModelContents());

  // Disable folders. Ensure non-oem folder is removed.
  model_.SetFoldersEnabled(false);
  ASSERT_FALSE(model_.FindFolderItem(folder_id));
  ASSERT_TRUE(model_.FindFolderItem(oem_folder_id));
  EXPECT_EQ("Item 0,Item 1,oem_folder", GetModelContents());

  // Ensure folder creation fails.
  EXPECT_EQ(std::string(), model_.MergeItems(item0->id(), item1->id()));
}

}  // namespace app_list
