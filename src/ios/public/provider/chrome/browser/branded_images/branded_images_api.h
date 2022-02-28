// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BRANDED_IMAGES_BRANDED_IMAGES_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BRANDED_IMAGES_BRANDED_IMAGES_API_H_

#import <UIKit/UIKit.h>

namespace ios {
namespace provider {

// Enumeration corresponding to the different branded images.
enum class BrandedImage {
  // The image to use for the "account and activity" item on the clear
  // browsing data settings screen.
  kClearBrowsingDataAccountActivity,

  // The image to use for the "account and activity" item on the clear
  // browsing data settings screen.
  kClearBrowsingDataSiteData,

  // The image corresponding to the application logo for the what's new
  // promo.
  kWhatsNewLogo,

  // The image corresponding to the application logo with a rounded corner
  // rectangle surrounding it for the what's new promo.
  kWhatsNewLogoRoundedRectangle,

  // The image to use for the "Download Google Drive" icon on Download
  // Manager UI.
  kDownloadGoogleDrive,

  // The image to use for the fallback icon for answers in the omnibox
  // popup and in the omnibox as the default search engine icon.
  kOmniboxAnswer,

  // The image used for the "Stay Safe" default browser promo.
  kStaySafePromo,

  // The image used for the "Made for iOS" default browser promo.
  kMadeForIOSPromo,

  // The image used for the "Made for iPadOS" default browser promo.
  kMadeForIPadOSPromo,

  // The image used for the non-modal default browser promo.
  kNonModalDefaultBrowserPromo,
};

// Return the branded image corresponding to |branded_image|.
UIImage* GetBrandedImage(BrandedImage branded_image);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BRANDED_IMAGES_BRANDED_IMAGES_API_H_
