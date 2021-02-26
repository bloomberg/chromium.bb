// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_tab_helper.h"

#include "chrome/browser/prefetch/no_state_prefetch/prerender_manager_factory.h"
#include "components/no_state_prefetch/browser/prerender_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace prerender {

NoStatePrefetchTabHelper::NoStatePrefetchTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

NoStatePrefetchTabHelper::~NoStatePrefetchTabHelper() = default;

void NoStatePrefetchTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return;
  }

  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (!prerender_manager)
    return;
  if (prerender_manager->IsWebContentsPrerendering(web_contents()))
    return;
  prerender_manager->RecordNavigation(navigation_handle->GetURL());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NoStatePrefetchTabHelper)

}  // namespace prerender
