// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_UPDATER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_UPDATER_H_

#include <string>

#include "base/macros.h"

namespace content {
class BrowserContext;
}

// Responsible for handling of Chrome app life-cycle events.
class LauncherAppUpdater {
 public:
  class Delegate {
   public:
    virtual void OnAppInstalled(content::BrowserContext* browser_context,
                                const std::string& app_id) {}
    virtual void OnAppUpdated(content::BrowserContext* browser_context,
                              const std::string& app_id) {}
    virtual void OnAppUninstalledPrepared(
        content::BrowserContext* browser_context,
        const std::string& app_id) {}
    virtual void OnAppUninstalled(content::BrowserContext* browser_context,
                                  const std::string& app_id) {}

   protected:
    virtual ~Delegate() {}
  };

  virtual ~LauncherAppUpdater();

  Delegate* delegate() { return delegate_; }

  content::BrowserContext* browser_context() { return browser_context_; }

 protected:
  LauncherAppUpdater(Delegate* delegate,
                     content::BrowserContext* browser_context);

 private:
  // Unowned pointers
  Delegate* delegate_;
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(LauncherAppUpdater);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_UPDATER_H_
