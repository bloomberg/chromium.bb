// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_box_view.h"

#include <cctype>
#include <map>

#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_test.h"

namespace app_list {
namespace test {

class KeyPressCounterView : public views::View {
 public:
  KeyPressCounterView() : count_(0) {}
  virtual ~KeyPressCounterView() {}

  int GetCountAndReset() {
    int count = count_;
    count_ = 0;
    return count;
  }

 private:
  // Overridden from views::View:
  virtual bool OnKeyPressed(const ui::KeyEvent& key_event) OVERRIDE {
    if (!::isalnum(static_cast<int>(key_event.key_code()))) {
      ++count_;
      return true;
    }
    return false;
  }
  int count_;

  DISALLOW_COPY_AND_ASSIGN(KeyPressCounterView);
};

class SearchBoxViewTest : public views::test::WidgetTest,
                          public SearchBoxViewDelegate {
 public:
  SearchBoxViewTest() : query_changed_count_(0) {}
  virtual ~SearchBoxViewTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    views::test::WidgetTest::SetUp();
    widget_ = CreateTopLevelPlatformWidget();
    view_ = new SearchBoxView(this, &view_delegate_);
    counter_view_ = new KeyPressCounterView();
    widget_->GetContentsView()->AddChildView(view_);
    widget_->GetContentsView()->AddChildView(counter_view_);
    view_->set_contents_view(counter_view_);
  }

  virtual void TearDown() OVERRIDE {
    widget_->CloseNow();
    views::test::WidgetTest::TearDown();
  }

 protected:
  SearchBoxView* view() { return view_; }

  void SetLongAutoLaunchTimeout() {
    // Sets a long timeout that lasts longer than the test run.
    view_delegate_.set_auto_launch_timeout(base::TimeDelta::FromDays(1));
  }

  base::TimeDelta GetAutoLaunchTimeout() {
    return view_delegate_.GetAutoLaunchTimeout();
  }

  void ResetAutoLaunchTimeout() {
    view_delegate_.set_auto_launch_timeout(base::TimeDelta());
  }

  int GetContentsViewKeyPressCountAndReset() {
    return counter_view_->GetCountAndReset();
  }

  void KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE, true);
    view_->search_box()->OnKeyPressed(event);
    // Emulates the input method.
    if (::isalnum(static_cast<int>(key_code))) {
      base::char16 character = ::tolower(static_cast<int>(key_code));
      view_->search_box()->InsertText(base::string16(1, character));
    }
  }

  std::string GetLastQueryAndReset() {
    base::string16 query = last_query_;
    last_query_.clear();
    return base::UTF16ToUTF8(query);
  }

  int GetQueryChangedCountAndReset() {
    int result = query_changed_count_;
    query_changed_count_ = 0;
    return result;
  }

 private:
  // Overridden from SearchBoxViewDelegate:
  virtual void QueryChanged(SearchBoxView* sender) OVERRIDE {
    ++query_changed_count_;
    last_query_ = sender->search_box()->text();
  }

  AppListTestViewDelegate view_delegate_;
  views::Widget* widget_;
  SearchBoxView* view_;
  KeyPressCounterView* counter_view_;
  base::string16 last_query_;
  int query_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxViewTest);
};

TEST_F(SearchBoxViewTest, Basic) {
  KeyPress(ui::VKEY_A);
  EXPECT_EQ("a", GetLastQueryAndReset());
  EXPECT_EQ(1, GetQueryChangedCountAndReset());
  EXPECT_EQ(0, GetContentsViewKeyPressCountAndReset());

  KeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(0, GetQueryChangedCountAndReset());
  EXPECT_EQ(1, GetContentsViewKeyPressCountAndReset());

  view()->ClearSearch();
  EXPECT_EQ(1, GetQueryChangedCountAndReset());
  EXPECT_TRUE(GetLastQueryAndReset().empty());
}

TEST_F(SearchBoxViewTest, CancelAutoLaunch) {
  SetLongAutoLaunchTimeout();
  ASSERT_NE(base::TimeDelta(), GetAutoLaunchTimeout());

  // Normal key event cancels the timeout.
  KeyPress(ui::VKEY_A);
  EXPECT_EQ(base::TimeDelta(), GetAutoLaunchTimeout());
  ResetAutoLaunchTimeout();

  // Unusual key event doesn't cancel -- it will be canceled in
  // SearchResultListView.
  SetLongAutoLaunchTimeout();
  KeyPress(ui::VKEY_DOWN);
  EXPECT_NE(base::TimeDelta(), GetAutoLaunchTimeout());
  ResetAutoLaunchTimeout();

  // Clearing search box also cancels.
  SetLongAutoLaunchTimeout();
  view()->ClearSearch();
  EXPECT_EQ(base::TimeDelta(), GetAutoLaunchTimeout());
}

}  // namespace test
}  // namespace app_list
