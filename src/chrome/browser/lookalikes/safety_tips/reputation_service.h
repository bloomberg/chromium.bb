// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_REPUTATION_SERVICE_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_REPUTATION_SERVICE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace lookalikes {
struct DomainInfo;
}

namespace safety_tips {

// Callback type used for retrieving reputation status. |ignored| indicates
// whether the user has dismissed the warning and thus should not be warned
// again. |url| is the URL applicable for this result,
using ReputationCheckCallback = base::OnceCallback<
    void(security_state::SafetyTipStatus, bool ignored, const GURL& url)>;

// Provides reputation information on URLs for Safety Tips.
class ReputationService : public KeyedService {
 public:
  explicit ReputationService(Profile* profile);
  ~ReputationService() override;

  static ReputationService* Get(Profile* profile);

  // Calculate the overall reputation status of the given URL, and
  // asynchronously call |callback| with the results. See
  // ReputationCheckCallback above for details on what's returned. |callback|
  // will be called regardless of whether |url| is flagged or
  // not. (Specifically, |callback| will be called with SafetyTipStatus::kNone
  // if the url is not flagged).
  void GetReputationStatus(const GURL& url, ReputationCheckCallback callback);

  // Tells the service that the user has explicitly ignored the warning, and
  // records a histogram.
  // Exposed in subsequent results from GetReputationStatus.
  void SetUserIgnore(content::WebContents* web_contents, const GURL& url);

 private:
  // Returns whether the warning should be shown on the given URL. This is
  // mostly just a helper function to ensure that we always query the allowlist
  // by origin.
  bool IsIgnored(const GURL& url) const;

  // Callback once we have up-to-date |engaged_sites|. Performs checks on the
  // navigated |url|. Displays the warning when needed.
  void GetReputationStatusWithEngagedSites(
      ReputationCheckCallback callback,
      const GURL& url,
      const lookalikes::DomainInfo& navigated_domain,
      const std::vector<lookalikes::DomainInfo>& engaged_sites);

  // Set of origins that we've warned about, and the user has explicitly
  // ignored.  Used to avoid re-warning the user.
  std::set<url::Origin> warning_dismissed_origins_;

  Profile* profile_;

  base::WeakPtrFactory<ReputationService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ReputationService);
};

// Checks SafeBrowsing-style permutations of |url| against the component updater
// blocklist and returns the match type. kNone means the URL is not blocked.
security_state::SafetyTipStatus GetUrlBlockType(const GURL& url);

}  // namespace safety_tips

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_REPUTATION_SERVICE_H_
