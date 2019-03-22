// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/unified_consent/unified_consent_service_client_impl.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/pref_names.h"

UnifiedConsentServiceClientImpl::UnifiedConsentServiceClientImpl(
    PrefService* user_pref_service,
    PrefService* local_pref_service)
    : user_pref_service_(user_pref_service),
      local_pref_service_(local_pref_service) {
  DCHECK(user_pref_service_);
  DCHECK(local_pref_service_);
  ObserveServicePrefChange(Service::kMetricsReporting,
                           metrics::prefs::kMetricsReportingEnabled,
                           local_pref_service);
  ObserveServicePrefChange(Service::kNetworkPrediction,
                           prefs::kNetworkPredictionEnabled,
                           user_pref_service_);
  ObserveServicePrefChange(Service::kSearchSuggest,
                           prefs::kSearchSuggestEnabled, user_pref_service_);
}

UnifiedConsentServiceClientImpl::ServiceState
UnifiedConsentServiceClientImpl::GetServiceState(Service service) {
  bool enabled;
  switch (service) {
    case Service::kMetricsReporting: {
      BooleanPrefMember metrics_pref;
      metrics_pref.Init(metrics::prefs::kMetricsReportingEnabled,
                        local_pref_service_);
      enabled = metrics_pref.GetValue();
      break;
    }
    case Service::kNetworkPrediction: {
      BooleanPrefMember network_pref;
      network_pref.Init(prefs::kNetworkPredictionEnabled, user_pref_service_);
      enabled = network_pref.GetValue();
      break;
    }
    case Service::kSearchSuggest:
      enabled = user_pref_service_->GetBoolean(prefs::kSearchSuggestEnabled);
      break;
    case Service::kAlternateErrorPages:
    case Service::kSafeBrowsing:
    case Service::kSafeBrowsingExtendedReporting:
    case Service::kSpellCheck:
    case Service::kContextualSearch:
      return ServiceState::kNotSupported;
  }
  return enabled ? ServiceState::kEnabled : ServiceState::kDisabled;
}

void UnifiedConsentServiceClientImpl::SetServiceEnabled(Service service,
                                                        bool enabled) {
  switch (service) {
    case Service::kMetricsReporting: {
      BooleanPrefMember metrics_pref;
      metrics_pref.Init(metrics::prefs::kMetricsReportingEnabled,
                        local_pref_service_);
      metrics_pref.SetValue(enabled);
      BooleanPrefMember metrics_wifi_pref;
      metrics_wifi_pref.Init(prefs::kMetricsReportingWifiOnly,
                             local_pref_service_);
      metrics_wifi_pref.SetValue(enabled);
      break;
    }
    case Service::kNetworkPrediction: {
      BooleanPrefMember network_pref;
      network_pref.Init(prefs::kNetworkPredictionEnabled, user_pref_service_);
      network_pref.SetValue(enabled);
      BooleanPrefMember network_wifi_pref;
      network_wifi_pref.Init(prefs::kNetworkPredictionWifiOnly,
                             user_pref_service_);
      network_wifi_pref.SetValue(enabled);
      break;
    }
    case Service::kSearchSuggest:
      user_pref_service_->SetBoolean(prefs::kSearchSuggestEnabled, enabled);
      break;
    case Service::kAlternateErrorPages:
    case Service::kSafeBrowsing:
    case Service::kSafeBrowsingExtendedReporting:
    case Service::kSpellCheck:
    case Service::kContextualSearch:
      NOTIMPLEMENTED() << "Feature not available on iOS";
      break;
  }
}
