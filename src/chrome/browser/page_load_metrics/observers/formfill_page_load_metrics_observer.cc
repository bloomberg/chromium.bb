// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/formfill_page_load_metrics_observer.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/federated_learning/floc_eligibility_observer.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

const char kUserDataFieldFilledKey[] = "UserDataFieldFilled";

}  // namespace

FormfillPageLoadMetricsObserver::FormfillPageLoadMetricsObserver() = default;

FormfillPageLoadMetricsObserver::~FormfillPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FormfillPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(Profile::FromBrowserContext(
          GetDelegate().GetWebContents()->GetBrowserContext()));
  DCHECK(settings_map);

  const url::Origin& origin =
      navigation_handle->GetRenderFrameHost()->GetLastCommittedOrigin();

  std::unique_ptr<base::Value> formfill_metadata =
      settings_map->GetWebsiteSetting(origin.GetURL(), origin.GetURL(),
                                      ContentSettingsType::FORMFILL_METADATA,
                                      nullptr);

  // User data field was detected on this site before.
  if (formfill_metadata && formfill_metadata->is_dict() &&
      formfill_metadata->FindBoolKey(kUserDataFieldFilledKey)) {
    page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
        navigation_handle->GetRenderFrameHost(),
        blink::mojom::WebFeature::kUserDataFieldFilledPreviously);
  }

  return page_load_metrics::PageLoadMetricsObserver::ObservePolicy::
      CONTINUE_OBSERVING;
}

void FormfillPageLoadMetricsObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  if (user_data_field_detected_)
    return;

  bool observed_user_data_field_detected_feature = base::ranges::any_of(
      features, [](const blink::UseCounterFeature& feature) {
        return feature.type() ==
                   blink::mojom::UseCounterFeatureType::kWebFeature &&
               (static_cast<blink::mojom::WebFeature>(feature.value()) ==
                    blink::mojom::WebFeature::
                        kUserDataFieldFilled_PredictedTypeMatch ||
                static_cast<blink::mojom::WebFeature>(feature.value()) ==
                    blink::mojom::WebFeature::kEmailFieldFilled_PatternMatch);
      });

  if (!observed_user_data_field_detected_feature)
    return;

  user_data_field_detected_ = true;

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(Profile::FromBrowserContext(
          GetDelegate().GetWebContents()->GetBrowserContext()));
  DCHECK(settings_map);

  const url::Origin& origin = rfh->GetLastCommittedOrigin();
  std::unique_ptr<base::Value> formfill_metadata =
      settings_map->GetWebsiteSetting(origin.GetURL(), origin.GetURL(),
                                      ContentSettingsType::FORMFILL_METADATA,
                                      nullptr);

  if (!formfill_metadata || !formfill_metadata->is_dict()) {
    formfill_metadata =
        std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  }

  if (!formfill_metadata->FindBoolKey(kUserDataFieldFilledKey)) {
    formfill_metadata->SetBoolKey(kUserDataFieldFilledKey, true);

    settings_map->SetWebsiteSettingDefaultScope(
        origin.GetURL(), origin.GetURL(),
        ContentSettingsType::FORMFILL_METADATA, std::move(formfill_metadata));
  }
}
