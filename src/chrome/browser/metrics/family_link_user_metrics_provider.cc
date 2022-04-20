// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "components/session_manager/core/session_manager.h"

namespace {

constexpr char kFamilyLinkUserLogSegmentHistogramName[] =
    "FamilyLinkUser.LogSegment";

}  // namespace

FamilyLinkUserMetricsProvider::FamilyLinkUserMetricsProvider() {
  auto* factory = IdentityManagerFactory::GetInstance();
  if (factory)
    scoped_factory_observation_.Observe(factory);
}

FamilyLinkUserMetricsProvider::~FamilyLinkUserMetricsProvider() = default;

void FamilyLinkUserMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  // This function is called at unpredictable intervals throughout the Chrome
  // session, so guarantee it will never crash.
  if (!log_segment_)
    return;
  base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                log_segment_.value());
}

void FamilyLinkUserMetricsProvider::IdentityManagerCreated(
    signin::IdentityManager* identity_manager) {
  CHECK(identity_manager);
  scoped_observations_.AddObservation(identity_manager);
}

void FamilyLinkUserMetricsProvider::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (scoped_observations_.IsObservingSource(identity_manager)) {
    scoped_observations_.RemoveObservation(identity_manager);
  }
}

void FamilyLinkUserMetricsProvider::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  auto is_subject_to_parental_controls =
      account_info.capabilities.is_subject_to_parental_controls();
  switch (is_subject_to_parental_controls) {
    case signin::Tribool::kFalse:
    case signin::Tribool::kUnknown: {
      // Log as unsupervised user if the account is subject to parental
      // controls or if the capability is not known.
      SetLogSegment(LogSegment::kUnsupervised);
      return;
    }
    case signin::Tribool::kTrue: {
      auto can_stop_supervision =
          account_info.capabilities.can_stop_parental_supervision();
      if (can_stop_supervision == signin::Tribool::kTrue) {
        // Log as a supervised user that has chosen to enable parental
        // supervision on their account, e.g. Geller accounts.
        SetLogSegment(LogSegment::kSupervisionEnabledByUser);
      } else {
        // Log as a supervised user that has parental supervision enabled
        // by a policy applied to their account, e.g. Unicorn accounts.
        SetLogSegment(LogSegment::kSupervisionEnabledByPolicy);
      }
      return;
    }
  }
}

// static
const char* FamilyLinkUserMetricsProvider::GetHistogramNameForTesting() {
  return kFamilyLinkUserLogSegmentHistogramName;
}

void FamilyLinkUserMetricsProvider::SetLogSegment(LogSegment log_segment) {
  log_segment_ = log_segment;
}
