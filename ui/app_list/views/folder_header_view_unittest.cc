// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/folder_header_view.h"

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/views/folder_header_view_delegate.h"
#include "ui/views/test/views_test_base.h"

namespace app_list {
namespace test {

namespace {

class TestFolderHeaderViewDelegate : public FolderHeaderViewDelegate {
 public:
  TestFolderHeaderViewDelegate() {}
  virtual ~TestFolderHeaderViewDelegate() {}

  // FolderHeaderViewDelegate
  virtual void NavigateBack(AppListFolderItem* item,
                            const ui::Event& event_flags) OVERRIDE {}

  virtual void GiveBackFocusToSearchBox() OVERRIDE {}

  virtual void SetItemName(AppListFolderItem* item,
                           const std::string& name) OVERRIDE {
    folder_name_ = name;
  }

  const std::string& folder_name() const { return folder_name_; }

 private:
  std::string folder_name_;

  DISALLOW_COPY_AND_ASSIGN(TestFolderHeaderViewDelegate);
};

}  // namespace

class FolderHeaderViewTest : public views::ViewsTestBase {
 public:
  FolderHeaderViewTest() {}
  virtual ~FolderHeaderViewTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    views::ViewsTestBase::SetUp();
    model_.reset(new AppListTestModel);
    model_->SetFoldersEnabled(true);

    delegate_.reset(new TestFolderHeaderViewDelegate);
    folder_header_view_.reset(new FolderHeaderView(delegate_.get()));
  }

  virtual void TearDown() OVERRIDE {
    folder_header_view_.reset();  // Release apps grid view before models.
    delegate_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  void UpdateFolderName(const std::string& name) {
    base::string16 folder_name = base::UTF8ToUTF16(name);
    folder_header_view_->SetFolderNameForTest(folder_name);
    folder_header_view_->ContentsChanged(NULL, folder_name);
  }

  const std::string GetFolderNameFromUI() {
    return base::UTF16ToUTF8(folder_header_view_->GetFolderNameForTest());
  }

  bool CanEditFolderName() {
    return folder_header_view_->IsFolderNameEnabledForTest();
  }

  scoped_ptr<AppListTestModel> model_;
  scoped_ptr<FolderHeaderView> folder_header_view_;
  scoped_ptr<TestFolderHeaderViewDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FolderHeaderViewTest);
};

TEST_F(FolderHeaderViewTest, SetFolderName) {
  // Creating a folder with empty folder name.
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  folder_header_view_->SetFolderItem(folder_item);
  EXPECT_EQ("", GetFolderNameFromUI());
  EXPECT_TRUE(CanEditFolderName());

  // Update UI to set folder name to "test folder".
  UpdateFolderName("test folder");
  EXPECT_EQ("test folder", delegate_->folder_name());
}

TEST_F(FolderHeaderViewTest, MaxFoldernNameLength) {
  // Creating a folder with empty folder name.
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  folder_header_view_->SetFolderItem(folder_item);
  EXPECT_EQ("", GetFolderNameFromUI());
  EXPECT_TRUE(CanEditFolderName());

  // Update UI to set folder name to really long one beyond its maxium limit,
  // The folder name should be trucated to the maximum length.
  std::string max_len_name;
  for (size_t i = 0; i < kMaxFolderNameChars; ++i)
    max_len_name += "a";
  UpdateFolderName(max_len_name);
  EXPECT_EQ(max_len_name, delegate_->folder_name());
  std::string too_long_name = max_len_name + "a";
  UpdateFolderName(too_long_name);
  EXPECT_EQ(max_len_name, delegate_->folder_name());
}

TEST_F(FolderHeaderViewTest, OemFolderNameNotEditable) {
  AppListFolderItem* folder_item = model_->CreateAndAddOemFolder("oem folder");
  folder_header_view_->SetFolderItem(folder_item);
  EXPECT_EQ("", GetFolderNameFromUI());
  EXPECT_FALSE(CanEditFolderName());
}

}  // namespace test
}  // namespace app_list
