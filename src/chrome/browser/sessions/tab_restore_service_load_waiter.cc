// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"

#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/sessions/core/tab_restore_service.h"

// Class used to run a message loop waiting for the TabRestoreService to finish
// loading. Does nothing if the TabRestoreService was already loaded.
TabRestoreServiceLoadWaiter::TabRestoreServiceLoadWaiter(Browser* browser)
    : tab_restore_service_(
          TabRestoreServiceFactory::GetForProfile(browser->profile())),
      do_wait_(!tab_restore_service_->IsLoaded()) {
  if (do_wait_)
    tab_restore_service_->AddObserver(this);
}

TabRestoreServiceLoadWaiter::~TabRestoreServiceLoadWaiter() {
  if (do_wait_)
    tab_restore_service_->RemoveObserver(this);
}

void TabRestoreServiceLoadWaiter::Wait() {
  if (do_wait_)
    run_loop_.Run();
}

void TabRestoreServiceLoadWaiter::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  DCHECK(do_wait_);
  run_loop_.Quit();
}
