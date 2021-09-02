// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_

#include <string>

#include "url/gurl.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace send_tab_to_self {

enum SendTabToSelfMenuType { kTab, kOmnibox, kContent, kLink };

// Adds a new entry to SendTabToSelfModel when user clicks a target device. Will
// not show a confirmation notification if |show_notification| is false.
void CreateNewEntry(content::WebContents* tab,
                    const std::string& target_device_name,
                    const std::string& target_device_guid,
                    const GURL& link_url = GURL());

// Adds a new entry to SendTabToSelfModel when user clicks the single valid
// device. Will be called when GetValidDeviceCount() == 1.
void ShareToSingleTarget(content::WebContents* tab,
                         const GURL& link_url = GURL());

// Gets the count of valid device number.
size_t GetValidDeviceCount(Profile* profile);

// Gets the name of the single valid device. Will be called when
// GetValidDeviceCount() == 1.
std::u16string GetSingleTargetDeviceName(Profile* profile);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DESKTOP_UTIL_H_
