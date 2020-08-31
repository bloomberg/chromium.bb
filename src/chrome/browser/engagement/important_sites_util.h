// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_IMPORTANT_SITES_UTIL_H_
#define CHROME_BROWSER_ENGAGEMENT_IMPORTANT_SITES_UTIL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "url/gurl.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Helper methods for important sites.
// All methods should be used on the UI thread.
class ImportantSitesUtil {
 public:
#if defined(OS_ANDROID)
  static const int kMaxImportantSites = 5;
#else
  static const int kMaxImportantSites = 10;
#endif

  struct ImportantDomainInfo {
    ImportantDomainInfo();
    ~ImportantDomainInfo();
    ImportantDomainInfo(ImportantDomainInfo&&);
    ImportantDomainInfo(const ImportantDomainInfo&) = delete;
    ImportantDomainInfo& operator=(ImportantDomainInfo&&);
    ImportantDomainInfo& operator=(const ImportantDomainInfo&) = delete;
    std::string registerable_domain;
    GURL example_origin;
    double engagement_score = 0;
    int32_t reason_bitfield = 0;
    // |usage| has to be initialized by ImportantSitesUsageCounter before it
    // will contain the number of bytes used for quota and localstorage.
    int64_t usage = 0;
    // Only set if the domain belongs to an installed app.
    base::Optional<std::string> app_name;
  };

  // Do not change the values here, as they are used for UMA histograms.
  enum ImportantReason {
    ENGAGEMENT = 0,
    DURABLE = 1,
    BOOKMARKS = 2,
    HOME_SCREEN = 3,
    NOTIFICATIONS = 4,
    REASON_BOUNDARY
  };

  static std::string GetRegisterableDomainOrIP(const GURL& url);

  static std::string GetRegisterableDomainOrIPFromHost(base::StringPiece host);

  static bool IsDialogDisabled(Profile* profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // This returns the top |<=max_results| important registrable domains. This
  // uses site engagement and notifications to generate the list. |max_results|
  // is assumed to be small.
  // See net/base/registry_controlled_domains/registry_controlled_domain.h for
  // more details on registrable domains and the current list of effective
  // eTLDs.
  static std::vector<ImportantDomainInfo> GetImportantRegisterableDomains(
      Profile* profile,
      size_t max_results);

#if !defined(OS_ANDROID)
  // Return the top |<=max_results| important registrable domains that have an
  // associated installed app. |max_results| is assumed to be small.
  static std::vector<ImportantDomainInfo> GetInstalledRegisterableDomains(
      browsing_data::TimePeriod time_period,
      Profile* profile,
      size_t max_results);
#endif

  // Record the sites that the user chose to blacklist from clearing (in the
  // Clear Browsing Dialog) and the sites they ignored. The blacklisted sites
  // are NOT cleared as they are 'blacklisted' from the clear operation.
  // This records metrics for blacklisted and ignored sites and removes any
  // 'ignored' sites from our important sites list if they were ignored 3 times
  // in a row.
  static void RecordBlacklistedAndIgnoredImportantSites(
      Profile* profile,
      const std::vector<std::string>& blacklisted_sites,
      const std::vector<int32_t>& blacklisted_sites_reason_bitfield,
      const std::vector<std::string>& ignored_sites,
      const std::vector<int32_t>& ignored_sites_reason_bitfield);

  // This marks the given origin as important for testing. Note: This changes
  // the score requirements for the Site Engagement Service, so ONLY call for
  // testing.
  static void MarkOriginAsImportantForTesting(Profile* profile,
                                              const GURL& origin);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ImportantSitesUtil);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_IMPORTANT_SITES_UTIL_H_
