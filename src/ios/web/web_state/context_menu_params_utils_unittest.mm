// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/context_menu_params_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/web/common/referrer_util.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/web_state/context_menu_constants.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Text values for the tapped element triggering the context menu.
const char kLinkUrl[] = "http://link.url/";
const char kSrcUrl[] = "http://src.url/";
const char kTitle[] = "title";
const char kReferrerPolicy[] = "always";
const char kLinkText[] = "link text";
const char kJavaScriptLinkUrl[] = "javascript://src.url/";
const char kDataUrl[] = "data://foo.bar/";
const char kAlt[] = "alt text";
}

namespace web {

// Test fixture for ContextMenuParams utilities.
typedef PlatformTest ContextMenuParamsUtilsTest;

// Tests the empty contructor.
TEST_F(ContextMenuParamsUtilsTest, EmptyParams) {
  ContextMenuParams params;
  EXPECT_TRUE(params.is_main_frame);
  EXPECT_EQ(params.menu_title, nil);
  EXPECT_EQ(params.menu_title_origin, ContextMenuTitleOrigin::kUnknown);
  EXPECT_FALSE(params.link_url.is_valid());
  EXPECT_FALSE(params.src_url.is_valid());
  EXPECT_EQ(params.referrer_policy, ReferrerPolicyDefault);
  EXPECT_EQ(params.view, nil);
  EXPECT_TRUE(CGPointEqualToPoint(params.location, CGPointZero));
  EXPECT_EQ(params.link_text, nil);
}

// Tests the parsing of the element NSDictionary.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTest) {
  ContextMenuParams params = ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementHyperlink : @(kLinkUrl),
    kContextMenuElementSource : @(kSrcUrl),
    kContextMenuElementTitle : @(kTitle),
    kContextMenuElementReferrerPolicy : @(kReferrerPolicy),
    kContextMenuElementInnerText : @(kLinkText),
  });

  EXPECT_TRUE(params.is_main_frame);
  EXPECT_NSEQ(params.menu_title, @(kTitle));
  EXPECT_EQ(params.menu_title_origin, ContextMenuTitleOrigin::kImageTitle);
  EXPECT_EQ(params.link_url, GURL(kLinkUrl));
  EXPECT_EQ(params.src_url, GURL(kSrcUrl));
  EXPECT_NSEQ(params.link_text, @(kLinkText));
  EXPECT_EQ(params.referrer_policy, ReferrerPolicyFromString(kReferrerPolicy));

  EXPECT_EQ(params.view, nil);
  EXPECT_TRUE(CGPointEqualToPoint(params.location, CGPointZero));
}

// Tests title is set as the formatted URL when there is no title.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTestNoTitle) {
  ContextMenuParams params = ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementHyperlink : @(kLinkUrl),
  });
  base::string16 urlText = url_formatter::FormatUrl(GURL(kLinkUrl));
  NSString* title = base::SysUTF16ToNSString(urlText);

  EXPECT_NSEQ(params.menu_title, title);
  EXPECT_EQ(params.menu_title_origin, ContextMenuTitleOrigin::kURL);
}

// Tests title is set to "JavaScript" if there is no title and "href" links to
// JavaScript URL.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTestJavascriptTitle) {
  ContextMenuParams params = ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementHyperlink : @(kJavaScriptLinkUrl),
  });
  EXPECT_NSEQ(params.menu_title, @"JavaScript");
  EXPECT_EQ(params.menu_title_origin, ContextMenuTitleOrigin::kURL);
}

// Tests title is set to |src_url| if there is no title.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTestSrcTitle) {
  ContextMenuParams params = ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementSource : @(kSrcUrl),
  });
  EXPECT_EQ(params.src_url, GURL(kSrcUrl));
  EXPECT_NSEQ(params.menu_title, @(kSrcUrl));
  EXPECT_EQ(params.menu_title_origin, ContextMenuTitleOrigin::kURL);
}

// Tests title is set to nil if there is no title and src is a data URL.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTestDataTitle) {
  ContextMenuParams params = ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementSource : @(kDataUrl),
  });
  EXPECT_EQ(params.src_url, GURL(kDataUrl));
  EXPECT_NSEQ(params.menu_title, nil);
  EXPECT_EQ(params.menu_title_origin, ContextMenuTitleOrigin::kURL);
}

// Tests that a context menu will not be shown for an empty element dictionary.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestEmptyDictionary) {
  EXPECT_FALSE(CanShowContextMenuForElementDictionary(@{}));
}

// Tests that a context menu will not be shown for an element dictionary with
// only a request id.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestRequestIdOnly) {
  EXPECT_FALSE(CanShowContextMenuForElementDictionary(
      @{kContextMenuElementRequestId : @"kContextMenuElementRequestId"}));
}

// Tests that a context menu will be shown for a link.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestHyperlink) {
  EXPECT_TRUE(CanShowContextMenuForElementDictionary(@{
    kContextMenuElementHyperlink : @"http://example.com",
    kContextMenuElementInnerText : @"Click me."
  }));
}

// Tests that a context menu will not be shown for an invalid link.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestInvalidHyperlink) {
  EXPECT_FALSE(CanShowContextMenuForElementDictionary(
      @{kContextMenuElementHyperlink : @"invalid_url"}));
}

// Tests that a context menu will be shown for an image.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestImageWithTitle) {
  EXPECT_TRUE(CanShowContextMenuForElementDictionary(@{
    kContextMenuElementSource : @"http://example.com/image.jpeg",
    kContextMenuElementTitle : @"Image"
  }));
}

// Tests that a context menu will not be shown for an image with an invalid
// source url.
TEST_F(ContextMenuParamsUtilsTest,
       CanShowContextMenuTestImageWithInvalidSource) {
  EXPECT_FALSE(CanShowContextMenuForElementDictionary(@{
    kContextMenuElementSource : @"invalid_url",
  }));
}

// Tests that a context menu will be shown for a linked image.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestLinkedImage) {
  EXPECT_TRUE(CanShowContextMenuForElementDictionary(@{
    kContextMenuElementHyperlink : @"http://example.com",
    kContextMenuElementSource : @"http://example.com/image.jpeg"
  }));
}

// Tests that the menu title prepends the element's alt text if it is an image
// without a link.
TEST_F(ContextMenuParamsUtilsTest,
       DictionaryConstructorTestPrependAltForImage) {
  ContextMenuParams params = ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementSource : @(kSrcUrl),
    kContextMenuElementAlt : @(kAlt),
  });

  EXPECT_TRUE([params.menu_title hasPrefix:@(kAlt)]);
  EXPECT_TRUE([params.menu_title hasSuffix:@(kSrcUrl)]);
  EXPECT_EQ(params.menu_title_origin, ContextMenuTitleOrigin::kImageAltText);
}

// Tests that the menu title prepends the element's alt text if it is an image
// without a link.
TEST_F(ContextMenuParamsUtilsTest,
       DictionaryConstructorTestPrependAltForImageWithTitle) {
  ContextMenuParams params = ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementSource : @(kSrcUrl),
    kContextMenuElementTitle : @(kTitle),
    kContextMenuElementAlt : @(kAlt),
  });

  EXPECT_TRUE([params.menu_title hasPrefix:@(kAlt)]);
  EXPECT_TRUE([params.menu_title hasSuffix:@(kTitle)]);
  EXPECT_EQ(params.menu_title_origin, ContextMenuTitleOrigin::kImageTitle);
}

}  // namespace web
