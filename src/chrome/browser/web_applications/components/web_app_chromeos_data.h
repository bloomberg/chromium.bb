// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_CHROMEOS_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_CHROMEOS_DATA_H_

#include <iosfwd>

namespace web_app {

struct WebAppChromeOsData {
  // By default an app is shown everywhere.
  bool show_in_launcher = true;
  bool show_in_search = true;
  bool show_in_management = true;
  // By default the app is not disabled. Disabling the app means having a
  // blocked logo on top of the app icon, and the user can't launch the app.
  bool is_disabled = false;
};

// For logging and debugging purposes.
std::ostream& operator<<(std::ostream& out, const WebAppChromeOsData& data);

bool operator==(const WebAppChromeOsData& chromeos_data1,
                const WebAppChromeOsData& chromeos_data2);
bool operator!=(const WebAppChromeOsData& chromeos_data1,
                const WebAppChromeOsData& chromeos_data2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_CHROMEOS_DATA_H_
