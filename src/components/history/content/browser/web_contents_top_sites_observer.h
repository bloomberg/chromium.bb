// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CONTENT_BROWSER_WEB_CONTENTS_TOP_SITES_OBSERVER_H_
#define COMPONENTS_HISTORY_CONTENT_BROWSER_WEB_CONTENTS_TOP_SITES_OBSERVER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace history {

class TopSites;

// WebContentsTopSitesObserver forwards navigation events from
// content::WebContents to TopSites.
class WebContentsTopSitesObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebContentsTopSitesObserver> {
 public:
  ~WebContentsTopSitesObserver() override;

  static void CreateForWebContents(content::WebContents* web_contents,
                                   TopSites* top_sites);

 private:
  friend class content::WebContentsUserData<WebContentsTopSitesObserver>;

  WebContentsTopSitesObserver(content::WebContents* web_contents,
                              TopSites* top_sites);

  // content::WebContentsObserver implementation.
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  // Underlying TopSites instance, may be null during testing.
  TopSites* top_sites_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(WebContentsTopSitesObserver);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CONTENT_BROWSER_WEB_CONTENTS_TOP_SITES_OBSERVER_H_
