// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <QuickLook/QuickLook.h>

#include <memory>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/download/download_test_util.h"
#include "ios/chrome/browser/download/features.h"
#include "ios/chrome/browser/download/usdz_mime_type.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using chrome_test_util::GetMainController;

namespace {

// USDZ landing page and download request handler.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);

  if (request.GetURL().path() == "/") {
    result->set_content(
        "<a id='forbidden' href='/forbidden'>Forbidden</a>"
        "<a id='unauthorized' href='/unauthorized'>Unauthorized</a>"
        "<a id='changing-mime-type' href='/changing-mime-type'>Changing Mime "
        "Type</a>"
        "<a id='good' href='/good'>Good</a>");
    return result;
  }

  result->AddCustomHeader("Content-Type", kUsdzMimeType);
  result->set_content(testing::GetTestFileContents(testing::kUsdzFilePath));

  if (request.GetURL().path() == "/forbidden") {
    result->set_code(net::HTTP_FORBIDDEN);
  } else if (request.GetURL().path() == "/unauthorized") {
    result->set_code(net::HTTP_UNAUTHORIZED);
  } else if (request.GetURL().path() == "/changing-mime-type") {
    result->AddCustomHeader("Content-Type", "unkown");
  }

  return result;
}

}  // namespace

// Tests previewing USDZ format files.
@interface ARQuickLookEGTest : ChromeTestCase

@end

@implementation ARQuickLookEGTest

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(base::BindRepeating(&GetResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that QLPreviewController is shown for sucessfully downloaded USDZ file.
- (void)testDownloadUsdz {
  if (!download::IsUsdzPreviewEnabled()) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled if 'USDZPreview' feature is disabled or on iOS versions "
         "below 12 because QLPreviewController is not available.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebViewContainingText:"Good"];
  [ChromeEarlGrey tapWebViewElementWithID:@"good"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI. Instead this test relies on view
  // controller presentation as the signal that QLPreviewController UI is shown.
  UIViewController* BVC = GetMainController().browserViewInformation.mainBVC;
  bool shown = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    UIViewController* presentedController = BVC.presentedViewController;
    return [presentedController class] == [QLPreviewController class];
  });
  GREYAssert(shown, @"QLPreviewController was not shown.");
}

- (void)testDownloadUnauthorized {
  if (!download::IsUsdzPreviewEnabled()) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled if 'USDZPreview' feature is disabled or on iOS versions "
         "below 12 because QLPreviewController is not available.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebViewContainingText:"Unauthorized"];
  [ChromeEarlGrey tapWebViewElementWithID:@"unauthorized"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI. Instead this test relies on view
  // controller presentation as the signal that QLPreviewController UI is shown.
  UIViewController* BVC = GetMainController().browserViewInformation.mainBVC;
  bool shown = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    UIViewController* presentedController = BVC.presentedViewController;
    return [presentedController class] == [QLPreviewController class];
  });
  GREYAssertFalse(shown, @"QLPreviewController should not have shown.");
}

- (void)testDownloadForbidden {
  if (!download::IsUsdzPreviewEnabled()) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled if 'USDZPreview' feature is disabled or on iOS versions "
         "below 12 because QLPreviewController is not available.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebViewContainingText:"Forbidden"];
  [ChromeEarlGrey tapWebViewElementWithID:@"forbidden"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI. Instead this test relies on view
  // controller presentation as the signal that QLPreviewController UI is shown.
  UIViewController* BVC = GetMainController().browserViewInformation.mainBVC;
  bool shown = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    UIViewController* presentedController = BVC.presentedViewController;
    return [presentedController class] == [QLPreviewController class];
  });
  GREYAssertFalse(shown, @"QLPreviewController should not have shown.");
}

- (void)testDownloadChangingMimeType {
  if (!download::IsUsdzPreviewEnabled()) {
    EARL_GREY_TEST_SKIPPED(
        @"Disabled if 'USDZPreview' feature is disabled or on iOS versions "
         "below 12 because QLPreviewController is not available.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebViewContainingText:"Changing Mime Type"];
  [ChromeEarlGrey tapWebViewElementWithID:@"changing-mime-type"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI. Instead this test relies on view
  // controller presentation as the signal that QLPreviewController UI is shown.
  UIViewController* BVC = GetMainController().browserViewInformation.mainBVC;
  bool shown = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    UIViewController* presentedController = BVC.presentedViewController;
    return [presentedController class] == [QLPreviewController class];
  });
  GREYAssertFalse(shown, @"QLPreviewController should not have shown.");
}

@end
