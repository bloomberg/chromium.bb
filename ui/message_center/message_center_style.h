// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_export.h"

namespace message_center {

// Exported values /////////////////////////////////////////////////////////////

// Square image sizes in DIPs.
MESSAGE_CENTER_EXPORT extern const int kNotificationButtonIconSize;
MESSAGE_CENTER_EXPORT extern const int kNotificationIconSize;
MESSAGE_CENTER_EXPORT extern const int kNotificationPreferredImageSize;
MESSAGE_CENTER_EXPORT extern const float kNotificationPreferredImageRatio;
MESSAGE_CENTER_EXPORT extern const int kSettingsIconSize;

// Limits.
MESSAGE_CENTER_EXPORT extern const size_t kMaxVisiblePopupNotifications;
MESSAGE_CENTER_EXPORT extern const size_t kMaxVisibleMessageCenterNotifications;

// DIP dimension
MESSAGE_CENTER_EXPORT extern const int kNotificationWidth;  // H size of the
                                                            // whole card.

// Colors.
MESSAGE_CENTER_EXPORT extern const SkColor kMessageCenterBorderColor;
MESSAGE_CENTER_EXPORT extern const SkColor kMessageCenterShadowColor;

// Within a notification ///////////////////////////////////////////////////////

// DIP dimensions (H = horizontal, V = vertical).
extern const int kControlButtonSize;  // Square size of close & expand buttons.
extern const int kIconToTextPadding;  // H space between icon & title/message.
extern const int kTextTopPadding;     // V space between text elements.
extern const int kIconBottomPadding;  // Minimum non-zero V space between icon
                                      // and frame.

// Text sizes.
extern const int kTitleFontSize;             // For title only.
extern const int kTitleLineHeight;           // In DIPs.
extern const int kMessageFontSize;           // For everything but title.
extern const int kMessageLineHeight;         // In DIPs.

// Colors.
extern const SkColor kNotificationBackgroundColor; // Background of the card.
extern const SkColor kLegacyIconBackgroundColor;   // Used behind icons smaller.
                                                   // than the icon view.
extern const SkColor kRegularTextColor;            // Title, message, ...
extern const SkColor kDimTextColor;
extern const SkColor kFocusBorderColor;  // The focus border.

// Limits.

// Given the size of an image, returns the rect the image should be displayed
// in, centered.
gfx::Size GetImageSizeForWidth(int width, const gfx::Size& image_size);

extern const int kNotificationMaximumImageHeight;  // For image notifications.
extern const size_t kNotificationMaximumItems;     // For list notifications.

// Timing.
extern const int kAutocloseDefaultDelaySeconds;
extern const int kAutocloseHighPriorityDelaySeconds;

// Buttons.
const int kButtonHeight = 38;              // In DIPs.
const int kButtonHorizontalPadding = 16;   // In DIPs.
const int kButtonIconTopPadding = 11;      // In DIPs.
const int kButtonIconToTitlePadding = 16;  // In DIPs.

#if !defined(OS_LINUX) || defined(USE_AURA)
const SkColor kButtonSeparatorColor = SkColorSetRGB(234, 234, 234);
const SkColor kHoveredButtonBackgroundColor = SkColorSetRGB(243, 243, 243);
#endif

// Progress bar.
const int kProgressBarThickness = 5;
const int kProgressBarTopPadding = 16;
const int kProgressBarCornerRadius = 3;
const SkColor kProgressBarBackgroundColor = SkColorSetRGB(216, 216, 216);
const SkColor kProgressBarSliceColor = SkColorSetRGB(120, 120, 120);

// Line limits.
const int kTitleLineLimit = 3;
const int kMessageCollapsedLineLimit = 3;
const int kMessageExpandedLineLimit = 7;
const int kContextMessageLineLimit = 1;

// Around notifications ////////////////////////////////////////////////////////

// DIP dimensions (H = horizontal, V = vertical).
MESSAGE_CENTER_EXPORT extern const int kMarginBetweenItems; // H & V space
                                                            // around & between
                                                            // notifications.

// Colors.
extern const SkColor kBackgroundLightColor;  // Behind notifications, gradient
extern const SkColor kBackgroundDarkColor;   // from light to dark.

extern const SkColor kShadowColor;           // Shadow in the tray.

extern const SkColor kMessageCenterBackgroundColor;
extern const SkColor kFooterDelimiterColor;  // Separator color for the tray.
extern const SkColor kFooterTextColor;       // Text color for tray labels.

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
