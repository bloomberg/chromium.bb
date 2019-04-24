// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_logging_metrics.h"

#include <cmath>
#include <string>
#include <type_traits>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "ui/base/mojo/window_open_disposition.mojom.h"

namespace feed {

namespace {

// The constant integers(bucket sizes) and strings(UMA names) in this file need
// matching with Zine's in the file
// components/ntp_snippets/content_suggestions_metrics.cc. The purpose to have
// identical bucket sizes and names with Zine is for comparing Feed with Zine
// easily. After Zine is deprecated, we can change the values if we needed.

// Constants used as max sample sizes for histograms.
const int kMaxContentCount = 50;
const int kMaxFailureCount = 10;
const int kMaxSuggestionsTotal = 50;
const int kMaxTokenCount = 10;

// Keep in sync with MAX_SUGGESTIONS_PER_SECTION in NewTabPageUma.java.
const int kMaxSuggestionsForArticle = 20;

const char kHistogramArticlesUsageTimeLocal[] =
    "NewTabPage.ContentSuggestions.UsageTimeLocal";

// Values correspond to
// third_party/feed/src/src/main/java/com/google/android/libraries/feed/host/
// logging/SpinnerType.java, enums.xml and histograms.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SpinnerType {
  KInitialLoad = 1,
  KZeroStateRefresh = 2,
  KMoreButton = 3,
  KSyntheticToken = 4,
  KInfiniteFeed = 5,
  kMaxValue = KInfiniteFeed
};

// Each suffix here should correspond to an entry under histogram suffix
// ContentSuggestionCategory in histograms.xml.
std::string GetSpinnerTypeSuffix(SpinnerType spinner_type) {
  switch (spinner_type) {
    case SpinnerType::KInitialLoad:
      return "InitialLoad";
    case SpinnerType::KZeroStateRefresh:
      return "ZeroStateRefresh";
    case SpinnerType::KMoreButton:
      return "MoreButton";
    case SpinnerType::KSyntheticToken:
      return "SyntheticToken";
    case SpinnerType::KInfiniteFeed:
      return "InfiniteFeed";
  }

  // TODO(https://crbug.com/935602): Handle new values when adding new values on
  // java side.
  NOTREACHED();
  return std::string();
}

// Records ContentSuggestions usage. Therefore the day is sliced into 20min
// buckets. Depending on the current local time the count of the corresponding
// bucket is increased.
void RecordContentSuggestionsUsage(base::Time now) {
  const int kBucketSizeMins = 20;
  const int kNumBuckets = 24 * 60 / kBucketSizeMins;

  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);
  int bucket = (now_exploded.hour * 60 + now_exploded.minute) / kBucketSizeMins;

  const char* kWeekdayNames[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};
  std::string histogram_name(
      base::StringPrintf("%s.%s", kHistogramArticlesUsageTimeLocal,
                         kWeekdayNames[now_exploded.day_of_week]));
  // Since the |histogram_name| is dynamic, we can't use the regular macro.
  base::UmaHistogramExactLinear(histogram_name, bucket, kNumBuckets);
  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramArticlesUsageTimeLocal, bucket,
                             kNumBuckets);

  base::RecordAction(
      base::UserMetricsAction("NewTabPage_ContentSuggestions_ArticlesUsage"));
}

int ToUMAScore(float score) {
  // Scores are typically reported in a range of (0,1]. As UMA does not support
  // floats, we put them on a discrete scale of [1,10]. We keep the extra bucket
  // 11 for unexpected over-flows as we want to distinguish them from scores
  // close to 1. For instance, the discrete value 1 represents score values
  // within (0.0, 0.1].
  return ceil(score * 10);
}

void RecordSuggestionPageVisited(bool return_to_ntp) {
  if (return_to_ntp) {
    base::RecordAction(
        base::UserMetricsAction("MobileNTP.Snippets.VisitEndBackInNTP"));
  }
  base::RecordAction(base::UserMetricsAction("MobileNTP.Snippets.VisitEnd"));
}

void RecordUndoableActionUMA(const std::string& histogram_base,
                             int position,
                             bool committed) {
  std::string histogram_name =
      histogram_base + (committed ? ".Commit" : ".Undo");

  // Since the |histogram_name| is dynamic, we can't use the regular macro.
  base::UmaHistogramExactLinear(histogram_name, position, kMaxSuggestionsTotal);
}

void CheckURLVisitedDone(int position, bool committed, bool visited) {
  if (visited) {
    RecordUndoableActionUMA("NewTabPage.ContentSuggestions.DismissedVisited",
                            position, committed);
  } else {
    RecordUndoableActionUMA("NewTabPage.ContentSuggestions.DismissedUnvisited",
                            position, committed);
  }
}

void RecordSpinnerTimeUMA(const char* base_name,
                          base::TimeDelta time,
                          int spinner_type) {
  SpinnerType type = static_cast<SpinnerType>(spinner_type);
  std::string suffix = GetSpinnerTypeSuffix(type);
  std::string histogram_name(
      base::StringPrintf("%s.%s", base_name, suffix.c_str()));
  base::UmaHistogramTimes(histogram_name, time);
  base::UmaHistogramTimes(base_name, time);
}

}  // namespace

FeedLoggingMetrics::FeedLoggingMetrics(
    HistoryURLCheckCallback history_url_check_callback,
    base::Clock* clock)
    : history_url_check_callback_(std::move(history_url_check_callback)),
      clock_(clock),
      weak_ptr_factory_(this) {}

FeedLoggingMetrics::~FeedLoggingMetrics() = default;

void FeedLoggingMetrics::OnPageShown(const int suggestions_count) {
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible",
      suggestions_count, kMaxSuggestionsTotal);
}

void FeedLoggingMetrics::OnPagePopulated(base::TimeDelta timeToPopulate) {
  UMA_HISTOGRAM_MEDIUM_TIMES("ContentSuggestions.Feed.PagePopulatingTime",
                             timeToPopulate);
}

void FeedLoggingMetrics::OnSuggestionShown(int position,
                                           base::Time publish_date,
                                           float score,
                                           base::Time fetch_date) {
  UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.Shown", position,
                             kMaxSuggestionsTotal);

  base::TimeDelta age = clock_->Now() - publish_date;
  UMA_HISTOGRAM_CUSTOM_TIMES("NewTabPage.ContentSuggestions.ShownAge.Articles",
                             age, base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromDays(7), 100);

  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.ShownScoreNormalized.Articles",
      ToUMAScore(score), 11);

  // Records the time since the fetch time of the displayed snippet.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "NewTabPage.ContentSuggestions.TimeSinceSuggestionFetched",
      clock_->Now() - fetch_date, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(7),
      /*bucket_count=*/100);

  // When the first of the articles suggestions is shown, then we count this as
  // a single usage of content suggestions.
  if (position == 0) {
    RecordContentSuggestionsUsage(clock_->Now());
  }
}

void FeedLoggingMetrics::OnSuggestionOpened(int position,
                                            base::Time publish_date,
                                            float score) {
  UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.Opened", position,
                             kMaxSuggestionsTotal);

  base::TimeDelta age = clock_->Now() - publish_date;
  UMA_HISTOGRAM_CUSTOM_TIMES("NewTabPage.ContentSuggestions.OpenedAge.Articles",
                             age, base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromDays(7), 100);

  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.OpenedScoreNormalized.Articles",
      ToUMAScore(score), 11);

  RecordContentSuggestionsUsage(clock_->Now());

  base::RecordAction(base::UserMetricsAction("Suggestions.Content.Opened"));
}

void FeedLoggingMetrics::OnSuggestionWindowOpened(
    WindowOpenDisposition disposition) {
  // We use WindowOpenDisposition::MAX_VALUE + 1 for |value_max| since MAX_VALUE
  // itself is a valid (and used) enum value.
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.OpenDisposition.Articles",
      static_cast<int>(disposition),
      static_cast<int>(WindowOpenDisposition::MAX_VALUE) + 1);

  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    base::RecordAction(base::UserMetricsAction("Suggestions.Card.Tapped"));
  }
}

void FeedLoggingMetrics::OnSuggestionMenuOpened(int position,
                                                base::Time publish_date,
                                                float score) {
  UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.MenuOpened",
                             position, kMaxSuggestionsTotal);

  base::TimeDelta age = clock_->Now() - publish_date;
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "NewTabPage.ContentSuggestions.MenuOpenedAge.Articles", age,
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(7), 100);

  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.MenuOpenedScoreNormalized.Articles",
      ToUMAScore(score), 11);
}

void FeedLoggingMetrics::OnSuggestionDismissed(int position,
                                               const GURL& url,
                                               bool committed) {
  history_url_check_callback_.Run(
      url, base::BindOnce(&CheckURLVisitedDone, position, committed));

  base::RecordAction(base::UserMetricsAction("Suggestions.Content.Dismissed"));
}

void FeedLoggingMetrics::OnSuggestionSwiped() {
  base::RecordAction(base::UserMetricsAction("Suggestions.Card.SwipedAway"));
}

void FeedLoggingMetrics::OnSuggestionArticleVisited(base::TimeDelta visit_time,
                                                    bool return_to_ntp) {
  base::UmaHistogramLongTimes(
      "NewTabPage.ContentSuggestions.VisitDuration.Articles", visit_time);
  RecordSuggestionPageVisited(return_to_ntp);
}

void FeedLoggingMetrics::OnSuggestionOfflinePageVisited(
    base::TimeDelta visit_time,
    bool return_to_ntp) {
  base::UmaHistogramLongTimes(
      "NewTabPage.ContentSuggestions.VisitDuration.Downloads", visit_time);
  RecordSuggestionPageVisited(return_to_ntp);
}

void FeedLoggingMetrics::OnMoreButtonShown(int position) {
  // The "more" card can appear in addition to the actual suggestions, so add
  // one extra bucket to this histogram.
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.MoreButtonShown.Articles", position,
      kMaxSuggestionsForArticle + 1);
}

void FeedLoggingMetrics::OnMoreButtonClicked(int position) {
  // The "more" card can appear in addition to the actual suggestions, so add
  // one extra bucket to this histogram.
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.MoreButtonClicked.Articles", position,
      kMaxSuggestionsForArticle + 1);
}

void FeedLoggingMetrics::OnNotInterestedInSource(int position, bool committed) {
  RecordUndoableActionUMA(
      "ContentSuggestions.Feed.InterestHeader.NotInterestedInSource", position,
      committed);
}

void FeedLoggingMetrics::OnNotInterestedInTopic(int position, bool committed) {
  RecordUndoableActionUMA(
      "ContentSuggestions.Feed.InterestHeader.NotInterestedInTopic", position,
      committed);
}

void FeedLoggingMetrics::OnSpinnerStarted(int spinner_type) {
  // TODO(https://crbug.com/935602): Handle new values when adding new values on
  // java side.
  SpinnerType type = static_cast<SpinnerType>(spinner_type);
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.FetchPendingSpinner.Shown",
                            type);
}

void FeedLoggingMetrics::OnSpinnerFinished(base::TimeDelta shown_time,
                                           int spinner_type) {
  RecordSpinnerTimeUMA(
      "ContentSuggestions.Feed.FetchPendingSpinner.VisibleDuration", shown_time,
      spinner_type);
}

void FeedLoggingMetrics::OnSpinnerDestroyedWithoutCompleting(
    base::TimeDelta shown_time,
    int spinner_type) {
  RecordSpinnerTimeUMA(
      "ContentSuggestions.Feed.FetchPendingSpinner."
      "VisibleDurationWithoutCompleting",
      shown_time, spinner_type);
}

void FeedLoggingMetrics::OnPietFrameRenderingEvent(
    std::vector<int> piet_error_codes) {
  for (auto error_code : piet_error_codes) {
    base::UmaHistogramSparse(
        "ContentSuggestions.Feed.Piet.FrameRenderingErrorCode", error_code);
  }
}

void FeedLoggingMetrics::OnInternalError(int internal_error) {
  // TODO(https://crbug.com/935602): The max value here is fragile, figure out
  // some way to test the @IntDef size.
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.InternalError",
                            internal_error, 10);
}

void FeedLoggingMetrics::OnTokenCompleted(bool was_synthetic,
                                          int content_count,
                                          int token_count) {
  if (was_synthetic) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenCompleted.ContentCount2.Synthetic",
        content_count, kMaxContentCount);
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenCompleted.TokenCount.Synthetic",
        token_count, kMaxTokenCount);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenCompleted.ContentCount2.NotSynthetic",
        content_count, kMaxContentCount);
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenCompleted.TokenCount.NotSynthetic",
        token_count, kMaxTokenCount);
  }
}

void FeedLoggingMetrics::OnTokenFailedToComplete(bool was_synthetic,
                                                 int failure_count) {
  if (was_synthetic) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenFailedToCompleted.Synthetic",
        failure_count, kMaxFailureCount);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenFailedToCompleted.NotSynthetic",
        failure_count, kMaxFailureCount);
  }
}

void FeedLoggingMetrics::OnServerRequest(int request_reason) {
  // TODO(https://crbug.com/935602): The max value here is fragile, figure out
  // some way to test the @IntDef size.
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.ServerRequest.Reason",
                            request_reason, 8);
}

void FeedLoggingMetrics::OnZeroStateShown(int zero_state_show_reason) {
  // TODO(https://crbug.com/935602): The max value here is fragile, figure out
  // some way to test the @IntDef size.
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.ZeroStateShown.Reason",
                            zero_state_show_reason, 3);
}

void FeedLoggingMetrics::OnZeroStateRefreshCompleted(int new_content_count,
                                                     int new_token_count) {
  UMA_HISTOGRAM_EXACT_LINEAR(
      "ContentSuggestions.Feed.ZeroStateRefreshCompleted.ContentCount",
      new_content_count, kMaxContentCount);
  UMA_HISTOGRAM_EXACT_LINEAR(
      "ContentSuggestions.Feed.ZeroStateRefreshCompleted.TokenCount",
      new_token_count, kMaxTokenCount);
}

void FeedLoggingMetrics::ReportScrolledAfterOpen() {
  base::RecordAction(base::UserMetricsAction("Suggestions.ScrolledAfterOpen"));
}

}  // namespace feed
