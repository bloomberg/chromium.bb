// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"

#import "ios/web/public/test/web_test_with_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for VoiceSearchNavigations.
class VoiceSearchNavigationsTest : public web::WebTestWithWebState {
 public:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    VoiceSearchNavigationTabHelper::CreateForWebState(web_state());
  }

  VoiceSearchNavigationTabHelper* navigations() {
    return VoiceSearchNavigationTabHelper::FromWebState(web_state());
  }
};

// Tests that a navigation commit reset the value of
// IsExpectingVoiceSearch().
TEST_F(VoiceSearchNavigationsTest, CommitResetVoiceSearchExpectation) {
  navigations()->WillLoadVoiceSearchResult();
  EXPECT_TRUE(navigations()->IsExpectingVoiceSearch());
  LoadHtml(@"<html></html>");
  EXPECT_FALSE(navigations()->IsExpectingVoiceSearch());
}
