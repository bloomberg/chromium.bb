// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_style.h"

#include <algorithm>

namespace message_center {

// Exported values /////////////////////////////////////////////////////////////

// Square image sizes in pixels.
const int kNotificationButtonIconSize = 16;
const int kNotificationIconSize = 80;
// Same as kNotificationWidth.
const int kNotificationPreferredImageSize = 360;
const float kNotificationPreferredImageRatio = 1.5;
const int kSettingsIconSize = 16;

// Limits.
const size_t kMaxVisibleMessageCenterNotifications = 100;
const size_t kMaxVisiblePopupNotifications = 3;

// Colors.
const SkColor kMessageCenterBorderColor = SkColorSetRGB(0xC7, 0xCA, 0xCE);
const SkColor kMessageCenterShadowColor = SkColorSetARGB(0.5 * 255, 0, 0, 0);

// Within a notification ///////////////////////////////////////////////////////

// Pixel dimensions.
const int kControlButtonSize = 29;
const int kNotificationWidth = 360;
const int kIconToTextPadding = 16;
const int kTextTopPadding = 12;
const int kIconBottomPadding = 16;

// Text sizes.
const int kTitleFontSize = 14;
const int kTitleLineHeight = 20;
const int kMessageFontSize = 12;
const int kMessageLineHeight = 18;

// Colors.
const SkColor kNotificationBackgroundColor = SkColorSetRGB(255, 255, 255);
const SkColor kLegacyIconBackgroundColor = SkColorSetRGB(0xf5, 0xf5, 0xf5);
const SkColor kRegularTextColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kDimTextColor = SkColorSetRGB(0x7f, 0x7f, 0x7f);
const SkColor kFocusBorderColor = SkColorSetRGB(64, 128, 250);

// Limits.

gfx::Size GetImageSizeForWidth(int width, const gfx::Size& image_size) {
  const int kNotificationMaximumImageHeight =
      kNotificationWidth * kNotificationPreferredImageRatio;

  gfx::Size size = image_size;
  if (width > 0 && !size.IsEmpty()) {
    double proportion = size.height() / static_cast<double>(size.width());
    size.SetSize(width, std::max(0.5 + width * proportion, 1.0));
    if (size.height() > kNotificationMaximumImageHeight) {
      int height = kNotificationMaximumImageHeight;
      size.SetSize(std::max(0.5 + height / proportion, 1.0), height);
    }
  }
  return size;
}

const size_t kNotificationMaximumItems = 5;

// Timing.
const int kAutocloseDefaultDelaySeconds = 8;
const int kAutocloseHighPriorityDelaySeconds = 25;

// Around notifications ////////////////////////////////////////////////////////

// Pixel dimensions.
const int kMarginBetweenItems = 10;

// Colors.
const SkColor kBackgroundLightColor = SkColorSetRGB(0xf1, 0xf1, 0xf1);
const SkColor kBackgroundDarkColor = SkColorSetRGB(0xe7, 0xe7, 0xe7);
const SkColor kShadowColor = SkColorSetARGB(0.3 * 255, 0, 0, 0);
const SkColor kMessageCenterBackgroundColor = SkColorSetRGB(0xee, 0xee, 0xee);
const SkColor kFooterDelimiterColor = SkColorSetRGB(0xcc, 0xcc, 0xcc);
const SkColor kFooterTextColor = SkColorSetRGB(0x7b, 0x7b, 0x7b);

}  // namespace message_center
