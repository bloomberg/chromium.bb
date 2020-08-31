// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/passwords/save_password_infobar_banner_overlay_mediator.h"

#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/password_form.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_feature.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/test/fake_infobar_ui_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Constants used in tests.
NSString* const kUsername = @"username";
NSString* const kPassword = @"12345";
}

// Test fixture for SavePasswordInfobarBannerOverlayMediator.
class SavePasswordInfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  SavePasswordInfobarBannerOverlayMediatorTest() {
    feature_list_.InitWithFeatures({kIOSInfobarUIReboot},
                                   {kInfobarUIRebootOnlyiOS13});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a SavePasswordInfobarBannerOverlayMediator correctly sets up its
// consumer.
TEST_F(SavePasswordInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  // Create an InfoBarIOS with a IOSChromeSavePasswordInfoBarDelegate.
  FakeInfobarUIDelegate* ui_delegate = [[FakeInfobarUIDelegate alloc] init];
  std::unique_ptr<IOSChromeSavePasswordInfoBarDelegate> passed_delegate =
      MockIOSChromeSavePasswordInfoBarDelegate::Create(kUsername, kPassword);
  IOSChromeSavePasswordInfoBarDelegate* delegate = passed_delegate.get();
  InfoBarIOS infobar(ui_delegate, std::move(passed_delegate));
  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request = OverlayRequest::CreateWithConfig<
      SavePasswordInfobarBannerOverlayRequestConfig>(&infobar);
  SavePasswordInfobarBannerOverlayMediator* mediator =
      [[SavePasswordInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  NSString* title = base::SysUTF16ToNSString(delegate->GetMessageText());
  NSString* password = [@"" stringByPaddingToLength:kPassword.length
                                         withString:@"•"
                                    startingAtIndex:0];
  NSString* subtitle =
      [NSString stringWithFormat:@"%@ %@", kUsername, password];
  NSString* bannerAccessibilityLabel =
      [NSString stringWithFormat:@"%@, %@, %@", title, kUsername,
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL)];
  EXPECT_NSEQ(bannerAccessibilityLabel, consumer.bannerAccessibilityLabel);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK)),
              consumer.buttonText);
  EXPECT_NSEQ(title, consumer.titleText);
  EXPECT_NSEQ(subtitle, consumer.subtitleText);
  EXPECT_NSEQ([UIImage imageNamed:@"infobar_passwords_icon"],
              consumer.iconImage);
  EXPECT_TRUE(consumer.presentsModal);
}
