// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/apps_search_box_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#include "ui/app_list/app_list_menu.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#import "ui/base/cocoa/menu_controller.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

@interface TestAppsSearchBoxDelegate : NSObject<AppsSearchBoxDelegate> {
 @private
  app_list::SearchBoxModel searchBoxModel_;
  app_list::test::AppListTestViewDelegate appListDelegate_;
  app_list::test::AppListTestModel appListModel_;
  int textChangeCount_;
}

@property(assign, nonatomic) int textChangeCount;

@end

@implementation TestAppsSearchBoxDelegate

@synthesize textChangeCount = textChangeCount_;

- (id)init {
  if ((self = [super init])) {
    app_list::AppListModel::Users users(2);
    users[0].name = ASCIIToUTF16("user1");
    users[1].name = ASCIIToUTF16("user2");
    users[1].email = ASCIIToUTF16("user2@chromium.org");
    users[1].active = true;
    appListModel_.SetUsers(users);
  }
  return self;
}

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

- (app_list::AppListModel*)appListModel {
  return &appListModel_;
}

@end

namespace app_list {
namespace test {

class AppsSearchBoxControllerTest : public ui::CocoaTest,
                                    public AppListModelObserver {
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
    [delegate_ appListModel]->AddObserver(this);

    ui::CocoaTest::SetUp();
    [[test_window() contentView] addSubview:[apps_search_box_controller_ view]];
  }

  virtual void TearDown() OVERRIDE {
    [delegate_ appListModel]->RemoveObserver(this);
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
  // Overridden from app_list::AppListModelObserver:
  virtual void OnAppListModelUsersChanged() OVERRIDE {
    [apps_search_box_controller_ rebuildMenu];
  }

  virtual void OnAppListModelSigninStatusChanged() OVERRIDE {}

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
  // Updates coming via the model should notify the delegate.
  EXPECT_EQ(1, [delegate_ textChangeCount]);

  // Updates from the view should update the model and notify the delegate.
  [apps_search_box_controller_ clearSearch];
  EXPECT_EQ(base::string16(), model->text());
  EXPECT_NSEQ([NSString string],
              [[apps_search_box_controller_ searchTextField] stringValue]);
  EXPECT_EQ(2, [delegate_ textChangeCount]);

  // Test pressing escape clears the search. First add some text.
  model->SetText(search_text);
  EXPECT_EQ(3, [delegate_ textChangeCount]);

  EXPECT_NSEQ(base::SysUTF16ToNSString(search_text),
              [[apps_search_box_controller_ searchTextField] stringValue]);
  SimulateKeyAction(@selector(complete:));
  EXPECT_NSEQ([NSString string],
              [[apps_search_box_controller_ searchTextField] stringValue]);
  EXPECT_EQ(4, [delegate_ textChangeCount]);
}

// Test the popup menu items when there is only one user..
TEST_F(AppsSearchBoxControllerTest, SearchBoxMenuSingleUser) {
  // Set a single user. We need to set the delegate again because the
  // AppListModel observer isn't hooked up in these tests.
  [delegate_ appListModel]->SetUsers(app_list::AppListModel::Users(1));
  [apps_search_box_controller_ setDelegate:delegate_];

  NSPopUpButton* menu_control = [apps_search_box_controller_ menuControl];
  EXPECT_TRUE([apps_search_box_controller_ appListMenu]);
  ui::MenuModel* menu_model
      = [apps_search_box_controller_ appListMenu]->menu_model();
  // Add one to the item count to account for the blank, first item that Cocoa
  // has in its popup menus.
  EXPECT_EQ(menu_model->GetItemCount() + 1,
            [[menu_control menu] numberOfItems]);

  // All command ids should be less than |SELECT_PROFILE| as no user menu items
  // are being shown.
  for (int i = 0; i < menu_model->GetItemCount(); ++i)
    EXPECT_LT(menu_model->GetCommandIdAt(i), AppListMenu::SELECT_PROFILE);

  // The number of items should match the index that starts profile items.
  EXPECT_EQ(AppListMenu::SELECT_PROFILE, menu_model->GetItemCount());
}

// Test the popup menu items for the multi-profile case.
TEST_F(AppsSearchBoxControllerTest, SearchBoxMenu) {
  const app_list::AppListModel::Users& users =
      [delegate_ appListModel]->users();
  NSPopUpButton* menu_control = [apps_search_box_controller_ menuControl];
  EXPECT_TRUE([apps_search_box_controller_ appListMenu]);
  ui::MenuModel* menu_model
      = [apps_search_box_controller_ appListMenu]->menu_model();
  // Add one to the item count to account for the blank, first item that Cocoa
  // has in its popup menus.
  EXPECT_EQ(menu_model->GetItemCount() + 1,
            [[menu_control menu] numberOfItems]);

  ui::MenuModel* found_menu_model = menu_model;
  int index;
  MenuController* controller = [[menu_control menu] delegate];

  // The first user item is an unchecked label.
  EXPECT_TRUE(ui::MenuModel::GetModelAndIndexForCommandId(
      AppListMenu::SELECT_PROFILE, &menu_model, &index));
  EXPECT_EQ(found_menu_model, menu_model);
  NSMenuItem* unchecked_user_item = [[menu_control menu] itemAtIndex:index + 1];
  [controller validateUserInterfaceItem:unchecked_user_item];
  // The profile name should be shown if there is no email available.
  EXPECT_NSEQ(base::SysUTF16ToNSString(users[0].name),
              [unchecked_user_item title]);
  EXPECT_EQ(NSOffState, [unchecked_user_item state]);

  // The second user item is a checked label because it is the active profile.
  EXPECT_TRUE(ui::MenuModel::GetModelAndIndexForCommandId(
      AppListMenu::SELECT_PROFILE + 1, &menu_model, &index));
  EXPECT_EQ(found_menu_model, menu_model);
  NSMenuItem* checked_user_item = [[menu_control menu] itemAtIndex:index + 1];
  [controller validateUserInterfaceItem:checked_user_item];
  // The email is shown when available.
  EXPECT_NSEQ(base::SysUTF16ToNSString(users[1].email),
              [checked_user_item title]);
  EXPECT_EQ(NSOnState, [checked_user_item state]);

  // A regular item should have just the label.
  EXPECT_TRUE(ui::MenuModel::GetModelAndIndexForCommandId(
      AppListMenu::SHOW_SETTINGS, &menu_model, &index));
  EXPECT_EQ(found_menu_model, menu_model);
  NSMenuItem* settings_item = [[menu_control menu] itemAtIndex:index + 1];
  EXPECT_FALSE([settings_item view]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(menu_model->GetLabelAt(index)),
              [settings_item title]);
}

// Test adding another user, and changing an existing one.
TEST_F(AppsSearchBoxControllerTest, SearchBoxMenuChangingUsers) {
  app_list::AppListModel::Users users = [delegate_ appListModel]->users();
  EXPECT_EQ(2u, users.size());
  ui::MenuModel* menu_model
      = [apps_search_box_controller_ appListMenu]->menu_model();
  // Adding one to account for the empty item at index 0 in Cocoa popup menus.
  int non_user_items = menu_model->GetItemCount() - users.size() + 1;

  NSPopUpButton* menu_control = [apps_search_box_controller_ menuControl];
  EXPECT_EQ(2, [[menu_control menu] numberOfItems] - non_user_items);
  EXPECT_NSEQ(base::SysUTF16ToNSString(users[0].name),
              [[[menu_control menu] itemAtIndex:1] title]);

  users[0].name = ASCIIToUTF16("renamed user");
  app_list::AppListModel::User new_user;
  new_user.name = ASCIIToUTF16("user3");
  users.push_back(new_user);
  [delegate_ appListModel]->SetUsers(users);

  // Should now be an extra item, and it should have correct titles.
  EXPECT_EQ(3, [[menu_control menu] numberOfItems] - non_user_items);
  EXPECT_NSEQ(base::SysUTF16ToNSString(users[0].name),
              [[[menu_control menu] itemAtIndex:1] title]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(new_user.name),
              [[[menu_control menu] itemAtIndex:3] title]);
}

}  // namespace test
}  // namespace app_list
