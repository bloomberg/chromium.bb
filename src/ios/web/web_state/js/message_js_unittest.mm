// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// JavaScript to return a frame's frameId.
NSString* const kGetFrameIdJs = @"__gCrWeb.message.getFrameId();";
}  // namespace

namespace web {

// Test fixture to test message.js.
typedef web::WebTestWithWebState MessageJsTest;

// Tests that a frameId is created.
TEST_F(MessageJsTest, FrameId) {
  ASSERT_TRUE(LoadHtml("<p>"));

  NSString* frame_id = ExecuteJavaScript(kGetFrameIdJs);
  // Validate frameId.
  EXPECT_TRUE(frame_id);
  EXPECT_TRUE([frame_id isKindOfClass:[NSString class]]);
  EXPECT_GT(frame_id.length, 0ul);
}

// Tests that the frameId is unique between two page loads.
TEST_F(MessageJsTest, UniqueFrameID) {
  ASSERT_TRUE(LoadHtml("<p>"));
  NSString* frame_id = ExecuteJavaScript(kGetFrameIdJs);

  ASSERT_TRUE(LoadHtml("<p>"));
  NSString* frame_id2 = ExecuteJavaScript(kGetFrameIdJs);
  // Validate second frameId.
  EXPECT_TRUE(frame_id2);
  EXPECT_TRUE([frame_id2 isKindOfClass:[NSString class]]);
  EXPECT_GT(frame_id2.length, 0ul);

  EXPECT_NSNE(frame_id, frame_id2);
}

}  // namespace web
