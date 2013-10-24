// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_model.h"

#include <map>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/base/models/list_model_observer.h"

namespace app_list {

namespace {

class TestObserver : public AppListModelObserver,
                     public ui::ListModelObserver {
 public:
  TestObserver()
      : status_changed_count_(0),
        users_changed_count_(0),
        signin_changed_count_(0),
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

  virtual void OnAppListModelUsersChanged() OVERRIDE {
    ++users_changed_count_;
  }

  virtual void OnAppListModelSigninStatusChanged() OVERRIDE {
    ++signin_changed_count_;
  }

  // ui::ListModelObserver
  virtual void ListItemsAdded(size_t start, size_t count) OVERRIDE {
    items_added_ += count;
  }

  virtual void ListItemsRemoved(size_t start, size_t count) OVERRIDE {
    items_removed_ += count;
  }

  virtual void ListItemMoved(size_t index, size_t target_index) OVERRIDE {
    items_moved_++;
  }

  virtual void ListItemsChanged(size_t index, size_t target_index) OVERRIDE {
  }

  int status_changed_count() const { return status_changed_count_; }
  int users_changed_count() const { return users_changed_count_; }
  int signin_changed_count() const { return signin_changed_count_; }
  size_t items_added() { return items_added_; }
  size_t items_removed() { return items_removed_; }
  size_t items_moved() { return items_moved_; }

 private:
  int status_changed_count_;
  int users_changed_count_;
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
    model_.apps()->AddObserver(&observer_);
  }
  virtual void TearDown() OVERRIDE {
    model_.RemoveObserver(&observer_);
    model_.apps()->RemoveObserver(&observer_);
  }

 protected:
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

TEST_F(AppListModelTest, SetUsers) {
  EXPECT_EQ(0u, model_.users().size());
  AppListModel::Users users;
  users.push_back(AppListModel::User());
  users[0].name = UTF8ToUTF16("test");
  model_.SetUsers(users);
  EXPECT_EQ(1, observer_.users_changed_count());
  ASSERT_EQ(1u, model_.users().size());
  EXPECT_EQ(UTF8ToUTF16("test"), model_.users()[0].name);
}

TEST_F(AppListModelTest, SetSignedIn) {
  EXPECT_TRUE(model_.signed_in());
  model_.SetSignedIn(false);
  EXPECT_EQ(1, observer_.signin_changed_count());
  EXPECT_FALSE(model_.signed_in());
  model_.SetSignedIn(true);
  EXPECT_EQ(2, observer_.signin_changed_count());
  EXPECT_TRUE(model_.signed_in());
  // Set the same signin state, no change is expected.
  model_.SetSignedIn(true);
  EXPECT_EQ(2, observer_.signin_changed_count());
}

TEST_F(AppListModelTest, AppsObserver) {
  const size_t num_apps = 2;
  model_.PopulateApps(num_apps);
  EXPECT_EQ(num_apps, observer_.items_added());
}

TEST_F(AppListModelTest, ModelGetItem) {
  const size_t num_apps = 2;
  model_.PopulateApps(num_apps);
  AppListItemModel* item0 = model_.apps()->GetItemAt(0);
  ASSERT_TRUE(item0);
  EXPECT_EQ(model_.GetItemName(0), item0->id());
  AppListItemModel* item1 = model_.apps()->GetItemAt(1);
  ASSERT_TRUE(item1);
  EXPECT_EQ(model_.GetItemName(1), item1->id());
}

TEST_F(AppListModelTest, ModelFindItem) {
  const size_t num_apps = 2;
  model_.PopulateApps(num_apps);
  std::string item_name0 = model_.GetItemName(0);
  AppListItemModel* item0 = model_.FindItem(item_name0);
  ASSERT_TRUE(item0);
  EXPECT_EQ(item_name0, item0->id());
  std::string item_name1 = model_.GetItemName(1);
  AppListItemModel* item1 = model_.FindItem(item_name1);
  ASSERT_TRUE(item1);
  EXPECT_EQ(item_name1, item1->id());
}

TEST_F(AppListModelTest, ModelMoveItem) {
  const size_t num_apps = 3;
  model_.PopulateApps(num_apps);
  // Adding another item will add it to the end.
  model_.CreateAndAddItem("Inserted Item");
  ASSERT_EQ(num_apps + 1, model_.apps()->item_count());
  // Move it to the position 1.
  model_.apps()->Move(num_apps, 1);
  AppListItemModel* item = model_.apps()->GetItemAt(1);
  ASSERT_TRUE(item);
  EXPECT_EQ("Inserted Item", item->id());
}

}  // namespace app_list
