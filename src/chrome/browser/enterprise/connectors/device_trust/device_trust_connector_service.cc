// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

const base::Value* GetPolicyUrlPatterns(PrefService* prefs) {
  return prefs->GetList(kContextAwareAccessSignalsAllowlistPref);
}

bool ConnectorPolicyHasValues(PrefService* profile_prefs) {
  const auto* list = GetPolicyUrlPatterns(profile_prefs);
  return list && !list->GetList().empty();
}

}  // namespace

// static
bool DeviceTrustConnectorService::IsConnectorEnabled(
    PrefService* profile_prefs) {
  if (!IsDeviceTrustConnectorFeatureEnabled() || !profile_prefs) {
    return false;
  }

  return ConnectorPolicyHasValues(profile_prefs);
}

DeviceTrustConnectorService::DeviceTrustConnectorService(
    PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  DCHECK(profile_prefs_);
}

DeviceTrustConnectorService::~DeviceTrustConnectorService() = default;

bool DeviceTrustConnectorService::IsConnectorEnabled() const {
  return DeviceTrustConnectorService::IsConnectorEnabled(profile_prefs_);
}

void DeviceTrustConnectorService::Initialize() {
  if (!IsDeviceTrustConnectorFeatureEnabled()) {
    return;
  }

  if (!pref_observer_.IsEmpty()) {
    return;
  }

  pref_observer_.Init(profile_prefs_);
  pref_observer_.Add(
      kContextAwareAccessSignalsAllowlistPref,
      base::BindRepeating(&DeviceTrustConnectorService::OnPolicyUpdated,
                          weak_factory_.GetWeakPtr()));

  // Call once to initialize the watcher with the current pref's values.
  OnPolicyUpdated();
}

bool DeviceTrustConnectorService::Watches(const GURL& url) const {
  return matcher_ && !matcher_->MatchURL(url).empty();
}

void DeviceTrustConnectorService::OnConnectorEnabled() {
  // No-op by default.
}

void DeviceTrustConnectorService::OnPolicyUpdated() {
  DCHECK(IsDeviceTrustConnectorFeatureEnabled());

  const base::Value* url_patterns = GetPolicyUrlPatterns(profile_prefs_);

  if (!matcher_ || !matcher_->IsEmpty()) {
    // Reset the matcher.
    matcher_ = std::make_unique<url_matcher::URLMatcher>();
  }

  if (url_patterns && !url_patterns->GetList().empty()) {
    // Add the new endpoints to the conditions.
    url_matcher::util::AddAllowFilters(
        matcher_.get(), &base::Value::AsListValue(*url_patterns));

    // Call the hook which signals that the connector has been enabled.
    OnConnectorEnabled();
  }
}

}  // namespace enterprise_connectors
