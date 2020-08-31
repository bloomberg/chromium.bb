// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/generate_qr_code_activity.h"

#include "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/activity_services/data/share_to_data.h"
#include "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for covering the GenerateQrCodeActivity class.
class GenerateQrCodeActivityTest : public PlatformTest {
 protected:
  GenerateQrCodeActivityTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    mocked_handler_ = OCMStrictProtocolMock(@protocol(QRGenerationCommands));
  }

  // Creates a GenerateQrCodeActivity instance.
  GenerateQrCodeActivity* CreateActivity() {
    return
        [[GenerateQrCodeActivity alloc] initWithURL:GURL("https://example.com")
                                              title:@"Some title"
                                            handler:mocked_handler_];
  }

  base::test::ScopedFeatureList scoped_features_;
  id mocked_handler_;
};

// Tests that the activity can be performed when the feature flag is on.
TEST_F(GenerateQrCodeActivityTest, FlagOn_ActivityEnabled) {
  scoped_features_.InitAndEnableFeature(kQRCodeGeneration);
  GenerateQrCodeActivity* activity = CreateActivity();

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the activity cannot be performed when the feature flag is off.
TEST_F(GenerateQrCodeActivityTest, FlagOff_ActivityDisabled) {
  scoped_features_.InitAndDisableFeature(kQRCodeGeneration);
  GenerateQrCodeActivity* activity = CreateActivity();

  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the right handler method is invoked upon execution.
TEST_F(GenerateQrCodeActivityTest, Execution) {
  __block GURL fakeURL("https://example.com");
  __block NSString* fakeTitle = @"fake title";

  [[mocked_handler_ expect]
      generateQRCode:[OCMArg
                         checkWithBlock:^BOOL(GenerateQRCodeCommand* value) {
                           EXPECT_EQ(fakeURL, value.URL);
                           EXPECT_EQ(fakeTitle, value.title);
                           return YES;
                         }]];

  GenerateQrCodeActivity* activity =
      [[GenerateQrCodeActivity alloc] initWithURL:fakeURL
                                            title:fakeTitle
                                          handler:mocked_handler_];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
}
