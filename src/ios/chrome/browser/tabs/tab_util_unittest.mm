// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_util.h"

#include "components/search_engines/template_url.h"
#import "ios/web/public/navigation_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef PlatformTest TabUtilsTest;

TEST_F(TabUtilsTest, CreateWebLoadParamsWithoutPost) {
  // No post params, check URL and transition.
  GURL url("http://test.info/");
  web::NavigationManager::WebLoadParams params = CreateWebLoadParams(
      url, ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK,
      /*post_data=*/nullptr);
  EXPECT_EQ(params.url, url);
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      params.transition_type,
      ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK));
  // There should be no post data, and no extra headers.
  EXPECT_EQ(params.post_data, nil);
  EXPECT_EQ(params.extra_headers, nil);
}

TEST_F(TabUtilsTest, CreateWebLoadParamsWithPost) {
  // With post params.
  GURL url("http://test.info/");
  std::string post_data = "sphinx of black quartz judge my vow";
  TemplateURLRef::PostContent post_content("text/plain", post_data);
  web::NavigationManager::WebLoadParams params =
      CreateWebLoadParams(url, ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT,
                          /*post_data=*/&post_content);
  EXPECT_EQ(params.url, url);
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      params.transition_type, ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT));
  // Post data should be the same length as post_data
  EXPECT_EQ(params.post_data.length, post_data.length());
  EXPECT_NSEQ(params.extra_headers[@"Content-Type"], @"text/plain");
}
