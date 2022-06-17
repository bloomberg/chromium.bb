// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller+private.h"

#import <Foundation/Foundation.h>
#import <PassKit/PassKit.h>

#import <memory>

#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/prerender/fake_prerender_service.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_helper.h"
#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_coordinator.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_legacy_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_adaptor.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Private methods in BrowserViewController to test.
@interface BrowserViewController (Testing)

- (void)webStateSelected:(web::WebState*)webState
           notifyToolbar:(BOOL)notifyToolbar;
@end

#pragma mark -

namespace {
class BrowserViewControllerTest : public BlockCleanupTest {
 public:
 protected:
  BrowserViewControllerTest()
      : scene_state_([[SceneState alloc] initWithAppState:nil]) {}

  void SetUp() override {
    BlockCleanupTest::SetUp();
    // Set up a TestChromeBrowserState instance.
    TestChromeBrowserState::Builder test_cbs_builder;

    test_cbs_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());

    chrome_browser_state_ = test_cbs_builder.Build();

    id passKitController =
        [OCMockObject niceMockForClass:[PKAddPassesViewController class]];
    passKitViewController_ = passKitController;

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());

    WebUsageEnablerBrowserAgent::FromBrowser(browser_.get())
        ->SetWebUsageEnabled(true);

    SessionRestorationBrowserAgent::CreateForBrowser(
        browser_.get(), [[TestSessionService alloc] init]);
    SessionRestorationBrowserAgent::FromBrowser(browser_.get())
        ->SetSessionID([[NSUUID UUID] UUIDString]);

    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

    id mockActivityServiceCommandHandler =
        OCMProtocolMock(@protocol(ActivityServiceCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockActivityServiceCommandHandler
                     forProtocol:@protocol(ActivityServiceCommands)];
    id mockFindInPageCommandHandler =
        OCMProtocolMock(@protocol(FindInPageCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockFindInPageCommandHandler
                     forProtocol:@protocol(FindInPageCommands)];
    id mockTextZoomCommandHandler =
        OCMProtocolMock(@protocol(TextZoomCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockTextZoomCommandHandler
                     forProtocol:@protocol(TextZoomCommands)];
    id mockPageInfoCommandHandler =
        OCMProtocolMock(@protocol(PageInfoCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockPageInfoCommandHandler
                     forProtocol:@protocol(PageInfoCommands)];

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to ApplicationSettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockApplicationSettingsCommandHandler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockApplicationCommandHandler
                     forProtocol:@protocol(ApplicationCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mockApplicationSettingsCommandHandler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    // Create three web states.
    for (int i = 0; i < 3; i++) {
      web::WebState::CreateParams params(chrome_browser_state_.get());
      std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
      AttachTabHelpers(webState.get(), NO);
      browser_->GetWebStateList()->InsertWebState(0, std::move(webState), 0,
                                                  WebStateOpener());
      browser_->GetWebStateList()->ActivateWebStateAt(0);
    }

    // Load TemplateURLService.
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    template_url_service->Load();

    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());

    container_ = [[BrowserContainerViewController alloc] init];
    bvc_helper_ = [[BrowserViewControllerHelper alloc] init];
    key_commands_provider_ = [[KeyCommandsProvider alloc] init];

    fake_prerender_service_ = std::make_unique<FakePrerenderService>();

    bubble_presenter_ = [[BubblePresenter alloc]
        initWithBrowserState:chrome_browser_state_.get()];

    download_manager_coordinator_ = [[DownloadManagerCoordinator alloc]
        initWithBaseViewController:[[UIViewController alloc] init]
                           browser:browser_.get()];

    toolbar_coordinator_adaptor_ = [[ToolbarCoordinatorAdaptor alloc]
        initWithDispatcher:browser_->GetCommandDispatcher()];

    primary_toolbar_coordinator_ =
        [[PrimaryToolbarCoordinator alloc] initWithBrowser:browser_.get()];

    secondary_toolbar_coordinator_ =
        [[SecondaryToolbarCoordinator alloc] initWithBrowser:browser_.get()];

    tab_strip_coordinator_ =
        [[TabStripCoordinator alloc] initWithBrowser:browser_.get()];

    legacy_tab_strip_coordinator_ =
        [[TabStripLegacyCoordinator alloc] initWithBrowser:browser_.get()];

    BrowserViewControllerDependencies dependencies;
    dependencies.prerenderService = fake_prerender_service_.get();
    dependencies.bubblePresenter = bubble_presenter_;
    dependencies.downloadManagerCoordinator = download_manager_coordinator_;
    dependencies.toolbarInterface = toolbar_coordinator_adaptor_;
    dependencies.primaryToolbarCoordinator = primary_toolbar_coordinator_;
    dependencies.secondaryToolbarCoordinator = secondary_toolbar_coordinator_;
    dependencies.tabStripCoordinator = tab_strip_coordinator_;
    dependencies.legacyTabStripCoordinator = legacy_tab_strip_coordinator_;

    bvc_ = [[BrowserViewController alloc]
                       initWithBrowser:browser_.get()
        browserContainerViewController:container_
           browserViewControllerHelper:bvc_helper_
                            dispatcher:browser_->GetCommandDispatcher()
                   keyCommandsProvider:key_commands_provider_
                          dependencies:dependencies];

    // Force the view to load.
    UIWindow* window = [[UIWindow alloc] initWithFrame:CGRectZero];
    [window addSubview:[bvc_ view]];
    window_ = window;
  }

  void TearDown() override {
    [download_manager_coordinator_ stop];

    [[bvc_ view] removeFromSuperview];
    [bvc_ shutdown];

    BlockCleanupTest::TearDown();
  }

  web::WebState* ActiveWebState() {
    return browser_->GetWebStateList()->GetActiveWebState();
  }

  MOCK_METHOD0(OnCompletionCalled, void());

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<PrerenderService> fake_prerender_service_;
  BrowserViewControllerHelper* bvc_helper_;
  KeyCommandsProvider* key_commands_provider_;
  PKAddPassesViewController* passKitViewController_;
  OCMockObject* dependencyFactory_;
  CommandDispatcher* command_dispatcher_;
  BubblePresenter* bubble_presenter_;
  BrowserContainerViewController* container_;
  BrowserViewController* bvc_;
  UIWindow* window_;
  SceneState* scene_state_;
  DownloadManagerCoordinator* download_manager_coordinator_;
  ToolbarCoordinatorAdaptor* toolbar_coordinator_adaptor_;
  PrimaryToolbarCoordinator* primary_toolbar_coordinator_;
  SecondaryToolbarCoordinator* secondary_toolbar_coordinator_;
  TabStripCoordinator* tab_strip_coordinator_;
  TabStripLegacyCoordinator* legacy_tab_strip_coordinator_;
};

TEST_F(BrowserViewControllerTest, TestWebStateSelected) {
  [bvc_ webStateSelected:ActiveWebState() notifyToolbar:YES];
  EXPECT_EQ(ActiveWebState()->GetView().superview, container_.view);
  EXPECT_TRUE(ActiveWebState()->IsVisible());
}

TEST_F(BrowserViewControllerTest, TestClearPresentedState) {
  EXPECT_CALL(*this, OnCompletionCalled());
  [bvc_
      clearPresentedStateWithCompletion:^{
        this->OnCompletionCalled();
      }
                         dismissOmnibox:YES];
}

// Verifies the the next/previous tab commands from the keyboard work OK.
TEST_F(BrowserViewControllerTest, TestFocusNextPrevious) {
  // Add more web states.
  WebStateList* web_state_list = browser_->GetWebStateList();
  // This test assumes there are exactly three web states in the list.
  ASSERT_EQ(web_state_list->count(), 3);

  ASSERT_TRUE([bvc_ conformsToProtocol:@protocol(KeyCommandsPlumbing)]);

  id<KeyCommandsPlumbing> keyHandler =
      static_cast<id<KeyCommandsPlumbing>>(bvc_);

  [keyHandler focusNextTab];
  EXPECT_EQ(web_state_list->active_index(), 1);
  [keyHandler focusNextTab];
  EXPECT_EQ(web_state_list->active_index(), 2);
  [keyHandler focusNextTab];
  EXPECT_EQ(web_state_list->active_index(), 0);
  [keyHandler focusPreviousTab];
  EXPECT_EQ(web_state_list->active_index(), 2);
  [keyHandler focusPreviousTab];
  EXPECT_EQ(web_state_list->active_index(), 1);
  [keyHandler focusPreviousTab];
  EXPECT_EQ(web_state_list->active_index(), 0);
}

// Tests that WebState::WasShown() and WebState::WasHidden() is properly called
// for WebState activations in the BrowserViewController's WebStateList.
TEST_F(BrowserViewControllerTest, UpdateWebStateVisibility) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(3, web_state_list->count());

  // Activate each WebState in the list and check the visibility.
  web_state_list->ActivateWebStateAt(0);
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->IsVisible(), true);
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(2)->IsVisible(), false);
  web_state_list->ActivateWebStateAt(1);
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->IsVisible(), true);
  EXPECT_EQ(web_state_list->GetWebStateAt(2)->IsVisible(), false);
  web_state_list->ActivateWebStateAt(2);
  EXPECT_EQ(web_state_list->GetWebStateAt(0)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(1)->IsVisible(), false);
  EXPECT_EQ(web_state_list->GetWebStateAt(2)->IsVisible(), true);
}

}  // namespace
