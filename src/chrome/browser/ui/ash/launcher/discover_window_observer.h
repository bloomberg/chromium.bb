// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_DISCOVER_WINDOW_OBSERVER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_DISCOVER_WINDOW_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_window_manager_observer.h"
#include "ui/aura/window_tracker.h"

// Sets the window title and shelf item properties for Discover windows.
class DiscoverWindowObserver : public chromeos::DiscoverWindowManagerObserver {
 public:
  DiscoverWindowObserver();
  ~DiscoverWindowObserver() override;

  // chromeos::DiscoverWindowManagerObserver:
  void OnNewDiscoverWindow(Browser* discover_browser) override;

 private:
  // Set of windows whose title is forced to 'Settings.'
  std::unique_ptr<aura::WindowTracker> aura_window_tracker_;

  DISALLOW_COPY_AND_ASSIGN(DiscoverWindowObserver);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_DISCOVER_WINDOW_OBSERVER_H_
