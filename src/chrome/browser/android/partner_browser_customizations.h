// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PARTNER_BROWSER_CUSTOMIZATIONS_H_
#define CHROME_BROWSER_ANDROID_PARTNER_BROWSER_CUSTOMIZATIONS_H_

#include "base/macros.h"

namespace chrome {
namespace android {

class PartnerBrowserCustomizations {
 public:
  // Whether incognito mode is disabled by the partner.
  static bool IsIncognitoDisabled();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PartnerBrowserCustomizations);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_PARTNER_BROWSER_CUSTOMIZATIONS_H_
