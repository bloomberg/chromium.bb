// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_overlay_request_callback_installer.h"

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/test/mock_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Fake version of InfobarModalOverlayRequestCallbackInstaller that supports all
// requests.
class FakeInfobarModalOverlayRequestCallbackInstaller
    : public InfobarModalOverlayRequestCallbackInstaller {
 public:
  FakeInfobarModalOverlayRequestCallbackInstaller(
      InfobarModalInteractionHandler* handler)
      : InfobarModalOverlayRequestCallbackInstaller(
            OverlayRequestSupport::All(),
            handler) {}
};
}  // namespace

// Test fixture for InfobarModalOverlayRequestCallbackInstaller.
class InfobarModalOverlayRequestCallbackInstallerTest : public PlatformTest {
 public:
  InfobarModalOverlayRequestCallbackInstallerTest()
      : request_(OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
            &infobar_,
            InfobarOverlayType::kModal)),
        installer_(&mock_handler_) {
    installer_.InstallCallbacks(request_.get());
  }

 protected:
  FakeInfobarIOS infobar_;
  std::unique_ptr<OverlayRequest> request_;
  MockInfobarModalInteractionHandler mock_handler_;
  FakeInfobarModalOverlayRequestCallbackInstaller installer_;
};

// Tests that a dispatched InfobarModalMainActionResponse calls
// InfobarModalInteractionHandler::MainButtonTapped().
TEST_F(InfobarModalOverlayRequestCallbackInstallerTest, MainAction) {
  EXPECT_CALL(mock_handler_, PerformMainAction(&infobar_));
  request_->GetCallbackManager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<InfobarModalMainActionResponse>());
}
