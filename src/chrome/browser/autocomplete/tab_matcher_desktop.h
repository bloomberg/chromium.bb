// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_TAB_MATCHER_DESKTOP_H_
#define CHROME_BROWSER_AUTOCOMPLETE_TAB_MATCHER_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "content/public/browser/web_contents.h"

// Implementation of TabMatcher shared across all desktop platforms.
class TabMatcherDesktop : public TabMatcher {
 public:
  TabMatcherDesktop(const AutocompleteProviderClient& client, Profile* profile)
      : client_{client}, profile_{profile} {}

  bool IsTabOpenWithURL(const GURL& gurl,
                        const AutocompleteInput* input) const override;

 private:
  bool IsStrippedURLEqualToWebContentsURL(
      const GURL& stripped_url,
      content::WebContents* web_contents) const;

  const AutocompleteProviderClient& client_;
  raw_ptr<Profile> profile_{};
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_TAB_MATCHER_DESKTOP_H_
