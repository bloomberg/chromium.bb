// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/apps_search_box_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#include "ui/app_list/app_list_menu.h"
#import "ui/app_list/cocoa/current_user_menu_item_view.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

@interface TestAppsSearchBoxDelegate : NSObject<AppsSearchBoxDelegate> {
 @private
  app_list::SearchBoxModel searchBoxModel_;
  app_list::test::AppListTestViewDelegate appListDelegate_;
  int textChangeCount_;
}

@property(assign, nonatomic) int textChangeCount;

@end

@implementation TestAppsSearchBoxDelegate

@synthesize textChangeCount = textChangeCount_;

- (app_list::SearchBoxModel*)searchBoxModel {
  return &searchBoxModel_;
}

- (app_list::AppListViewDelegate*)appListDelegate {
  return &appListDelegate_;
}

- (BOOL)control:(NSControl*)control
               textView:(NSTextView*)textView
    doCommandBySelector:(SEL)command {
  return NO;
}

- (void)modelTextDidChange {
  ++textChangeCount_;
}

- (CGFloat)bubbleCornerRadius {
  return 3;
}

@end

namespace app_list {
namespace test {

class AppsSearchBoxControllerTest : public ui::CocoaTest {
 public:
  AppsSearchBoxControllerTest() {
    Init();
  }

  virtual void SetUp() OVERRIDE {
    apps_search_box_controller_.reset(
        [[AppsSearchBoxController alloc] initWithFrame:
            NSMakeRect(0, 0, 400, 100)]);
    delegate_.reset([[TestAppsSearchBoxDelegate alloc] init]);
    [apps_search_box_controller_ setDelegate:delegate_];

    ui::CocoaTest::SetUp();
    [[test_window() contentView] addSubview:[apps_search_box_controller_ view]];
  }

  virtual void TearDown() OVERRIDE {
    [apps_search_box_controller_ setDelegate:nil];
    ui::CocoaTest::TearDown();
  }

  void SimulateKeyAction(SEL c) {
    NSControl* control = [apps_search_box_controller_ searchTextField];
    [apps_search_box_controller_ control:control
                                textView:nil
                     doCommandBySelector:c];
  }

 protected:
  base::scoped_nsobject<TestAppsSearchBoxDelegate> delegate_;
  base::scoped_nsobject<AppsSearchBoxController> apps_search_box_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsSearchBoxControllerTest);
};

TEST_VIEW(AppsSearchBoxControllerTest, [apps_search_box_controller_ view]);

// Test the search box initialization, and search input and clearing.
TEST_F(AppsSearchBoxControllerTest, SearchBoxModel) {
  app_list::SearchBoxModel* model = [delegate_ searchBoxModel];
  // Usually localized "Search".
  const base::string16 hit_text(ASCIIToUTF16("hint"));
  model->SetHintText(hit_text);
  EXPECT_NSEQ(base::SysUTF16ToNSString(hit_text),
      [[[apps_search_box_controller_ searchTextField] cell] placeholderString]);

  const base::string16 search_text(ASCIIToUTF16("test"));
  model->SetText(search_text);
  EXPECT_NSEQ(base::SysUTF16ToNSString(search_text),
              [[apps_search_box_controller_ searchTextField] stringValue]);
  // Updates coming via the model should not notify the delegate.
  EXPECT_EQ(0, [delegate_ textChangeCount]);

  // Updates from the view should update the model and notify the delegate.
  [apps_search_box_controller_ clearSearch];
  EXPECT_EQ(base::string16(), model->text());
  EXPECT_NSEQ([NSString string],
              [[apps_search_box_controller_ searchTextField] stringValue]);
  EXPECT_EQ(1, [delegate_ textChangeCount]);

  // Test pressing escape clears the search.
  model->SetText(search_text);
  EXPECT_NSEQ(base::SysUTF16ToNSString(search_text),
              [[apps_search_box_controller_ searchTextField] stringValue]);
  SimulateKeyAction(@selector(complete:));
  EXPECT_NSEQ([NSString string],
              [[apps_search_box_controller_ searchTextField] stringValue]);
  EXPECT_EQ(2, [delegate_ textChangeCount]);
}

// Test the popup menu items.
TEST_F(AppsSearchBoxControllerTest, SearchBoxMenu) {
  NSPopUpButton* menu_control = [apps_search_box_controller_ menuControl];
  EXPECT_TRUE([apps_search_box_controller_ appListMenu]);
  ui::MenuModel* menu_model
      = [apps_search_box_controller_ appListMenu]->menu_model();
  // Add one to the item count to account for the blank, first item that Cocoa
  // has in its popup menus.
  EXPECT_EQ(menu_model->GetItemCount() + 1,
            [[menu_control menu] numberOfItems]);

  // The CURRENT_USER item should contain our custom view.
  ui::MenuModel* found_menu_model = menu_model;
  int index;
  EXPECT_TRUE(ui::MenuModel::GetModelAndIndexForCommandId(
      AppListMenu::CURRENT_USER, &menu_model, &index));
  EXPECT_EQ(found_menu_model, menu_model);
  NSMenuItem* current_user_item = [[menu_control menu] itemAtIndex:index + 1];
  EXPECT_TRUE([current_user_item view]);

  // A regular item should have just the label.
  EXPECT_TRUE(ui::MenuModel::GetModelAndIndexForCommandId(
      AppListMenu::SHOW_SETTINGS, &menu_model, &index));
  EXPECT_EQ(found_menu_model, menu_model);
  NSMenuItem* settings_item = [[menu_control menu] itemAtIndex:index + 1];
  EXPECT_FALSE([settings_item view]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(menu_model->GetLabelAt(index)),
              [settings_item title]);
}

// Test initialization and display of the custom menu item that shows the
// currently signed-in user. This is a non-interactive view.
class AppsSearchBoxCustomMenuItemTest : public ui::CocoaTest {
 public:
  AppsSearchBoxCustomMenuItemTest() {
    Init();
  }

  virtual void SetUp() OVERRIDE {
    scoped_ptr<AppListViewDelegate> delegate(new AppListTestViewDelegate);
    current_user_menu_item_.reset([[[CurrentUserMenuItemView alloc]
        initWithDelegate:delegate.get()] retain]);
    ui::CocoaTest::SetUp();
    [[test_window() contentView] addSubview:current_user_menu_item_];
  }

 protected:
  base::scoped_nsobject<NSView> current_user_menu_item_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsSearchBoxCustomMenuItemTest);
};

TEST_VIEW(AppsSearchBoxCustomMenuItemTest, current_user_menu_item_);

}  // namespace test
}  // namespace app_list
