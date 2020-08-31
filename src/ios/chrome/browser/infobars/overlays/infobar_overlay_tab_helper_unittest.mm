// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_tab_helper.h"

#import <Foundation/Foundation.h>

#include "components/infobars/core/infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/fake_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#include "ios/chrome/browser/infobars/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarDelegate;
using infobars::InfoBarManager;

// Test fixture for InfobarOverlayTabHelper.
class InfobarOverlayTabHelperTest : public PlatformTest {
 public:
  InfobarOverlayTabHelperTest() {
    web_state_.SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, std::make_unique<FakeInfobarOverlayRequestFactory>());
    InfobarOverlayTabHelper::CreateForWebState(&web_state_);
  }

  // Returns the front request of |web_state_|'s OverlayRequestQueue.
  OverlayRequest* front_request() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarBanner)
        ->front_request();
  }
  InfoBarManager* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }

 private:
  web::TestWebState web_state_;
};

// Tests that adding an InfoBar to the manager creates a fake banner request.
TEST_F(InfobarOverlayTabHelperTest, AddInfoBar) {
  ASSERT_FALSE(front_request());
  manager()->AddInfoBar(std::make_unique<FakeInfobarIOS>());
  ASSERT_TRUE(front_request());
}
