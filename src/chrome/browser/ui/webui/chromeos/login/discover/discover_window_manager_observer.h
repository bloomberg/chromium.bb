// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_WINDOW_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_WINDOW_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"

class Browser;

namespace chromeos {

class DiscoverWindowManagerObserver : public base::CheckedObserver {
 public:
  // Called when a new Discover App browser window is created.
  virtual void OnNewDiscoverWindow(Browser* discover_browser) = 0;

 protected:
  ~DiscoverWindowManagerObserver() override = default;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_WINDOW_MANAGER_OBSERVER_H_
