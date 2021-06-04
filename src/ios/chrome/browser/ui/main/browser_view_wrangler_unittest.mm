// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/main/test_browser_list_observer.h"
#import "ios/chrome/browser/sessions/scene_util_test_support.h"
#include "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/testing/scoped_block_swizzler.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SceneStateWithFakeScene : SceneState

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithAppState:(AppState*)appState NS_UNAVAILABLE;

@end

@implementation SceneStateWithFakeScene

- (instancetype)init {
  if ((self = [super initWithAppState:nil])) {
    if (@available(ios 13, *)) {
      [self setScene:FakeSceneWithIdentifier([[NSUUID UUID] UUIDString])];
    }
  }
  return self;
}

@end

namespace {

class BrowserViewWranglerTest : public PlatformTest {
 protected:
  BrowserViewWranglerTest()
      : scene_state_([[SceneStateWithFakeScene alloc] init]),
        test_session_service_([[TestSessionService alloc] init]) {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        SendTabToSelfSyncServiceFactory::GetDefaultFactory());

    chrome_browser_state_ = test_cbs_builder.Build();

    session_service_block_ = ^SessionServiceIOS*(id self) {
      return test_session_service_;
    };
    session_service_swizzler_.reset(new ScopedBlockSwizzler(
        [SessionServiceIOS class], @selector(sharedService),
        session_service_block_));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  SceneState* scene_state_;
  TestSessionService* test_session_service_;
  id session_service_block_;
  std::unique_ptr<ScopedBlockSwizzler> session_service_swizzler_;
};

TEST_F(BrowserViewWranglerTest, TestInitNilObserver) {
  // |task_environment_| must outlive all objects created by BVC, because those
  // objects may rely on threading API in dealloc.
  @autoreleasepool {
    BrowserViewWrangler* wrangler = [[BrowserViewWrangler alloc]
               initWithBrowserState:chrome_browser_state_.get()
                         sceneState:scene_state_
         applicationCommandEndpoint:(id<ApplicationCommands>)nil
        browsingDataCommandEndpoint:nil];
    [wrangler createMainBrowser];
    [wrangler createMainCoordinatorAndInterface];
    // Test that BVC is created on demand.
    UIViewController* bvc = wrangler.mainInterface.viewController;
    EXPECT_NE(bvc, nil);

    // Test that scene_state_ is associated with the browser.
    SceneState* main_browser_scene_state =
        SceneStateBrowserAgent::FromBrowser(wrangler.mainInterface.browser)
            ->GetSceneState();
    EXPECT_EQ(scene_state_, main_browser_scene_state);

    // Test that once created the BVC isn't re-created.
    EXPECT_EQ(bvc, wrangler.mainInterface.viewController);

    // Test that the OTR objects are (a) OTR and (b) not the same as the non-OTR
    // objects.
    EXPECT_NE(bvc, wrangler.incognitoInterface.viewController);
    EXPECT_TRUE(wrangler.incognitoInterface.browserState->IsOffTheRecord());

    // Test that the OTR browser has scene_state_ associated with it.
    SceneState* otr_browser_scene_state =
        SceneStateBrowserAgent::FromBrowser(wrangler.incognitoInterface.browser)
            ->GetSceneState();
    EXPECT_EQ(scene_state_, otr_browser_scene_state);

    [wrangler shutdown];
  }
}

TEST_F(BrowserViewWranglerTest, TestBrowserList) {
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(chrome_browser_state_.get());
  TestBrowserListObserver observer;
  browser_list->AddObserver(&observer);

  BrowserViewWrangler* wrangler = [[BrowserViewWrangler alloc]
             initWithBrowserState:chrome_browser_state_.get()
                       sceneState:scene_state_
       applicationCommandEndpoint:nil
      browsingDataCommandEndpoint:nil];

  // After creating the main browser, it should have been added to the browser
  // list.
  [wrangler createMainBrowser];
  [wrangler createMainCoordinatorAndInterface];
  EXPECT_EQ(wrangler.mainInterface.browser, observer.GetLastAddedBrowser());
  EXPECT_EQ(1UL, browser_list->AllRegularBrowsers().size());
  // The lazy OTR browser creation should involve an addition to the browser
  // list.
  EXPECT_EQ(wrangler.incognitoInterface.browser,
            observer.GetLastAddedIncognitoBrowser());
  EXPECT_EQ(1UL, browser_list->AllIncognitoBrowsers().size());

  Browser* prior_otr_browser = observer.GetLastAddedIncognitoBrowser();

  // WARNING: after the following call, |last_otr_browser| is unsafe.
  [wrangler willDestroyIncognitoBrowserState];
  chrome_browser_state_->DestroyOffTheRecordChromeBrowserState();
  chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  [wrangler incognitoBrowserStateCreated];

  // Expect that the prior OTR browser was removed, and a new one was added.
  EXPECT_EQ(prior_otr_browser, observer.GetLastRemovedIncognitoBrowser());
  EXPECT_EQ(wrangler.incognitoInterface.browser,
            observer.GetLastAddedIncognitoBrowser());
  // There still should be one OTR browser.
  EXPECT_EQ(1UL, browser_list->AllIncognitoBrowsers().size());

  // Store unsafe pointers to the current browsers.
  Browser* pre_shutdown_main_browser = wrangler.mainInterface.browser;
  Browser* pre_shutdown_incognito_browser = wrangler.incognitoInterface.browser;

  // After shutdown all browsers are destroyed.
  [wrangler shutdown];
  // There should be no browsers in the BrowserList.
  EXPECT_EQ(0UL, browser_list->AllRegularBrowsers().size());
  EXPECT_EQ(0UL, browser_list->AllIncognitoBrowsers().size());
  // Both browser removals should have been observed.
  EXPECT_EQ(pre_shutdown_main_browser, observer.GetLastRemovedBrowser());
  EXPECT_EQ(pre_shutdown_incognito_browser,
            observer.GetLastRemovedIncognitoBrowser());

  browser_list->RemoveObserver(&observer);
}

}  // namespace
