// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

#include "base/strings/string16.h"
#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"
#include "ui/gfx/image/image.h"

namespace infobars {
class InfoBar;
}

namespace confirm_infobar_overlays {

// Configuration object for OverlayRequests for the banner UI for an InfoBar
// with a ConfirmInfoBarDelegate.
class ConfirmBannerRequestConfig
    : public OverlayRequestConfig<ConfirmBannerRequestConfig> {
 public:
  ~ConfirmBannerRequestConfig() override;

  // The message text.
  base::string16 message_text() const { return message_text_; }

  // The button label text.
  base::string16 button_label_text() const { return button_label_text_; }

  // The infobar's icon image.
  gfx::Image icon_image() const { return icon_image_; }

 private:
  OVERLAY_USER_DATA_SETUP(ConfirmBannerRequestConfig);
  explicit ConfirmBannerRequestConfig(infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this banner.
  infobars::InfoBar* infobar_ = nullptr;
  // Configuration data extracted from |infobar_|'s confirm delegate.
  base::string16 message_text_;
  base::string16 button_label_text_;
  gfx::Image icon_image_;
};

}  // namespace confirm_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
