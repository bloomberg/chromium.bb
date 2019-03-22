// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_LOGGING_METRICS_H_
#define COMPONENTS_FEED_CORE_FEED_LOGGING_METRICS_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace base {
class Clock;
class Time;
class TimeDelta;
}  // namespace base

enum class WindowOpenDisposition;

namespace feed {

// FeedLoggingMetrics is a central place to report all the UMA metrics for Feed.
class FeedLoggingMetrics {
 public:
  // Return whether the URL is visited when calling checking URL visited.
  using CheckURLVisitCallback = base::OnceCallback<void(bool)>;

  // Calling this callback when need to check whether url is visited, and will
  // tell CheckURLVisitCallback the result.
  using HistoryURLCheckCallback =
      base::RepeatingCallback<void(const GURL&, CheckURLVisitCallback)>;

  explicit FeedLoggingMetrics(HistoryURLCheckCallback callback,
                              base::Clock* clock);
  ~FeedLoggingMetrics();

  // |suggestions_count| contains how many cards show to users. It does not
  // depend on whether the user actually saw the cards.
  void OnPageShown(const int suggestions_count);

  // Should only be called once per NTP for each suggestion.
  void OnSuggestionShown(int position,
                         base::Time publish_date,
                         float score,
                         base::Time fetch_date);

  void OnSuggestionOpened(int position, base::Time publish_date, float score);

  void OnSuggestionWindowOpened(WindowOpenDisposition disposition);

  void OnSuggestionMenuOpened(int position,
                              base::Time publish_date,
                              float score);

  void OnSuggestionDismissed(int position, const GURL& url);

  void OnSuggestionSwiped();

  void OnSuggestionArticleVisited(base::TimeDelta visit_time,
                                  bool return_to_ntp);

  void OnSuggestionOfflinePageVisited(base::TimeDelta visit_time,
                                      bool return_to_ntp);

  // Should only be called once per NTP for each "more" button.
  void OnMoreButtonShown(int position);

  void OnMoreButtonClicked(int position);

  void OnSpinnerShown(base::TimeDelta shown_time);

  void ReportScrolledAfterOpen();

 private:
  void CheckURLVisitedDone(int position, bool visited);

  const HistoryURLCheckCallback history_url_check_callback_;

  // Used to access current time, injected for testing.
  base::Clock* clock_;

  base::WeakPtrFactory<FeedLoggingMetrics> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedLoggingMetrics);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_LOGGING_METRICS_H_
