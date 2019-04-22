// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_browser_test.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/wait_for_did_start_navigate.h"
#include "content/public/test/browser_test_utils.h"

namespace chromeos {

using DiscoverModuleSyncFilesTest = DiscoverBrowserTest;

IN_PROC_BROWSER_TEST_F(DiscoverModuleSyncFilesTest, SyncFiles) {
  LoadAndInitializeDiscoverApp(ProfileManager::GetPrimaryUserProfile());

  content::WebContentsAddedObserver observe_new_contents;
  ClickOnCard("discover-sync-files-card");
  // Wait for the first WebContents to be created.
  // We do not expect another one to be created at the same time.
  content::WebContents* new_contents = observe_new_contents.GetWebContents();
  test::WaitForDidStartNavigate(
      new_contents, GURL("https://www.google.com/chromebook/switch/"))
      .Wait();
}

}  // namespace chromeos
