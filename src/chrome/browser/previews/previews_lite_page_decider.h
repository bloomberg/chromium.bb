// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_DECIDER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_DECIDER_H_

#include <memory>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle_manager.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "net/http/http_request_headers.h"

class PrefService;

namespace content {
class BrowserContext;
class NavigationHandle;
class NavigationThrottle;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

// This class ensures that the feature is enabled and the
// current Profile is not incognito before handing off the real legwork of the
// triggering decision to |PreviewsLitePageNavigationThrottle|.
class PreviewsLitePageDecider
    : public PreviewsLitePageNavigationThrottleManager,
      public data_reduction_proxy::DataReductionProxySettingsObserver {
 public:
  explicit PreviewsLitePageDecider(content::BrowserContext* browser_context);
  virtual ~PreviewsLitePageDecider();

  // Registers the prefs used in this class.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Checks if the feature is enabled and if so, returns a
  // |PreviewsLitePageNavigationThrottle| that handles the rest of the decision
  // making.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  // Removes |this| as a DataReductionProxySettingsObserver.
  void Shutdown();

  // Sets the internal clock for testing.
  void SetClockForTesting(const base::TickClock* clock);

  // Sets |drp_settings_| for testing and registers |this| as an observer.
  void SetDRPSettingsForTesting(
      data_reduction_proxy::DataReductionProxySettings* drp_settings);

  // Clears all single bypasses for testing.
  void ClearSingleBypassForTesting();

  // Sets that the user has seen the UI notification.
  void SetUserHasSeenUINotification();

 private:
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageDeciderTest, TestServerUnavailable);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageDeciderTest, TestSingleBypass);

  // PreviewsLitePageNavigationThrottleManager:
  void SetServerUnavailableFor(base::TimeDelta retry_after) override;
  bool IsServerUnavailable() override;
  void AddSingleBypass(std::string url) override;
  bool CheckSingleBypass(std::string url) override;
  uint64_t GeneratePageID() override;
  void ReportDataSavings(int64_t network_bytes,
                         int64_t original_bytes,
                         const std::string& host) override;
  bool NeedsToNotifyUser() override;
  void NotifyUser(content::WebContents* web_contents) override;

  // data_reduction_proxy::DataReductionProxySettingsObserver:
  void OnProxyRequestHeadersChanged(
      const net::HttpRequestHeaders& headers) override;
  void OnSettingsInitialized() override;

  // The time after which it is ok to send the server more preview requests.
  base::Optional<base::TimeTicks> retry_at_;

  // A map that tracks the time at which a URL will stop being bypassed.
  std::unordered_map<std::string, base::TimeTicks> single_bypass_;

  // The clock used for getting the current time ticks. Use |SetClockForTesting|
  // in tests.
  const base::TickClock* clock_;

  // The page id to send on requests to the previews server. This is reset to a
  // random value on instantiation and every time the server headers change.
  uint64_t page_id_;

  // A reference to the DRP Settings so that |this| can be removed as an
  // observer on |Shutdown|. Not owned.
  data_reduction_proxy::DataReductionProxySettings* drp_settings_;

  // A reference to the profile's |PrefService|.
  PrefService* pref_service_;

  // Whether the notification infobar needs to be shown to the user in order to
  // use this preview.
  bool need_to_show_notification_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageDecider);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_DECIDER_H_
