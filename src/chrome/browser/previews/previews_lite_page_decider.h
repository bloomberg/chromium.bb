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

namespace content {
class NavigationHandle;
class NavigationThrottle;
}  // namespace content

// This class ensures that the feature is enabled and the
// current Profile is not incognito before handing off the real legwork of the
// triggering decision to |PreviewsLitePageNavigationThrottle|.
class PreviewsLitePageDecider
    : public PreviewsLitePageNavigationThrottleManager {
 public:
  PreviewsLitePageDecider();
  virtual ~PreviewsLitePageDecider();

  // Checks if the feature is enabled and if so, returns a
  // |PreviewsLitePageNavigationThrottle| that handles the rest of the decision
  // making.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  // Sets the internal clock for testing.
  void SetClockForTesting(const base::TickClock* clock);

 protected:
  // Virtual for testing.
  virtual bool IsDataSaverEnabled(content::NavigationHandle* handle) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageDeciderTest, TestServerUnavailable);
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageDeciderTest, TestSingleBypass);

  // PreviewsLitePageNavigationThrottleManager:
  void SetServerUnavailableFor(base::TimeDelta retry_after) override;
  bool IsServerUnavailable() override;
  void AddSingleBypass(std::string url) override;
  bool CheckSingleBypass(std::string url) override;

  // The time after which it is ok to send the server more preview requests.
  base::Optional<base::TimeTicks> retry_at_;

  // A map that tracks the time at which a URL will stop being bypassed.
  std::unordered_map<std::string, base::TimeTicks> single_bypass_;

  // The clock used for getting the current time ticks. Use |SetClockForTesting|
  // in tests.
  const base::TickClock* clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageDecider);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_DECIDER_H_
