// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_LOCALIZED_STRINGS_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_LOCALIZED_STRINGS_H_

namespace content {
class WebUIDataSource;
}

namespace chromeos {
namespace network_health {

// Adds the strings needed for network health elements to |html_source|.
void AddLocalizedStrings(content::WebUIDataSource* html_source);

}  // namespace network_health
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_NETWORK_HEALTH_LOCALIZED_STRINGS_H_
