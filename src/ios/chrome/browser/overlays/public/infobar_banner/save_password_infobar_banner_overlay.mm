// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

namespace {
// The name of the icon image for the save passwords banner.
NSString* const kIconImageName = @"infobar_passwords_icon";
}

OVERLAY_USER_DATA_SETUP_IMPL(SavePasswordInfobarBannerOverlayRequestConfig);

SavePasswordInfobarBannerOverlayRequestConfig::
    SavePasswordInfobarBannerOverlayRequestConfig(InfoBar* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  IOSChromeSavePasswordInfoBarDelegate* delegate =
      IOSChromeSavePasswordInfoBarDelegate::FromInfobarDelegate(
          infobar_->delegate());
  message_ = base::SysUTF16ToNSString(delegate->GetMessageText());
  username_ = delegate->GetUserNameText();
  button_text_ = base::SysUTF16ToNSString(
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  icon_image_name_ = kIconImageName;
  password_length_ = delegate->GetPasswordText().length;
}

SavePasswordInfobarBannerOverlayRequestConfig::
    ~SavePasswordInfobarBannerOverlayRequestConfig() = default;

void SavePasswordInfobarBannerOverlayRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner);
}
