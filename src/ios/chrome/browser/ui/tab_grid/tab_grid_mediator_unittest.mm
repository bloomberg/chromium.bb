// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"

#import <Foundation/Foundation.h>
#include <memory>

#include "base/mac/foundation_util.h"
#include "components/sessions/core/live_tab.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_helper.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#include "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/closing_web_state_observer.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_item.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#include "ios/web/common/features.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace sessions {
class TabRestoreServiceObserver;
class LiveTabContext;
}

namespace {

// A Fake restore service that just store and returns tabs.
class FakeTabRestoreService : public sessions::TabRestoreService {
 public:
  void AddObserver(sessions::TabRestoreServiceObserver* observer) override {
    NOTREACHED();
  }

  void RemoveObserver(sessions::TabRestoreServiceObserver* observer) override {
    NOTREACHED();
  }

  void CreateHistoricalTab(sessions::LiveTab* live_tab, int index) override {
    auto tab = std::make_unique<Tab>();
    int entry_count =
        live_tab->IsInitialBlankNavigation() ? 0 : live_tab->GetEntryCount();
    tab->navigations.resize(static_cast<int>(entry_count));
    for (int i = 0; i < entry_count; ++i) {
      sessions::SerializedNavigationEntry entry = live_tab->GetEntryAtIndex(i);
      tab->navigations[i] = entry;
    }
    entries_.push_front(std::move(tab));
  }

  void BrowserClosing(sessions::LiveTabContext* context) override {
    NOTREACHED();
  }

  void BrowserClosed(sessions::LiveTabContext* context) override {
    NOTREACHED();
  }

  void ClearEntries() override { NOTREACHED(); }

  void DeleteNavigationEntries(const DeletionPredicate& predicate) override {
    NOTREACHED();
  }

  const Entries& entries() const override { return entries_; }

  std::vector<sessions::LiveTab*> RestoreMostRecentEntry(
      sessions::LiveTabContext* context) override {
    NOTREACHED();
    return std::vector<sessions::LiveTab*>();
  }

  std::unique_ptr<Tab> RemoveTabEntryById(SessionID session_id) override {
    Entries::iterator it = GetEntryIteratorById(session_id);
    if (it == entries_.end()) {
      return nullptr;
    }
    auto tab = std::unique_ptr<Tab>(static_cast<Tab*>(it->release()));
    entries_.erase(it);
    return tab;
  }

  std::vector<sessions::LiveTab*> RestoreEntryById(
      sessions::LiveTabContext* context,
      SessionID session_id,
      WindowOpenDisposition disposition) override {
    NOTREACHED();
    return std::vector<sessions::LiveTab*>();
  }

  void LoadTabsFromLastSession() override { NOTREACHED(); }

  bool IsLoaded() const override {
    NOTREACHED();
    return false;
  }

  void DeleteLastSession() override { NOTREACHED(); }

  bool IsRestoring() const override {
    NOTREACHED();
    return false;
  }

 private:
  // Returns an iterator to the entry with id |session_id|.
  Entries::iterator GetEntryIteratorById(SessionID session_id) {
    for (auto i = entries_.begin(); i != entries_.end(); ++i) {
      if ((*i)->id == session_id) {
        return i;
      }
    }
    return entries_.end();
  }
  Entries entries_;
};
}  // namespace

// Test object that conforms to GridConsumer and exposes inner state for test
// verification.
@interface FakeConsumer : NSObject<GridConsumer>
// The fake consumer only keeps the identifiers of items for simplicity
@property(nonatomic, strong) NSMutableArray<NSString*>* items;
@property(nonatomic, assign) NSString* selectedItemID;
@end
@implementation FakeConsumer
@synthesize items = _items;
@synthesize selectedItemID = _selectedItemID;

- (void)populateItems:(NSArray<GridItem*>*)items
       selectedItemID:(NSString*)selectedItemID {
  self.selectedItemID = selectedItemID;
  self.items = [NSMutableArray array];
  for (GridItem* item in items) {
    [self.items addObject:item.identifier];
  }
}

- (void)insertItem:(GridItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(NSString*)selectedItemID {
  [self.items insertObject:item.identifier atIndex:index];
  self.selectedItemID = selectedItemID;
}

- (void)removeItemWithID:(NSString*)removedItemID
          selectedItemID:(NSString*)selectedItemID {
  [self.items removeObject:removedItemID];
  self.selectedItemID = selectedItemID;
}

- (void)selectItemWithID:(NSString*)selectedItemID {
  self.selectedItemID = selectedItemID;
}

- (void)replaceItemID:(NSString*)itemID withItem:(GridItem*)item {
  NSUInteger index = [self.items indexOfObject:itemID];
  self.items[index] = item.identifier;
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)toIndex {
  [self.items removeObject:itemID];
  [self.items insertObject:itemID atIndex:toIndex];
}

@end

// Fake WebStateList delegate that attaches the required tab helper.
class TabHelperFakeWebStateListDelegate : public FakeWebStateListDelegate {
 public:
  TabHelperFakeWebStateListDelegate() {}
  ~TabHelperFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    TabIdTabHelper::CreateForWebState(web_state);
    // Create NTPTabHelper to ensure VisibleURL is set to kChromeUINewTabURL.
    id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
    NewTabPageTabHelper::CreateForWebState(web_state, delegate);
    PagePlaceholderTabHelper::CreateForWebState(web_state);
  }
};

class TabGridMediatorTest : public PlatformTest {
 public:
  TabGridMediatorTest() {}
  ~TabGridMediatorTest() override {}

  void SetUp() override {
    PlatformTest::SetUp();
    browser_state_ = TestChromeBrowserState::Builder().Build();
    web_state_list_delegate_ =
        std::make_unique<TabHelperFakeWebStateListDelegate>();
    web_state_list_ =
        std::make_unique<WebStateList>(web_state_list_delegate_.get());
    tab_restore_service_ = std::make_unique<FakeTabRestoreService>();
    closing_web_state_observer_ = [[ClosingWebStateObserver alloc]
        initWithRestoreService:tab_restore_service_.get()];
    closing_web_state_observer_bridge_ =
        std::make_unique<WebStateListObserverBridge>(
            closing_web_state_observer_);
    web_state_list_->AddObserver(closing_web_state_observer_bridge_.get());
    NSMutableSet<NSString*>* identifiers = [[NSMutableSet alloc] init];
    browser_ = std::make_unique<TestBrowser>(browser_state_.get(),
                                             web_state_list_.get());
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());

    // Insert some web states.
    for (int i = 0; i < 3; i++) {
      auto web_state = CreateTestWebStateWithURL(GURL("https://foo/bar"));
      NSString* identifier =
          TabIdTabHelper::FromWebState(web_state.get())->tab_id();
      // Tab IDs should be unique.
      ASSERT_FALSE([identifiers containsObject:identifier]);
      [identifiers addObject:identifier];
      web_state_list_->InsertWebState(i, std::move(web_state),
                                      WebStateList::INSERT_FORCE_INDEX,
                                      WebStateOpener());
    }
    original_identifiers_ = [identifiers copy];
    web_state_list_->ActivateWebStateAt(1);
    original_selected_identifier_ =
        TabIdTabHelper::FromWebState(web_state_list_->GetWebStateAt(1))
            ->tab_id();
    consumer_ = [[FakeConsumer alloc] init];
    mediator_ = [[TabGridMediator alloc] initWithConsumer:consumer_];
    mediator_.browser = browser_.get();
    mediator_.tabRestoreService = tab_restore_service_.get();
  }

  // Creates a TestWebState with a navigation history containing exactly only
  // the given |url|.
  std::unique_ptr<web::TestWebState> CreateTestWebStateWithURL(
      const GURL& url) {
    auto web_state = std::make_unique<web::TestWebState>();
    auto navigation_manager = std::make_unique<web::TestNavigationManager>();
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(browser_state_.get());
    TabIdTabHelper::CreateForWebState(web_state.get());
    SnapshotTabHelper::CreateForWebState(web_state.get(),
                                         [[NSUUID UUID] UUIDString]);
    return web_state;
  }

  void TearDown() override {
    web_state_list_->RemoveObserver(closing_web_state_observer_bridge_.get());
    PlatformTest::TearDown();
  }

  // Prepare the mock method to restore the tabs.
  void PrepareForRestoration() {
      TestSessionService* test_session_service =
          [[TestSessionService alloc] init];
      SessionRestorationBrowserAgent::CreateForBrowser(browser_.get(),
                                                       test_session_service);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  std::unique_ptr<TabHelperFakeWebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  std::unique_ptr<FakeTabRestoreService> tab_restore_service_;
  id tab_model_;
  FakeConsumer* consumer_;
  TabGridMediator* mediator_;
  NSSet<NSString*>* original_identifiers_;
  NSString* original_selected_identifier_;
  std::unique_ptr<Browser> browser_;
  ClosingWebStateObserver* closing_web_state_observer_;
  std::unique_ptr<WebStateListObserverBridge>
      closing_web_state_observer_bridge_;
};

#pragma mark - Consumer tests

// Tests that the consumer is populated after the tab model is set on the
// mediator.
TEST_F(TabGridMediatorTest, ConsumerPopulateItems) {
  EXPECT_EQ(3UL, consumer_.items.count);
  EXPECT_NSEQ(original_selected_identifier_, consumer_.selectedItemID);
}

// Tests that the consumer is notified when a web state is inserted.
TEST_F(TabGridMediatorTest, ConsumerInsertItem) {
  ASSERT_EQ(3UL, consumer_.items.count);
  auto web_state = std::make_unique<web::TestWebState>();
  TabIdTabHelper::CreateForWebState(web_state.get());
  NSString* item_identifier =
      TabIdTabHelper::FromWebState(web_state.get())->tab_id();
  web_state_list_->InsertWebState(1, std::move(web_state),
                                  WebStateList::INSERT_FORCE_INDEX,
                                  WebStateOpener());
  EXPECT_EQ(4UL, consumer_.items.count);
  // The same ID should be selected after the insertion, since the new web state
  // wasn't selected.
  EXPECT_NSEQ(original_selected_identifier_, consumer_.selectedItemID);
  EXPECT_NSEQ(item_identifier, consumer_.items[1]);
  EXPECT_FALSE([original_identifiers_ containsObject:item_identifier]);
}

// Tests that the consumer is notified when a web state is removed.
// The selected web state at index 1 is removed. The web state originally
// at index 2 should be the new selected item.
TEST_F(TabGridMediatorTest, ConsumerRemoveItem) {
  web_state_list_->CloseWebStateAt(1, WebStateList::CLOSE_NO_FLAGS);
  EXPECT_EQ(2UL, consumer_.items.count);
  // Expect that a different web state is selected now.
  EXPECT_NSNE(original_selected_identifier_, consumer_.selectedItemID);
}

// Tests that the consumer is notified when the active web state is changed.
TEST_F(TabGridMediatorTest, ConsumerUpdateSelectedItem) {
  EXPECT_NSEQ(original_selected_identifier_, consumer_.selectedItemID);
  web_state_list_->ActivateWebStateAt(2);
  EXPECT_NSEQ(
      TabIdTabHelper::FromWebState(web_state_list_->GetWebStateAt(2))->tab_id(),
      consumer_.selectedItemID);
}

// Tests that the consumer is notified when a web state is replaced.
// The selected item is replaced, so the new selected item id should be the
// id of the new item.
TEST_F(TabGridMediatorTest, ConsumerReplaceItem) {
  auto new_web_state = std::make_unique<web::TestWebState>();
  TabIdTabHelper::CreateForWebState(new_web_state.get());
  NSString* new_item_identifier =
      TabIdTabHelper::FromWebState(new_web_state.get())->tab_id();
  web_state_list_->ReplaceWebStateAt(1, std::move(new_web_state));
  EXPECT_EQ(3UL, consumer_.items.count);
  EXPECT_NSEQ(new_item_identifier, consumer_.selectedItemID);
  EXPECT_NSEQ(new_item_identifier, consumer_.items[1]);
  EXPECT_FALSE([original_identifiers_ containsObject:new_item_identifier]);
}

// Tests that the consumer is notified when a web state is moved.
TEST_F(TabGridMediatorTest, ConsumerMoveItem) {
  NSString* item1 = consumer_.items[1];
  NSString* item2 = consumer_.items[2];
  web_state_list_->MoveWebStateAt(1, 2);
  EXPECT_NSEQ(item1, consumer_.items[2]);
  EXPECT_NSEQ(item2, consumer_.items[1]);
}

#pragma mark - Command tests

// Tests that the active index is updated when |-selectItemWithID:| is called.
// Tests that the consumer's selected index is updated.
TEST_F(TabGridMediatorTest, SelectItemCommand) {
  // Previous selected index is 1.
  NSString* identifier =
      TabIdTabHelper::FromWebState(web_state_list_->GetWebStateAt(2))->tab_id();
  [mediator_ selectItemWithID:identifier];
  EXPECT_EQ(2, web_state_list_->active_index());
  EXPECT_NSEQ(identifier, consumer_.selectedItemID);
}

// Tests that the |web_state_list_| count is decremented when
// |-closeItemWithID:| is called.
// Tests that the consumer's item count is also decremented.
TEST_F(TabGridMediatorTest, CloseItemCommand) {
  // Previously there were 3 items.
  NSString* identifier =
      TabIdTabHelper::FromWebState(web_state_list_->GetWebStateAt(0))->tab_id();
  [mediator_ closeItemWithID:identifier];
  EXPECT_EQ(2, web_state_list_->count());
  EXPECT_EQ(2UL, consumer_.items.count);
}

// Tests that the |web_state_list_| and consumer's list are empty when
// |-closeAllItems| is called. Tests that |-undoCloseAllItems| does not restore
// the |web_state_list_|.
TEST_F(TabGridMediatorTest, CloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ closeAllItems];
  EXPECT_EQ(0, web_state_list_->count());
  EXPECT_EQ(0UL, consumer_.items.count);
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(0, web_state_list_->count());
}

// Tests that the |web_state_list_| and consumer's list are empty when
// |-saveAndCloseAllItems| is called.
TEST_F(TabGridMediatorTest, SaveAndCloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  EXPECT_EQ(0, web_state_list_->count());
  EXPECT_EQ(0UL, consumer_.items.count);
}

// Tests that the |web_state_list_| is not restored to 3 items when
// |-undoCloseAllItems| is called after |-discardSavedClosedItems| is called.
TEST_F(TabGridMediatorTest, DiscardSavedClosedItemsCommand) {
  PrepareForRestoration();
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  [mediator_ discardSavedClosedItems];
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(0, web_state_list_->count());
  EXPECT_EQ(0UL, consumer_.items.count);
}

// Tests that the |web_state_list_| is restored to 3 items when
// |-undoCloseAllItems| is called.
TEST_F(TabGridMediatorTest, UndoCloseAllItemsCommand) {
  PrepareForRestoration();
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(3, web_state_list_->count());
  EXPECT_EQ(3UL, consumer_.items.count);
  EXPECT_TRUE([original_identifiers_ containsObject:consumer_.items[0]]);
  EXPECT_TRUE([original_identifiers_ containsObject:consumer_.items[1]]);
  EXPECT_TRUE([original_identifiers_ containsObject:consumer_.items[2]]);
}

// Tests that the |web_state_list_| is restored to 3 items when
// |-undoCloseAllItems| is called.
TEST_F(TabGridMediatorTest, UndoCloseAllItemsCommandWithNTP) {
  PrepareForRestoration();
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  // The three tabs created in the SetUp should be passed to the restore
  // service.
  EXPECT_EQ(3UL, tab_restore_service_->entries().size());
  std::set<SessionID::id_type> ids;
  for (auto& entry : tab_restore_service_->entries()) {
    ids.insert(entry->id.id());
  }
  EXPECT_EQ(3UL, ids.size());
  // There should be no tabs in the WebStateList.
  EXPECT_EQ(0, web_state_list_->count());
  EXPECT_EQ(0UL, consumer_.items.count);

  // Add three new tabs.
  auto web_state1 = CreateTestWebStateWithURL(GURL("https://test/url1"));
  web_state_list_->InsertWebState(0, std::move(web_state1),
                                  WebStateList::INSERT_FORCE_INDEX,
                                  WebStateOpener());
  // Second tab is a NTP.
  auto web_state2 = CreateTestWebStateWithURL(GURL(kChromeUINewTabURL));
  web_state_list_->InsertWebState(1, std::move(web_state2),
                                  WebStateList::INSERT_FORCE_INDEX,
                                  WebStateOpener());
  auto web_state3 = CreateTestWebStateWithURL(GURL("https://test/url2"));
  web_state_list_->InsertWebState(2, std::move(web_state3),
                                  WebStateList::INSERT_FORCE_INDEX,
                                  WebStateOpener());
  web_state_list_->ActivateWebStateAt(0);

  [mediator_ saveAndCloseAllItems];
  // The NTP should not be saved.
  EXPECT_EQ(5UL, tab_restore_service_->entries().size());
  EXPECT_EQ(0, web_state_list_->count());
  EXPECT_EQ(0UL, consumer_.items.count);
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(3UL, tab_restore_service_->entries().size());
  EXPECT_EQ(3UL, consumer_.items.count);
  // Check the session entries were not changed.
  for (auto& entry : tab_restore_service_->entries()) {
    EXPECT_EQ(1UL, ids.count(entry->id.id()));
  }
}

// Tests that when |-addNewItem| is called, the |web_state_list_| count is
// incremented, the |active_index| is at the end of |web_state_list_|, the new
// web state has no opener, and the URL is the New Tab Page.
// Tests that the consumer has added an item with the correct identifier.
TEST_F(TabGridMediatorTest, AddNewItemAtEndCommand) {
  // Previously there were 3 items and the selected index was 1.
  [mediator_ addNewItem];
  EXPECT_EQ(4, web_state_list_->count());
  EXPECT_EQ(3, web_state_list_->active_index());
  web::WebState* web_state = web_state_list_->GetWebStateAt(3);
  ASSERT_TRUE(web_state);
  EXPECT_EQ(web_state->GetBrowserState(), browser_state_.get());
  EXPECT_FALSE(web_state->HasOpener());
  // The URL of pending item (i.e. kChromeUINewTabURL) will not be returned
  // here because WebState doesn't load the URL until it's visible and
  // NavigationManager::GetVisibleURL requires WebState::IsLoading to be true
  // to return pending item's URL.
  EXPECT_EQ("", web_state->GetVisibleURL().spec());
  NSString* identifier = TabIdTabHelper::FromWebState(web_state)->tab_id();
  EXPECT_FALSE([original_identifiers_ containsObject:identifier]);
  // Consumer checks.
  EXPECT_EQ(4UL, consumer_.items.count);
  EXPECT_NSEQ(identifier, consumer_.selectedItemID);
  EXPECT_NSEQ(identifier, consumer_.items[3]);
}

// Tests that when |-insertNewItemAtIndex:| is called, the |web_state_list_|
// count is incremented, the |active_index| is the newly added index, the new
// web state has no opener, and the URL is the new tab page.
// Checks that the consumer has added an item with the correct identifier.
TEST_F(TabGridMediatorTest, InsertNewItemCommand) {
  // Previously there were 3 items and the selected index was 1.
  [mediator_ insertNewItemAtIndex:0];
  EXPECT_EQ(4, web_state_list_->count());
  EXPECT_EQ(0, web_state_list_->active_index());
  web::WebState* web_state = web_state_list_->GetWebStateAt(0);
  ASSERT_TRUE(web_state);
  EXPECT_EQ(web_state->GetBrowserState(), browser_state_.get());
  EXPECT_FALSE(web_state->HasOpener());
  // The URL of pending item (i.e. kChromeUINewTabURL) will not be returned
  // here because WebState doesn't load the URL until it's visible and
  // NavigationManager::GetVisibleURL requires WebState::IsLoading to be true
  // to return pending item's URL.
  EXPECT_EQ("", web_state->GetVisibleURL().spec());
  NSString* identifier = TabIdTabHelper::FromWebState(web_state)->tab_id();
  EXPECT_FALSE([original_identifiers_ containsObject:identifier]);
  // Consumer checks.
  EXPECT_EQ(4UL, consumer_.items.count);
  EXPECT_NSEQ(identifier, consumer_.selectedItemID);
  EXPECT_NSEQ(identifier, consumer_.items[0]);
}

// Tests that |-insertNewItemAtIndex:| is a no-op if the mediator's browser
// is bullptr.
TEST_F(TabGridMediatorTest, InsertNewItemWithNoBrowserCommand) {
  mediator_.browser = nullptr;
  ASSERT_EQ(3, web_state_list_->count());
  ASSERT_EQ(1, web_state_list_->active_index());
  [mediator_ insertNewItemAtIndex:0];
  EXPECT_EQ(3, web_state_list_->count());
  EXPECT_EQ(1, web_state_list_->active_index());
}

// Tests that when |-moveItemFromIndex:toIndex:| is called, there is no change
// in the item count in |web_state_list_|, but that the constituent web states
// have been reordered.
TEST_F(TabGridMediatorTest, MoveItemCommand) {
  // Capture ordered original IDs.
  NSMutableArray<NSString*>* pre_move_ids = [[NSMutableArray alloc] init];
  for (int i = 0; i < 3; i++) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(i);
    [pre_move_ids addObject:TabIdTabHelper::FromWebState(web_state)->tab_id()];
  }
  NSString* pre_move_selected_id =
      pre_move_ids[web_state_list_->active_index()];
  // Items start ordered [A, B, C].
  [mediator_ moveItemWithID:consumer_.items[0] toIndex:2];
  // Items should now be ordered [B, C, A] -- the pre-move identifiers should
  // still be in this order.
  // Item count hasn't changed.
  EXPECT_EQ(3, web_state_list_->count());
  // Active index has moved -- it was 1, now 0.
  EXPECT_EQ(0, web_state_list_->active_index());
  // Identifier at 0, 1, 2 should match the original_identifier_ at 1, 2, 0.
  for (int index = 0; index < 2; index++) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    ASSERT_TRUE(web_state);
    NSString* identifier = TabIdTabHelper::FromWebState(web_state)->tab_id();
    EXPECT_NSEQ(identifier, pre_move_ids[(index + 1) % 3]);
    EXPECT_NSEQ(identifier, consumer_.items[index]);
  }
  EXPECT_EQ(pre_move_selected_id, consumer_.selectedItemID);
}
