// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom.h"

using FeatureType = blink::mojom::UseCounterFeatureType;
using UkmFeatureList = UseCounterPageLoadMetricsObserver::UkmFeatureList;
using WebFeature = blink::mojom::WebFeature;
using CSSSampleId = blink::mojom::CSSSampleId;
using PermissionsPolicyFeature = blink::mojom::PermissionsPolicyFeature;

namespace {

// It's always recommended to use the deprecation API in blink. If the feature
// was logged from the browser (or from both blink and the browser) where the
// deprecation API is not available, use this method for the console warning.
// Note that this doesn't generate the deprecation report.
void PossiblyWarnFeatureDeprecation(content::RenderFrameHost* rfh,
                                    WebFeature feature) {
  switch (feature) {
    case WebFeature::kDownloadInSandbox:
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "Download is disallowed. The frame initiating or instantiating the "
          "download is sandboxed, but the flag ‘allow-downloads’ is not set. "
          "See https://www.chromestatus.com/feature/5706745674465280 for more "
          "details.");
      return;
    case WebFeature::kDownloadInAdFrameWithoutUserGesture:
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "[Intervention] Download in ad frame without user activation is "
          "not allowed. See "
          "https://www.chromestatus.com/feature/6311883621531648 for more "
          "details.");
      return;

    default:
      return;
  }
}

template <size_t N>
bool TestAndSet(std::bitset<N>& bitset,
                blink::UseCounterFeature::EnumValue value) {
  bool has_record = bitset.test(value);
  bitset.set(value);
  return has_record;
}

}  // namespace

UseCounterPageLoadMetricsObserver::UseCounterPageLoadMetricsObserver() =
    default;

UseCounterPageLoadMetricsObserver::~UseCounterPageLoadMetricsObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  // Verify that no feature usage is observed before commit
  DCHECK_EQ(features_recorded_.count(), 0ul);
  DCHECK_EQ(main_frame_features_recorded_.count(), 0ul);
  DCHECK_EQ(ukm_features_recorded_.count(), 0ul);
  DCHECK_EQ(css_properties_recorded_.count(), 0ul);
  DCHECK_EQ(animated_css_properties_recorded_.count(), 0ul);
  DCHECK_EQ(violated_permissions_policy_features_recorded_.count(), 0ul);
  DCHECK_EQ(iframe_permissions_policy_features_recorded_, 0ul);
  DCHECK_EQ(header_permissions_policy_features_recorded_, 0ul);

  content::RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();

  auto web_feature_page_visit =
      static_cast<blink::UseCounterFeature::EnumValue>(WebFeature::kPageVisits);

  ukm::builders::Blink_UseCounter(source_id)
      .SetFeature(web_feature_page_visit)
      .SetIsMainFrameFeature(1)
      .Record(ukm::UkmRecorder::Get());
  ukm_features_recorded_.set(web_feature_page_visit);

  RecordMainFrameWebFeature(rfh, WebFeature::kPageVisits);
  RecordUseCounterFeature(rfh,
                          {FeatureType::kWebFeature, web_feature_page_visit});

  auto css_total_pages_measured =
      static_cast<blink::UseCounterFeature::EnumValue>(
          CSSSampleId::kTotalPagesMeasured);
  RecordUseCounterFeature(
      rfh, {FeatureType::kCssProperty, css_total_pages_measured});
  RecordUseCounterFeature(
      rfh, {FeatureType::kAnimatedCssProperty, css_total_pages_measured});

  return CONTINUE_OBSERVING;
}

void UseCounterPageLoadMetricsObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  for (const blink::UseCounterFeature& feature : features) {
    if (feature.type() == FeatureType::kWebFeature) {
      RecordMainFrameWebFeature(rfh, static_cast<WebFeature>(feature.value()));
    }
    RecordUseCounterFeature(rfh, feature);
  }
}

void UseCounterPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordUkmFeatures();
}

void UseCounterPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  RecordUkmFeatures();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordUkmFeatures();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  return PageLoadMetricsObserver::ShouldObserveMimeType(mime_type) ==
                     CONTINUE_OBSERVING ||
                 mime_type == "image/svg+xml"
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

void UseCounterPageLoadMetricsObserver::RecordUseCounterFeature(
    content::RenderFrameHost* rfh,
    const blink::UseCounterFeature& feature) {
  // Note: UMA_HISTOGRAM_ENUMERATION does accept a 3rd parameter as value bound,
  // when the second parameter is a general integer type, but it requires the
  // 3rd parameter to be constexpr in order to pass an internal static check.
  // Writing something like
  // UMA_HISTOGRAM_ENUMERATION(histogram_name, feature.value(),
  //                           feature.max_value());
  // will cause compile error.
  switch (feature.type()) {
    case FeatureType::kWebFeature: {
      auto web_feature = static_cast<WebFeature>(feature.value());
      if (TestAndSet(features_recorded_, feature.value()))
        return;
      UMA_HISTOGRAM_ENUMERATION(internal::kFeaturesHistogramName, web_feature);
      PossiblyWarnFeatureDeprecation(rfh, web_feature);
      break;
    }
    // There are about 600 enums, so the memory required for a vector
    // histogram is about 600 * 8 byes = 5KB 50% of the time there are about
    // 100 CSS properties recorded per page load. Storage in sparce
    // histogram entries are 48 bytes instead of 8 bytes so the memory
    // required for a sparse histogram is about 100 * 48 bytes = 5KB. On top
    // there will be std::map overhead and the acquire/release of a
    // base::Lock to protect the map during each update. Overal it is still
    // better to use a vector histogram here since it is faster to access
    // and merge and uses about same amount of memory.
    case FeatureType::kCssProperty:
      if (TestAndSet(css_properties_recorded_, feature.value()))
        return;
      UMA_HISTOGRAM_ENUMERATION(internal::kCssPropertiesHistogramName,
                                static_cast<CSSSampleId>(feature.value()));
      break;
    case FeatureType::kAnimatedCssProperty:
      if (TestAndSet(animated_css_properties_recorded_, feature.value()))
        return;
      UMA_HISTOGRAM_ENUMERATION(internal::kAnimatedCssPropertiesHistogramName,
                                static_cast<CSSSampleId>(feature.value()));
      break;

    case FeatureType::kPermissionsPolicyViolationEnforce:
      if (TestAndSet(violated_permissions_policy_features_recorded_,
                     feature.value()))
        return;
      UMA_HISTOGRAM_ENUMERATION(
          internal::kPermissionsPolicyViolationHistogramName,
          static_cast<PermissionsPolicyFeature>(feature.value()));
      break;
    case FeatureType::kPermissionsPolicyHeader:
      if (TestAndSet(header_permissions_policy_features_recorded_,
                     feature.value()))
        return;
      UMA_HISTOGRAM_ENUMERATION(
          internal::kPermissionsPolicyHeaderHistogramName,
          static_cast<PermissionsPolicyFeature>(feature.value()));
      break;
    case FeatureType::kPermissionsPolicyIframeAttribute:
      if (TestAndSet(iframe_permissions_policy_features_recorded_,
                     feature.value()))
        return;
      UMA_HISTOGRAM_ENUMERATION(
          internal::kPermissionsPolicyIframeAttributeHistogramName,
          static_cast<PermissionsPolicyFeature>(feature.value()));
      break;
  }
}

void UseCounterPageLoadMetricsObserver::RecordMainFrameWebFeature(
    content::RenderFrameHost* rfh,
    blink::mojom::WebFeature web_feature) {
  if (rfh->GetParentOrOuterDocument() != nullptr)
    return;

  if (TestAndSet(main_frame_features_recorded_,
                 static_cast<size_t>(web_feature))) {
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(internal::kFeaturesHistogramMainFrameName,
                            web_feature);
}

void UseCounterPageLoadMetricsObserver::RecordUkmFeatures() {
  for (WebFeature web_feature : GetAllowedUkmFeatures()) {
    auto feature_enum_value =
        static_cast<blink::UseCounterFeature::EnumValue>(web_feature);
    if (!features_recorded_.test(feature_enum_value))
      continue;

    if (TestAndSet(ukm_features_recorded_, feature_enum_value))
      continue;

    ukm::builders::Blink_UseCounter(GetDelegate().GetPageUkmSourceId())
        .SetFeature(feature_enum_value)
        .SetIsMainFrameFeature(
            main_frame_features_recorded_.test(feature_enum_value))
        .Record(ukm::UkmRecorder::Get());
  }
}
