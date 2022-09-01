// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screenshot/screenshot_delegate.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface_provider.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ScreenshotDelegateTest : public PlatformTest {
 protected:
  ScreenshotDelegateTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
  }
  ~ScreenshotDelegateTest() override {}

  void SetUp() override {
    browser_interface_ = [[StubBrowserInterface alloc] init];
    browser_interface_provider_ = [[StubBrowserInterfaceProvider alloc] init];
    screenshot_service_ = OCMClassMock([UIScreenshotService class]);
  }

  void createScreenshotDelegate() {
    screenshotDelegate_ = [[ScreenshotDelegate alloc]
        initWithBrowserInterfaceProvider:browser_interface_provider_];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  StubBrowserInterface* browser_interface_;
  StubBrowserInterfaceProvider* browser_interface_provider_;
  ScreenshotDelegate* screenshotDelegate_;
  id screenshot_service_;
};

// Tests that ScreenshotDelegate can be init with browserInterfaceProvider can
// be set and that data can be generated from it.
TEST_F(ScreenshotDelegateTest, ScreenshotService) {
  // Expected: Empty NSData.
  auto web_state = std::make_unique<web::FakeWebState>();
  TestBrowser browser(browser_state_.get());

  CRWWebViewScrollViewProxy* scroll_view_proxy =
      [[CRWWebViewScrollViewProxy alloc] init];
  UIScrollView* scroll_view = [[UIScrollView alloc] init];
  [scroll_view_proxy setScrollView:scroll_view];
  id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
  [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
  web_state->SetWebViewProxy(web_view_proxy_mock);

  // Fake scroll_view contentOffset, contentSize and frame.
  CGPoint content_offset = CGPointMake(10.0, 15.0);
  CGSize content_size = CGSizeMake(425, 4000);
  CGRect frame = CGRectMake(0, 0, 375, 812);
  scroll_view.contentOffset = content_offset;
  scroll_view.contentSize = content_size;
  scroll_view.frame = frame;

  CGRect expected_rect_in_page = CGRectZero;

  expected_rect_in_page =
      CGRectMake(content_offset.x,
                 content_size.height - frame.size.height - content_offset.y,
                 frame.size.width, frame.size.height);

  // Insert the web_state into the Browser.
  int insertion_index = browser.GetWebStateList()->InsertWebState(
      WebStateList::kInvalidIndex, std::move(web_state),
      WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  browser.GetWebStateList()->ActivateWebStateAt(insertion_index);

  // Add the Browser to StubBrowserInterface.
  browser_interface_.browser = &browser;

  // Add the StubBrowserInterface to StubBrowserInterfaceProvider.
  browser_interface_provider_.currentInterface = browser_interface_;

  createScreenshotDelegate();

  __block int nbCalls = 0;
  [screenshotDelegate_ screenshotService:screenshot_service_
      generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                NSInteger indexOfCurrentPage,
                                                CGRect rectInCurrentPage) {
        EXPECT_TRUE(PDFData);
        EXPECT_TRUE(
            CGRectEqualToRect(expected_rect_in_page, rectInCurrentPage));
        ++nbCalls;
      }];

  EXPECT_EQ(1, nbCalls);
}

// Tests that when ScreenshotDelegate's browserInterfaceProvider has a nil
// Browser screenshotService will return nil.
TEST_F(ScreenshotDelegateTest, NilBrowser) {
  // Expected: nil NSData.
  // Add the StubBrowserInterface with no set Browser to
  // StubBrowserInterfaceProvider.
  browser_interface_provider_.currentInterface = browser_interface_;

  createScreenshotDelegate();

  __block bool callback_ran = false;
  [screenshotDelegate_ screenshotService:screenshot_service_
      generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                NSInteger indexOfCurrentPage,
                                                CGRect rectInCurrentPage) {
        EXPECT_FALSE(PDFData);
        callback_ran = true;
      }];

  EXPECT_TRUE(callback_ran);
}

// Tests that when ScreenshotDelegate's browserInterfaceProvider has a nil
// WebSatate screenshotService will return nil.
TEST_F(ScreenshotDelegateTest, NilWebState) {
  // Expected: nil NSData.
  TestBrowser browser(browser_state_.get());

  // Add the empty Browser to StubBrowserInterface.
  browser_interface_.browser = &browser;

  // Add the StubBrowserInterface to StubBrowserInterfaceProvider.
  browser_interface_provider_.currentInterface = browser_interface_;

  createScreenshotDelegate();

  __block bool callback_ran = false;
  [screenshotDelegate_ screenshotService:screenshot_service_
      generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                NSInteger indexOfCurrentPage,
                                                CGRect rectInCurrentPage) {
        EXPECT_FALSE(PDFData);
        callback_ran = true;
      }];

  EXPECT_TRUE(callback_ran);
}
