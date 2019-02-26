// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/system/sys_info.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

content::RenderFrameHost* GetMainFrame(content::RenderFrameHost* rfh) {
  // Don't use rfh->GetRenderViewHost()->GetMainFrame() here because
  // RenderViewHost is being deprecated and because in OOPIF,
  // RenderViewHost::GetMainFrame() returns nullptr for child frames hosted in a
  // different process from the main frame.
  while (rfh->GetParent() != nullptr)
    rfh = rfh->GetParent();
  return rfh;
}

}  // namespace

struct NavigationPredictor::NavigationScore {
  NavigationScore(const GURL& url,
                  size_t area_rank,
                  double score,
                  bool contains_image)
      : url(url),
        area_rank(area_rank),
        score(score),
        contains_image(contains_image) {}
  // URL of the target link.
  const GURL url;

  // Rank in terms of anchor element area. It starts at 0, a lower rank implies
  // a larger area.
  const size_t area_rank;

  // Calculated navigation score, based on |area_rank| and other metrics.
  double score;

  // Multiple anchor elements may point to the same |url|. |contains_image| is
  // true if at least one of the anchor elements pointing to |url| contains an
  // image.
  const bool contains_image;

  // Rank of the |score| in this document. It starts at 0, a lower rank implies
  // a higher |score|.
  base::Optional<size_t> score_rank;
};

NavigationPredictor::NavigationPredictor(
    content::RenderFrameHost* render_frame_host)
    : browser_context_(
          render_frame_host->GetSiteInstance()->GetBrowserContext()),
      ratio_area_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "ratio_area_scale",
          100)),
      is_in_iframe_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "is_in_iframe_scale",
          0)),
      is_same_host_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "is_same_host_scale",
          100)),
      contains_image_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "contains_image_scale",
          50)),
      is_url_incremented_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "is_url_incremented_scale",
          100)),
      source_engagement_score_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "source_engagement_score_scale",
          100)),
      target_engagement_score_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "target_engagement_score_scale",
          100)),
      area_rank_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "area_rank_scale",
          100)),
      sum_scales_(ratio_area_scale_ + is_in_iframe_scale_ +
                  is_same_host_scale_ + contains_image_scale_ +
                  is_url_incremented_scale_ + source_engagement_score_scale_ +
                  target_engagement_score_scale_ + area_rank_scale_),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      prefetch_url_score_threshold_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "prefetch_url_score_threshold",
          0)),
      preconnect_origin_score_threshold_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "preconnect_origin_score_threshold",
          0)),
      same_origin_preconnecting_allowed_(
          base::GetFieldTrialParamByFeatureAsBool(
              blink::features::kRecordAnchorMetricsVisible,
              "same_origin_preconnecting_allowed",
              false))
#ifdef OS_ANDROID
      ,
      application_status_listener_(
          base::android::ApplicationStatusListener::New(base::BindRepeating(
              &NavigationPredictor::OnApplicationStateChange,
              // It's safe to use base::Unretained here since the application
              // state listener is owned by |this|. So, no callbacks can
              // arrive after |this| has been destroyed.
              base::Unretained(this)))),
      application_state_(base::android::ApplicationStatusListener::GetState())
#endif  // OS_ANDROID
{
  DCHECK(browser_context_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK_LE(0, preconnect_origin_score_threshold_);
  DCHECK_LE(0, prefetch_url_score_threshold_);

  if (render_frame_host != GetMainFrame(render_frame_host))
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  current_visibility_ = web_contents->GetVisibility();
  Observe(web_contents);
}

NavigationPredictor::~NavigationPredictor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Observe(nullptr);
}

void NavigationPredictor::Create(
    blink::mojom::AnchorElementMetricsHostRequest request,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(base::FeatureList::IsEnabled(
      blink::features::kRecordAnchorMetricsClicked));
  DCHECK(base::FeatureList::IsEnabled(
      blink::features::kRecordAnchorMetricsVisible));

  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  mojo::MakeStrongBinding(
      std::make_unique<NavigationPredictor>(render_frame_host),
      std::move(request));
}

bool NavigationPredictor::IsValidMetricFromRenderer(
    const blink::mojom::AnchorElementMetrics& metric) const {
  return metric.target_url.SchemeIsHTTPOrHTTPS() &&
         metric.source_url.SchemeIsHTTPOrHTTPS();
}


void NavigationPredictor::RecordTimingOnClick() {
  base::TimeTicks current_timing = base::TimeTicks::Now();

  // This is the first click in the document.
  // Note that multiple clicks can happen on the same document. For example,
  // if the click opens a new tab, then the old document is not necessarily
  // destroyed. The user can return to the old document and click.
  if (last_click_timing_ == base::TimeTicks()) {
    // Document may have not loaded yet when click happens.
    UMA_HISTOGRAM_TIMES("AnchorElementMetrics.Clicked.DurationLoadToFirstClick",
                        document_loaded_timing_ > base::TimeTicks()
                            ? current_timing - document_loaded_timing_
                            : base::TimeDelta());
  } else {
    UMA_HISTOGRAM_TIMES("AnchorElementMetrics.Clicked.ClickIntervals",
                        current_timing - last_click_timing_);
  }
  last_click_timing_ = current_timing;
}

void NavigationPredictor::RecordActionAccuracyOnClick(
    const GURL& target_url) const {
  static constexpr char histogram_name_dse[] =
      "NavigationPredictor.OnDSE.AccuracyActionTaken";
  static constexpr char histogram_name_non_dse[] =
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken";

  if (!prefetch_url_ && !preconnect_origin_) {
    base::UmaHistogramEnumeration(source_is_default_search_engine_page_
                                      ? histogram_name_dse
                                      : histogram_name_non_dse,
                                  ActionAccuracy::kNoActionTakenClickHappened);
    return;
  }

  // Exactly one action must have been taken.
  DCHECK(prefetch_url_.has_value() != preconnect_origin_.has_value());

  if (preconnect_origin_) {
    if (url::Origin::Create(target_url) == preconnect_origin_) {
      base::UmaHistogramEnumeration(
          source_is_default_search_engine_page_ ? histogram_name_dse
                                                : histogram_name_non_dse,
          ActionAccuracy::kPreconnectActionClickToSameOrigin);
      return;
    }

    base::UmaHistogramEnumeration(
        source_is_default_search_engine_page_ ? histogram_name_dse
                                              : histogram_name_non_dse,
        ActionAccuracy::kPreconnectActionClickToDifferentOrigin);
    return;
  }

  DCHECK(prefetch_url_);
  if (target_url == prefetch_url_.value()) {
    base::UmaHistogramEnumeration(
        source_is_default_search_engine_page_ ? histogram_name_dse
                                              : histogram_name_non_dse,
        ActionAccuracy::kPrefetchActionClickToSameURL);
    return;
  }

  if (url::Origin::Create(target_url) ==
      url::Origin::Create(prefetch_url_.value())) {
    base::UmaHistogramEnumeration(
        source_is_default_search_engine_page_ ? histogram_name_dse
                                              : histogram_name_non_dse,
        ActionAccuracy::kPrefetchActionClickToSameOrigin);
    return;
  }

  base::UmaHistogramEnumeration(
      source_is_default_search_engine_page_ ? histogram_name_dse
                                            : histogram_name_non_dse,
      ActionAccuracy::kPrefetchActionClickToDifferentOrigin);
  return;
}

void NavigationPredictor::OnVisibilityChanged(content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if the visibility changed from HIDDEN to VISIBLE. Since navigation
  // predictor is currently restriced to Android, it is okay to disregard the
  // occluded state.
  if (current_visibility_ != content::Visibility::HIDDEN ||
      visibility != content::Visibility::VISIBLE) {
    current_visibility_ = visibility;
    return;
  }

  current_visibility_ = visibility;

  // Previously, the visibility was HIDDEN, and now it is VISIBLE implying that
  // the web contents that was fully hidden is now fully visible.
  TakeActionNowOnTabOrAppVisibilityChange(
      Action::kPreconnectOnVisibilityChange);

  // To keep the overhead as low, Pre* action on tab foreground is taken at most
  // once per page.
  Observe(nullptr);
}

void NavigationPredictor::TakeActionNowOnTabOrAppVisibilityChange(
    Action log_action) {
  MaybePreconnectNow(log_action);
}

void NavigationPredictor::MaybePreconnectNow(Action log_action) {
  base::Optional<url::Origin> preconnect_origin = preconnect_origin_;

  if (prefetch_url_ && !preconnect_origin) {
    // Preconnect to the origin of the prefetch URL.
    preconnect_origin = url::Origin::Create(prefetch_url_.value());
  }

  if (!preconnect_origin)
    return;
  if (preconnect_origin->scheme() != url::kHttpScheme &&
      preconnect_origin->scheme() != url::kHttpsScheme) {
    return;
  }

  std::string action_histogram_name =
      source_is_default_search_engine_page_
          ? "NavigationPredictor.OnDSE.ActionTaken"
          : "NavigationPredictor.OnNonDSE.ActionTaken";
  base::UmaHistogramEnumeration(action_histogram_name, log_action);

  if (!same_origin_preconnecting_allowed_)
    return;

  auto* loading_predictor = predictors::LoadingPredictorFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  GURL preconnect_url_serialized(preconnect_origin->Serialize());
  DCHECK(preconnect_url_serialized.is_valid());
  loading_predictor->PrepareForPageLoad(
      preconnect_url_serialized, predictors::HintOrigin::NAVIGATION_PREDICTOR,
      true);
}

SiteEngagementService* NavigationPredictor::GetEngagementService() const {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* service = SiteEngagementService::Get(profile);
  DCHECK(service);
  return service;
}

TemplateURLService* NavigationPredictor::GetTemplateURLService() const {
  return TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
}

void NavigationPredictor::ReportAnchorElementMetricsOnClick(
    blink::mojom::AnchorElementMetricsPtr metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(
      blink::features::kRecordAnchorMetricsClicked));

  if (browser_context_->IsOffTheRecord())
    return;

  if (!IsValidMetricFromRenderer(*metrics)) {
    mojo::ReportBadMessage("Bad anchor element metrics: onClick.");
    return;
  }

  source_is_default_search_engine_page_ =
      GetTemplateURLService() &&
      GetTemplateURLService()->IsSearchResultsPageFromDefaultSearchProvider(
          metrics->source_url);
  if (!metrics->source_url.SchemeIsCryptographic() ||
      !metrics->target_url.SchemeIsCryptographic()) {
    return;
  }

  RecordTimingOnClick();

  SiteEngagementService* engagement_service = GetEngagementService();

  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Clicked.DocumentEngagementScore",
      static_cast<int>(engagement_service->GetScore(metrics->source_url)));

  double target_score = engagement_service->GetScore(metrics->target_url);
  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.HrefEngagementScore2",
                           static_cast<int>(target_score));
  if (target_score > 0) {
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Clicked.HrefEngagementScorePositive",
        static_cast<int>(target_score));
  }
  if (!metrics->is_same_host) {
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Clicked.HrefEngagementScoreExternal",
        static_cast<int>(target_score));
  }

  RecordActionAccuracyOnClick(metrics->target_url);

  // Look up the clicked URL in |navigation_scores_map_|. Record if we find it.
  auto iter = navigation_scores_map_.find(metrics->target_url.spec());
  if (iter == navigation_scores_map_.end())
    return;

  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.AreaRank",
                           static_cast<int>(iter->second->area_rank));
  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.NavigationScore",
                           static_cast<int>(iter->second->score));
  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.NavigationScoreRank",
                           static_cast<int>(iter->second->score_rank.value()));

  // Guaranteed to be non-zero since we have found the clicked link in
  // |navigation_scores_map_|.
  int number_of_anchors = static_cast<int>(navigation_scores_map_.size());
  if (metrics->is_same_host) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioSameHost_SameHost",
        (number_of_anchors_same_host_ * 100) / number_of_anchors);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioSameHost_DiffHost",
        (number_of_anchors_same_host_ * 100) / number_of_anchors);
  }

  if (source_is_default_search_engine_page_) {
    UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.OnDSE.SameHost",
                          metrics->is_same_host);
  } else {
    UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.OnNonDSE.SameHost",
                          metrics->is_same_host);
  }

  // Check if the clicked anchor element contains image or if any other anchor
  // element pointing to the same url contains an image.
  if (metrics->contains_image || iter->second->contains_image) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage",
        (number_of_anchors_contains_image_ * 100) / number_of_anchors);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioContainsImage_NoImage",
        (number_of_anchors_contains_image_ * 100) / number_of_anchors);
  }

  if (metrics->is_in_iframe) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioInIframe_InIframe",
        (number_of_anchors_in_iframe_ * 100) / number_of_anchors);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioInIframe_NotInIframe",
        (number_of_anchors_in_iframe_ * 100) / number_of_anchors);
  }

  if (metrics->is_url_incremented_by_one) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioUrlIncremented_UrlIncremented",
        (number_of_anchors_url_incremented_ * 100) / number_of_anchors);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioUrlIncremented_NotIncremented",
        (number_of_anchors_url_incremented_ * 100) / number_of_anchors);
  }
}

void NavigationPredictor::MergeMetricsSameTargetUrl(
    std::vector<blink::mojom::AnchorElementMetricsPtr>* metrics) const {
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", metrics->size());

  // Maps from target url (href) to anchor element metrics from renderer.
  std::unordered_map<std::string, blink::mojom::AnchorElementMetricsPtr>
      metrics_map;

  // This size reserve is aggressive since |metrics_map| may contain fewer
  // elements than metrics->size() after merge.
  metrics_map.reserve(metrics->size());

  for (auto& metric : *metrics) {
    // Do not include anchor elements that point to the same URL as the URL of
    // the current navigation since these are unlikely to be clicked.
    if (metric->target_url == metric->source_url)
      continue;

    if (!metric->target_url.SchemeIsCryptographic())
      continue;

    // Currently, all predictions are made based on elements that are within the
    // main frame since it is unclear if we can pre* the target of the elements
    // within iframes.
    if (metric->is_in_iframe)
      continue;

    const std::string& key = metric->target_url.spec();
    auto iter = metrics_map.find(key);
    if (iter == metrics_map.end()) {
      metrics_map[key] = std::move(metric);
    } else {
      auto& prev_metric = iter->second;
      prev_metric->ratio_area += metric->ratio_area;
      prev_metric->ratio_visible_area += metric->ratio_visible_area;

      // After merging, value of |ratio_area| can go beyond 1.0. This can
      // happen, e.g., when there are 2 anchor elements pointing to the same
      // target. The first anchor element occupies 90% of the viewport. The
      // second one has size 0.8 times the viewport, and only part of it is
      // visible in the viewport. In that case, |ratio_area| may be 1.7.
      if (prev_metric->ratio_area > 1.0)
        prev_metric->ratio_area = 1.0;
      DCHECK_LE(0.0, prev_metric->ratio_area);
      DCHECK_GE(1.0, prev_metric->ratio_area);

      DCHECK_GE(1.0, prev_metric->ratio_visible_area);

      // Position related metrics are tricky to merge. Another possible way to
      // merge is simply add up the calculated navigation scores.
      prev_metric->ratio_distance_root_top =
          std::min(prev_metric->ratio_distance_root_top,
                   metric->ratio_distance_root_top);
      prev_metric->ratio_distance_root_bottom =
          std::max(prev_metric->ratio_distance_root_bottom,
                   metric->ratio_distance_root_bottom);
      prev_metric->ratio_distance_top_to_visible_top =
          std::min(prev_metric->ratio_distance_top_to_visible_top,
                   metric->ratio_distance_top_to_visible_top);
      prev_metric->ratio_distance_center_to_visible_top =
          std::min(prev_metric->ratio_distance_center_to_visible_top,
                   metric->ratio_distance_center_to_visible_top);

      // Anchor element is not considered in an iframe as long as at least one
      // of them is not in an iframe.
      prev_metric->is_in_iframe =
          prev_metric->is_in_iframe && metric->is_in_iframe;
      prev_metric->contains_image =
          prev_metric->contains_image || metric->contains_image;
      DCHECK_EQ(prev_metric->is_same_host, metric->is_same_host);
    }
  }

  metrics->clear();

  if (metrics_map.empty())
    return;

  metrics->reserve(metrics_map.size());
  for (auto& metric_mapping : metrics_map) {
    metrics->push_back(std::move(metric_mapping.second));
  }

  DCHECK(!metrics->empty());
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge",
      metrics->size());
}

void NavigationPredictor::ReportAnchorElementMetricsOnLoad(
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(
      blink::features::kRecordAnchorMetricsVisible));

  // Each document should only report metrics once when page is loaded.
  DCHECK(navigation_scores_map_.empty());

  if (browser_context_->IsOffTheRecord())
    return;

  if (metrics.empty()) {
    mojo::ReportBadMessage("Bad anchor element metrics: empty.");
    return;
  }

  for (const auto& metric : metrics) {
    if (!IsValidMetricFromRenderer(*metric)) {
      mojo::ReportBadMessage("Bad anchor element metrics: onLoad.");
      return;
    }
  }

  if (!metrics[0]->source_url.SchemeIsCryptographic())
    return;

  document_loaded_timing_ = base::TimeTicks::Now();

  source_is_default_search_engine_page_ =
      GetTemplateURLService() &&
      GetTemplateURLService()->IsSearchResultsPageFromDefaultSearchProvider(
          metrics[0]->source_url);
  MergeMetricsSameTargetUrl(&metrics);

  if (metrics.empty())
    return;

  // Count the number of anchors that have specific metrics.
  for (const auto& metric : metrics) {
    number_of_anchors_same_host_ += static_cast<int>(metric->is_same_host);
    number_of_anchors_contains_image_ +=
        static_cast<int>(metric->contains_image);
    number_of_anchors_in_iframe_ += static_cast<int>(metric->is_in_iframe);
    number_of_anchors_url_incremented_ +=
        static_cast<int>(metric->is_url_incremented_by_one);
  }

  // Retrieve site engagement score of the document. |metrics| is guaranteed to
  // be non-empty. All |metrics| have the same source_url.
  SiteEngagementService* engagement_service = GetEngagementService();
  double document_engagement_score =
      engagement_service->GetScore(metrics[0]->source_url);
  DCHECK(document_engagement_score >= 0 &&
         document_engagement_score <= engagement_service->GetMaxPoints());
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.DocumentEngagementScore",
      static_cast<int>(document_engagement_score));

  // Sort metric by area in descending order to get area rank, which is a
  // derived feature to calculate navigation score.
  std::sort(metrics.begin(), metrics.end(), [](const auto& a, const auto& b) {
    return a->ratio_area > b->ratio_area;
  });

  // Loop |metrics| to compute navigation scores.
  std::vector<std::unique_ptr<NavigationScore>> navigation_scores;
  navigation_scores.reserve(metrics.size());
  double total_score = 0.0;
  for (size_t i = 0; i != metrics.size(); ++i) {
    const auto& metric = metrics[i];
    RecordMetricsOnLoad(*metric);

    const double target_engagement_score =
        engagement_service->GetScore(metric->target_url);
    DCHECK(target_engagement_score >= 0 &&
           target_engagement_score <= engagement_service->GetMaxPoints());
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Visible.HrefEngagementScore2",
        static_cast<int>(target_engagement_score));
    if (!metric->is_same_host) {
      UMA_HISTOGRAM_COUNTS_100(
          "AnchorElementMetrics.Visible.HrefEngagementScoreExternal",
          static_cast<int>(target_engagement_score));
    }

    // Anchor elements with the same area are assigned with the same rank.
    size_t area_rank = i;
    if (i > 0 && metric->ratio_area == metrics[i - 1]->ratio_area)
      area_rank = navigation_scores[navigation_scores.size() - 1]->area_rank;

    double score = CalculateAnchorNavigationScore(
        *metric, document_engagement_score, target_engagement_score, area_rank,
        metrics.size());
    total_score += score;

    navigation_scores.push_back(std::make_unique<NavigationScore>(
        metric->target_url, area_rank, score, metric->contains_image));
  }

  // Normalize |score| to a total sum of 100.0 across all anchor elements
  // received.
  if (total_score > 0.0) {
    for (auto& navigation_score : navigation_scores) {
      navigation_score->score = navigation_score->score / total_score * 100.0;
    }
  }

  // Sort scores by the calculated navigation score in descending order. This
  // score rank is used by MaybeTakeActionOnLoad, and stored in
  // |navigation_scores_map_|.
  std::sort(navigation_scores.begin(), navigation_scores.end(),
            [](const auto& a, const auto& b) { return a->score > b->score; });

  const url::Origin document_origin =
      url::Origin::Create(metrics[0]->source_url);
  MaybeTakeActionOnLoad(document_origin, navigation_scores);

  // Store navigation scores in |navigation_scores_map_| for fast look up upon
  // clicks.
  navigation_scores_map_.reserve(navigation_scores.size());
  for (size_t i = 0; i != navigation_scores.size(); ++i) {
    navigation_scores[i]->score_rank = base::make_optional(i);
    navigation_scores_map_[navigation_scores[i]->url.spec()] =
        std::move(navigation_scores[i]);
  }
}

double NavigationPredictor::CalculateAnchorNavigationScore(
    const blink::mojom::AnchorElementMetrics& metrics,
    double document_engagement_score,
    double target_engagement_score,
    int area_rank,
    int number_of_anchors) const {
  DCHECK(!browser_context_->IsOffTheRecord());

  if (sum_scales_ == 0)
    return 0.0;

  double max_engagement_points = GetEngagementService()->GetMaxPoints();
  document_engagement_score /= max_engagement_points;
  target_engagement_score /= max_engagement_points;

  double area_rank_score =
      (double)((number_of_anchors - area_rank)) / number_of_anchors;

  DCHECK_LE(0, metrics.ratio_visible_area);
  DCHECK_GE(1, metrics.ratio_visible_area);

  DCHECK_LE(0, metrics.is_in_iframe);
  DCHECK_GE(1, metrics.is_in_iframe);

  DCHECK_LE(0, metrics.is_same_host);
  DCHECK_GE(1, metrics.is_same_host);

  DCHECK_LE(0, metrics.contains_image);
  DCHECK_GE(1, metrics.contains_image);

  DCHECK_LE(0, metrics.is_url_incremented_by_one);
  DCHECK_GE(1, metrics.is_url_incremented_by_one);

  DCHECK_LE(0, document_engagement_score);
  DCHECK_GE(1, document_engagement_score);

  DCHECK_LE(0, target_engagement_score);
  DCHECK_GE(1, target_engagement_score);

  DCHECK_LE(0, area_rank_score);
  DCHECK_GE(1, area_rank_score);

  double host_score = 0.0;
  // On pages from default search engine, give higher weight to target URLs that
  // link to a different host. On non-default search engine pages, give higher
  // weight to target URLs that link to the same host.
  if (!source_is_default_search_engine_page_ && metrics.is_same_host) {
    host_score = is_same_host_scale_;
  } else if (source_is_default_search_engine_page_ && !metrics.is_same_host) {
    host_score = is_same_host_scale_;
  }

  // TODO(chelu): https://crbug.com/850624/. Experiment with other heuristic
  // algorithms for computing the anchor elements score.
  double score = ratio_area_scale_ * metrics.ratio_visible_area +
                 is_in_iframe_scale_ * metrics.is_in_iframe +
                 contains_image_scale_ * metrics.contains_image + host_score +
                 is_url_incremented_scale_ * metrics.is_url_incremented_by_one +
                 source_engagement_score_scale_ * document_engagement_score +
                 target_engagement_score_scale_ * target_engagement_score +
                 area_rank_scale_ * (area_rank_score);

  // Normalize to 100.
  score = score / sum_scales_ * 100.0;
  DCHECK_LE(0.0, score);
  DCHECK_GE(100.0, score);
  return score;
}

void NavigationPredictor::MaybeTakeActionOnLoad(
    const url::Origin& document_origin,
    const std::vector<std::unique_ptr<NavigationScore>>&
        sorted_navigation_scores) {
  DCHECK(!browser_context_->IsOffTheRecord());

  // |sorted_navigation_scores| are sorted in descending order, the first one
  // has the highest navigation score.
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.HighestNavigationScore",
      static_cast<int>(sorted_navigation_scores[0]->score));

  std::string action_histogram_name =
      source_is_default_search_engine_page_
          ? "NavigationPredictor.OnDSE.ActionTaken"
          : "NavigationPredictor.OnNonDSE.ActionTaken";

  DCHECK(!preconnect_origin_.has_value());
  DCHECK(!prefetch_url_.has_value());

  // Try prefetch first.
  prefetch_url_ = GetUrlToPrefetch(document_origin, sorted_navigation_scores);
  if (prefetch_url_.has_value()) {
    DCHECK_EQ(document_origin.host(), prefetch_url_->host());
    MaybePreconnectNow(Action::kPrefetch);
    return;
  }

  // Compute preconnect origin only if there is no valid prefetch URL.
  preconnect_origin_ =
      GetOriginToPreconnect(document_origin, sorted_navigation_scores);
  if (preconnect_origin_.has_value()) {
    DCHECK_EQ(document_origin.host(), preconnect_origin_->host());
    MaybePreconnectNow(Action::kPreconnect);
    return;
  }

  base::UmaHistogramEnumeration(action_histogram_name, Action::kNone);
}

base::Optional<GURL> NavigationPredictor::GetUrlToPrefetch(
    const url::Origin& document_origin,
    const std::vector<std::unique_ptr<NavigationScore>>&
        sorted_navigation_scores) const {
  // Currently, prefetch is disabled on low-end devices since prefetch may
  // increase memory usage.
  if (is_low_end_device_)
    return base::nullopt;

  // On search engine results page, next navigation is likely to be a different
  // origin. Currently, the prefetch is only allowed for same orgins. Hence,
  // prefetch is currently disabled on search engine results page.
  if (source_is_default_search_engine_page_)
    return base::nullopt;

  if (sorted_navigation_scores.empty())
    return base::nullopt;

  // Only the same origin URLs are eligible for prefetching. If the URL with
  // the highest score is from a different origin, then we skip prefetching
  // since same origin URLs are not likely to be clicked.
  if (url::Origin::Create(sorted_navigation_scores[0]->url) !=
      document_origin) {
    return base::nullopt;
  }

  // If the prediction score of the highest scoring URL is less than the
  // threshold, then return.
  if (sorted_navigation_scores[0]->score < prefetch_url_score_threshold_)
    return base::nullopt;
  return sorted_navigation_scores[0]->url;
}

base::Optional<url::Origin> NavigationPredictor::GetOriginToPreconnect(
    const url::Origin& document_origin,
    const std::vector<std::unique_ptr<NavigationScore>>&
        sorted_navigation_scores) const {
  // On search engine results page, next navigation is likely to be a different
  // origin. Currently, the preconnect is only allowed for same origins. Hence,
  // preconnect is currently disabled on search engine results page.
  if (source_is_default_search_engine_page_)
    return base::nullopt;

  // Compute preconnect score for each origins: Multiple anchor elements on
  // the webpage may point to the same origin. The preconnect score for an
  // origin is computed by taking sum of score of all anchor elements that
  // point to that origin.
  std::map<url::Origin, double> preconnect_score_by_origin_map;
  for (const auto& navigation_score : sorted_navigation_scores) {
    const url::Origin origin = url::Origin::Create(navigation_score->url);

    auto iter = preconnect_score_by_origin_map.find(origin);
    if (iter == preconnect_score_by_origin_map.end()) {
      preconnect_score_by_origin_map[origin] = navigation_score->score;
    } else {
      double& existing_metric = iter->second;
      existing_metric += navigation_score->score;
    }
  }

  struct ScoreByOrigin {
    url::Origin origin;
    double score;

    ScoreByOrigin(const url::Origin& origin, double score)
        : origin(origin), score(score) {}
  };

  // |sorted_preconnect_scores| would contain preconnect scores of different
  // origins sorted in descending order of the preconnect score.
  std::vector<ScoreByOrigin> sorted_preconnect_scores;

  // First copy all entries from |preconnect_score_by_origin_map| to
  // |sorted_preconnect_scores|.
  for (const auto& score_by_origin_map_entry : preconnect_score_by_origin_map) {
    ScoreByOrigin entry(score_by_origin_map_entry.first,
                        score_by_origin_map_entry.second);
    sorted_preconnect_scores.push_back(entry);
  }

  if (sorted_preconnect_scores.empty())
    return base::nullopt;

  // Sort scores by the calculated preconnect score in descending order.
  std::sort(sorted_preconnect_scores.begin(), sorted_preconnect_scores.end(),
            [](const auto& a, const auto& b) { return a.score > b.score; });

#if DCHECK_IS_ON()
  // |sum_of_scores| must be close to the total score of 100.
  double sum_of_scores = 0.0;
  for (const auto& score_by_origin : sorted_preconnect_scores)
    sum_of_scores += score_by_origin.score;
  // Allow an error of 2.0. i.e., |sum_of_scores| is expected to be between 98
  // and 102.
  DCHECK_GE(2.0, std::abs(sum_of_scores - 100));
#endif

  // Connect to the origin with highest score provided the origin is same
  // as the document origin.
  if (sorted_preconnect_scores[0].origin != document_origin)
    return base::nullopt;

  // If the prediction score of the highest scoring origin is less than the
  // threshold, then return.
  if (sorted_preconnect_scores[0].score < preconnect_origin_score_threshold_) {
    return base::nullopt;
  }

  return sorted_preconnect_scores[0].origin;
}

void NavigationPredictor::RecordMetricsOnLoad(
    const blink::mojom::AnchorElementMetrics& metric) const {
  DCHECK(!browser_context_->IsOffTheRecord());

  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Visible.RatioArea",
                           static_cast<int>(metric.ratio_area * 100));

  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Visible.RatioVisibleArea",
                           static_cast<int>(metric.ratio_visible_area * 100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Visible.RatioDistanceTopToVisibleTop",
      static_cast<int>(
          std::min(metric.ratio_distance_top_to_visible_top, 1.0f) * 100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Visible.RatioDistanceCenterToVisibleTop",
      static_cast<int>(
          std::min(metric.ratio_distance_center_to_visible_top, 1.0f) * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Visible.RatioDistanceRootTop",
      static_cast<int>(std::min(metric.ratio_distance_root_top, 100.0f) * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Visible.RatioDistanceRootBottom",
      static_cast<int>(std::min(metric.ratio_distance_root_bottom, 100.0f) *
                       100));

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsInIFrame",
                        metric.is_in_iframe);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.ContainsImage",
                        metric.contains_image);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsSameHost",
                        metric.is_same_host);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsUrlIncrementedByOne",
                        metric.is_url_incremented_by_one);
}

#ifdef OS_ANDROID
void NavigationPredictor::OnApplicationStateChange(
    base::android::ApplicationState application_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!application_status_listener_)
    return;

  if (application_state_ !=
          base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES ||
      application_state !=
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    application_state_ = application_state;
    return;
  }

  application_state_ = application_state;

  // The app was moved from background to foreground.
  TakeActionNowOnTabOrAppVisibilityChange(Action::kPreconnectOnAppForeground);

  // To keep the overhead as low, Pre* action on app foreground is taken at most
  // once per page. Stop listening to application change events only if
  // OnLoad() has been fired implying that prediction computation has finished.
  if (document_loaded_timing_ != base::TimeTicks())
    application_status_listener_.reset();
}
#endif  // OS_ANDROID
