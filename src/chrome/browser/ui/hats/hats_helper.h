// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_HELPER_H_
#define CHROME_BROWSER_UI_HATS_HATS_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class Profile;

// This is a browser side per tab helper that allows an entry trigger to
// launch Happiness Tracking Surveys (HaTS)
class HatsHelper : public content::WebContentsObserver,
                   public content::WebContentsUserData<HatsHelper> {
 public:
  HatsHelper(const HatsHelper&) = delete;
  HatsHelper& operator=(const HatsHelper&) = delete;

  ~HatsHelper() override;

 private:
  friend class content::WebContentsUserData<HatsHelper>;

  explicit HatsHelper(content::WebContents* web_contents);

  // contents::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  Profile* profile() const;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_HELPER_H_
