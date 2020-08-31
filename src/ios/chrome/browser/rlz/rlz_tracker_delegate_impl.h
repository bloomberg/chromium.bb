// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_RLZ_RLZ_TRACKER_DELEGATE_IMPL_H_
#define IOS_CHROME_BROWSER_RLZ_RLZ_TRACKER_DELEGATE_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "components/rlz/rlz_tracker_delegate.h"

class ChromeBrowserState;
struct OmniboxLog;

// RLZTrackerDelegateImpl implements RLZTrackerDelegate abstract interface
// and provides access to Chrome on iOS features.
class RLZTrackerDelegateImpl : public rlz::RLZTrackerDelegate {
 public:
  RLZTrackerDelegateImpl();
  ~RLZTrackerDelegateImpl() override;

  static bool IsGoogleDefaultSearch(ChromeBrowserState* browser_state);
  static bool IsGoogleHomepage(ChromeBrowserState* browser_state);
  static bool IsGoogleInStartpages(ChromeBrowserState* browser_state);

 private:
  // RLZTrackerDelegate implementation.
  void Cleanup() override;
  bool IsOnUIThread() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool GetBrand(std::string* brand) override;
  bool IsBrandOrganic(const std::string& brand) override;
  bool GetReactivationBrand(std::string* brand) override;
  bool ShouldEnableZeroDelayForTesting() override;
  bool GetLanguage(base::string16* language) override;
  bool GetReferral(base::string16* referral) override;
  bool ClearReferral() override;
  void SetOmniboxSearchCallback(base::OnceClosure callback) override;
  void SetHomepageSearchCallback(base::OnceClosure callback) override;
  bool ShouldUpdateExistingAccessPointRlz() override;

  // Called when user open an URL from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  base::OnceClosure on_omnibox_search_callback_;
  std::unique_ptr<base::CallbackList<void(OmniboxLog*)>::Subscription>
      on_omnibox_url_opened_subscription_;

  DISALLOW_COPY_AND_ASSIGN(RLZTrackerDelegateImpl);
};

#endif  // IOS_CHROME_BROWSER_RLZ_RLZ_TRACKER_DELEGATE_IMPL_H_
