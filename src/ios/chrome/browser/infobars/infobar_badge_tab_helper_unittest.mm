// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"

#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_container_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_confirm_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// InfobarTabHelperDelegate for testing.
@interface InfobarBadgeTabHelperTestDelegate
    : NSObject <InfobarBadgeTabHelperDelegate>
@property(nonatomic, assign) BOOL displayingBadge;
@end

@implementation InfobarBadgeTabHelperTestDelegate
@synthesize badgeState = _badgeState;
- (void)displayBadge:(BOOL)display {
  self.displayingBadge = display;
}
@end

// InfobarBadgeUIDelegate for testing.
// TODO(crbug.com/892376): Once InfobarContainerMediator stops using TabModel we
// should be able to use it instead of this fake.
@interface InfobarBadgeUITestDelegate : NSObject <InfobarBadgeUIDelegate>
@property(nonatomic, assign) InfobarBadgeTabHelper* infobarBadgeTabHelper;
@end

@implementation InfobarBadgeUITestDelegate
- (void)infobarBannerWasDismissed {
  self.infobarBadgeTabHelper->UpdateBadgeForInfobarBannerDismissed();
}
- (void)infobarModalWasPresented {
  self.infobarBadgeTabHelper->UpdateBadgeForInfobarModalPresented();
}
- (void)infobarModalWasDismissed {
  self.infobarBadgeTabHelper->UpdateBadgeForInfobarModalDismissed();
}
- (void)infobarWasAccepted {
  self.infobarBadgeTabHelper->UpdateBadgeForInfobarAccepted();
}
@end

// Fake Infobar Container.
@interface FakeInfobarContainerCoordinator : NSObject <InfobarContainerConsumer>
@property(nonatomic, strong) UIViewController* baseViewController;
@property(nonatomic, strong) InfobarCoordinator* infobarCoordinator;
@property(nonatomic, assign) BOOL bannerIsPresenting;
- (void)presentModal;
- (void)destroyInfobar;
- (void)removeInfobarView;
@end

@implementation FakeInfobarContainerCoordinator
- (void)addInfoBarWithDelegate:(id<InfobarUIDelegate>)infoBarDelegate {
  self.infobarCoordinator = static_cast<InfobarCoordinator*>(infoBarDelegate);
  self.infobarCoordinator.baseViewController = self.baseViewController;
  [self.infobarCoordinator start];
  self.bannerIsPresenting = YES;
  [self.infobarCoordinator presentInfobarBannerAnimated:NO completion:nil];
}

- (void)setUserInteractionEnabled:(BOOL)enabled {
}
- (void)updateLayoutAnimated:(BOOL)animated {
}
- (void)presentModal {
  [self.infobarCoordinator presentInfobarModal];
}
- (void)dismissBanner {
  [self.infobarCoordinator dismissInfobarBanner:self
                                       animated:NO
                                     completion:^{
                                       self.bannerIsPresenting = NO;
                                     }];
}
- (void)destroyInfobar {
  [self.infobarCoordinator detachView];
}
- (void)removeInfobarView {
  [self.infobarCoordinator removeView];
}
@end

// Test fixture for testing InfobarBadgeTabHelper.
class InfobarBadgeTabHelperTest : public PlatformTest {
 protected:
  InfobarBadgeTabHelperTest()
      : infobar_badge_tab_delegate_(
            [[InfobarBadgeTabHelperTestDelegate alloc] init]),
        browser_state_(TestChromeBrowserState::Builder().Build()),
        infobar_container_coordinator_(
            [[FakeInfobarContainerCoordinator alloc] init]),
        infobar_badge_ui_delegate_([[InfobarBadgeUITestDelegate alloc] init]) {
    // Enable kInfobarUIReboot flag.
    feature_list_.InitAndEnableFeature(kInfobarUIReboot);

    // Setup navigation manager. Needed for InfobarManager.
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager->SetBrowserState(browser_state_.get());
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    web_state_.SetBrowserState(browser_state_.get());

    // Create the InfobarManager for web_state_.
    InfoBarManagerImpl::CreateForWebState(&web_state_);

    // Create the InfobarBadgeTabHelper for web_state_ and set its delegate.
    InfobarBadgeTabHelper::CreateForWebState(&web_state_);
    tab_helper()->SetDelegate(infobar_badge_tab_delegate_);

    // Configure the fake InfobarContainerCoordinator, and set its baseVC as
    // rootVC.
    infobar_container_coordinator_.baseViewController =
        [[UIViewController alloc] init];
    [scoped_key_window_.Get()
        setRootViewController:infobar_container_coordinator_
                                  .baseViewController];

    // Configure the fake InfobarBadgeUIDelegate.
    infobar_badge_ui_delegate_.infobarBadgeTabHelper = tab_helper();

    // Create InfobarContainerIOS.
    infobar_container_model_.reset(
        new InfoBarContainerIOS(infobar_container_coordinator_, nil));
    infobar_container_model_->ChangeInfoBarManager(GetInfobarManager());

    // Create and configure the InfobarCoordinator.
    TestInfoBarDelegate* test_infobar_delegate =
        new TestInfoBarDelegate(@"Title");
    InfobarConfirmCoordinator* coordinator = [[InfobarConfirmCoordinator alloc]
        initWithInfoBarDelegate:test_infobar_delegate];
    coordinator.browserState = browser_state_.get();
    coordinator.badgeDelegate = infobar_badge_ui_delegate_;

    // Create the InfobarIOS using the InfobarCoordinator and add it to the
    // InfobarManager, this will trigger the Infobar presentation.
    std::unique_ptr<ConfirmInfoBarDelegate> infobar_delegate =
        std::unique_ptr<ConfirmInfoBarDelegate>(test_infobar_delegate);
    GetInfobarManager()->AddInfoBar(
        std::make_unique<InfoBarIOS>(coordinator, std::move(infobar_delegate)));
  }

  ~InfobarBadgeTabHelperTest() override {
    if (infobar_container_coordinator_.bannerIsPresenting) {
      [infobar_container_coordinator_ dismissBanner];
      EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, ^bool {
            return !infobar_container_coordinator_.bannerIsPresenting;
          }));
    }
  }

  // Returns InfobarBadgeTabHelper attached to web_state_.
  InfobarBadgeTabHelper* tab_helper() {
    return InfobarBadgeTabHelper::FromWebState(&web_state_);
  }

  // Returns InfoBarManager attached to web_state_.
  infobars::InfoBarManager* GetInfobarManager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }

  base::test::ScopedTaskEnvironment environment_;
  InfobarBadgeTabHelperTestDelegate* infobar_badge_tab_delegate_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  FakeInfobarContainerCoordinator* infobar_container_coordinator_;
  InfobarBadgeUITestDelegate* infobar_badge_ui_delegate_;
  base::test::ScopedFeatureList feature_list_;
  web::TestWebState web_state_;
  web::TestNavigationManager* navigation_manager_;
  std::unique_ptr<InfoBarContainerIOS> infobar_container_model_;
  ScopedKeyWindow scoped_key_window_;
};

// Test each UpdateBadge public method individually.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeStates) {
  // Test that dismissing the Banner unselects the badge.
  tab_helper()->UpdateBadgeForInfobarBannerDismissed();
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeState &
               InfobarBadgeStateSelected);
  // Test that dismissing the Modal unselects the badge.
  tab_helper()->UpdateBadgeForInfobarModalDismissed();
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeState &
               InfobarBadgeStateSelected);
  // Test that presenting the Modal selects the badge.
  tab_helper()->UpdateBadgeForInfobarModalPresented();
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeState &
              InfobarBadgeStateSelected);
  // Test that accepting the Infobar sets the badge to accepted state.
  tab_helper()->UpdateBadgeForInfobarAccepted();
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeState &
              InfobarBadgeStateAccepted);
}

// Tests that dismissing or presenting the Modal doesn't change the previously
// accepted state.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeAcceptedState) {
  // Accept the Infobar.
  tab_helper()->UpdateBadgeForInfobarAccepted();
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeState &
              InfobarBadgeStateAccepted);
  // Test badge is still "accepted" after Modal presentation.
  tab_helper()->UpdateBadgeForInfobarModalPresented();
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeState &
              InfobarBadgeStateAccepted);
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeState &
              InfobarBadgeStateSelected);
  // Test badge is still "accepted" after Modal dismissal.
  tab_helper()->UpdateBadgeForInfobarModalDismissed();
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeState &
              InfobarBadgeStateAccepted);
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeState &
               InfobarBadgeStateSelected);
}

// Test the initial badge state once the banner has been presented.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeStateOnBannerPresentation) {
  EXPECT_TRUE(infobar_badge_tab_delegate_.displayingBadge);
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeState &
              InfobarBadgeStateSelected);
}

// Tests that once the Modal is presented the default state is
// InfobarBadgeStateSelected.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeStateOnModalPresentation) {
  [infobar_container_coordinator_ dismissBanner];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !infobar_container_coordinator_.bannerIsPresenting;
      }));
  [infobar_container_coordinator_ presentModal];
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeState &
              InfobarBadgeStateSelected);
  EXPECT_TRUE(infobar_badge_tab_delegate_.displayingBadge);
}

// Tests that the InfobarBadge is still being displayed after dismissing the
// InfobarBanner.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnBannerDismissal) {
  [infobar_container_coordinator_ dismissBanner];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !infobar_container_coordinator_.bannerIsPresenting;
      }));
  EXPECT_TRUE(infobar_badge_tab_delegate_.displayingBadge);
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeState &
               InfobarBadgeStateSelected);
}

// Test that the Accepted badge state remains after dismissing the
// InfobarBanner.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnBannerAccepted) {
  EXPECT_FALSE(infobar_badge_tab_delegate_.badgeState &
               InfobarBadgeStateAccepted);
  tab_helper()->UpdateBadgeForInfobarAccepted();
  [infobar_container_coordinator_ dismissBanner];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !infobar_container_coordinator_.bannerIsPresenting;
      }));
  EXPECT_TRUE(infobar_badge_tab_delegate_.badgeState &
              InfobarBadgeStateAccepted);
}

// Test that removing the InfobarView doesn't stop displaying the badge.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnInfobarViewRemoval) {
  [infobar_container_coordinator_ dismissBanner];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !infobar_container_coordinator_.bannerIsPresenting;
      }));
  [infobar_container_coordinator_ removeInfobarView];
  EXPECT_TRUE(infobar_badge_tab_delegate_.displayingBadge);
}

// Test that destroying the InfobarView stops displaying the badge.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnInfobarDestroyal) {
  [infobar_container_coordinator_ dismissBanner];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return !infobar_container_coordinator_.bannerIsPresenting;
      }));
  [infobar_container_coordinator_ destroyInfobar];
  EXPECT_FALSE(infobar_badge_tab_delegate_.displayingBadge);
}
