// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/wait_for_did_start_navigate.h"

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_window_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test_utils.h"

namespace chromeos {

namespace test {

WaitForDidStartNavigate::WaitForDidStartNavigate(
    content::WebContents* web_contents,
    const GURL& url)
    : content::WebContentsObserver(web_contents), url_(url) {}

void WaitForDidStartNavigate::Wait() {
  // Run() will return immediately if Quit() has been called before.
  run_loop_.Run();
}

void WaitForDidStartNavigate::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetURL() == url_)
    run_loop_.Quit();
}

}  // namespace test
}  // namespace chromeos
