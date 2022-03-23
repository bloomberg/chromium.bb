// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"

namespace autofill {

namespace {

// Converts the VirtualCardEnrollmentRequestType to string to be used in
// histograms.
const char* GetVirtualCardEnrollmentRequestType(
    VirtualCardEnrollmentRequestType type) {
  switch (type) {
    case VirtualCardEnrollmentRequestType::kEnroll:
      return "Enroll";
    case VirtualCardEnrollmentRequestType::kUnenroll:
      return "Unenroll";
    case VirtualCardEnrollmentRequestType::kNone:
      return "Unknown";
  }
}

}  // namespace

// Converts the VirtualCardEnrollmentSource to string to be used in histograms.
const std::string VirtualCardEnrollmentSourceToMetricSuffix(
    VirtualCardEnrollmentSource source) {
  switch (source) {
    case VirtualCardEnrollmentSource::kUpstream:
      return "Upstream";
    case VirtualCardEnrollmentSource::kDownstream:
      return "Downstream";
    case VirtualCardEnrollmentSource::kSettingsPage:
      return "SettingsPage";
    case VirtualCardEnrollmentSource::kNone:
      return "Unknown";
  }
}

void LogVirtualCardEnrollmentBubbleShownMetric(
    VirtualCardEnrollmentBubbleSource source,
    bool is_reshow) {
  base::UmaHistogramBoolean(
      "Autofill.VirtualCardEnrollBubble.Shown." +
          VirtualCardEnrollmentBubbleSourceToMetricSuffix(source),
      is_reshow);
}

void LogVirtualCardEnrollmentBubbleResultMetric(
    VirtualCardEnrollmentBubbleResult result,
    VirtualCardEnrollmentBubbleSource source,
    bool is_reshow) {
  base::UmaHistogramEnumeration(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentBubbleSourceToMetricSuffix(source) +
          (is_reshow ? ".Reshows" : ".FirstShow"),
      result);
}

void LogGetDetailsForEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source) {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.VirtualCard.GetDetailsForEnrollment.Attempt.",
                    VirtualCardEnrollmentSourceToMetricSuffix(source)}),
      true);
}

void LogGetDetailsForEnrollmentRequestResult(VirtualCardEnrollmentSource source,
                                             bool succeeded) {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.VirtualCard.GetDetailsForEnrollment.Result.",
                    VirtualCardEnrollmentSourceToMetricSuffix(source)}),
      succeeded);
}

void LogUpdateVirtualCardEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type) {
  base::UmaHistogramBoolean(
      base::JoinString(
          {"Autofill.VirtualCard", GetVirtualCardEnrollmentRequestType(type),
           "Attempt", VirtualCardEnrollmentSourceToMetricSuffix(source)},
          "."),
      true);
}

void LogUpdateVirtualCardEnrollmentRequestResult(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type,
    bool succeeded) {
  base::UmaHistogramBoolean(
      base::JoinString(
          {"Autofill.VirtualCard", GetVirtualCardEnrollmentRequestType(type),
           "Result", VirtualCardEnrollmentSourceToMetricSuffix(source)},
          "."),
      succeeded);
}

std::string VirtualCardEnrollmentBubbleSourceToMetricSuffix(
    VirtualCardEnrollmentBubbleSource source) {
  switch (source) {
    case VirtualCardEnrollmentBubbleSource::
        VIRTUAL_CARD_ENROLLMENT_UNKNOWN_SOURCE:
      return "Unknown";
    case VirtualCardEnrollmentBubbleSource::
        VIRTUAL_CARD_ENROLLMENT_UPSTREAM_SOURCE:
      return "Upstream";
    case VirtualCardEnrollmentBubbleSource::
        VIRTUAL_CARD_ENROLLMENT_DOWNSTREAM_SOURCE:
      return "Downstream";
    case VirtualCardEnrollmentBubbleSource::
        VIRTUAL_CARD_ENROLLMENT_SETTINGS_PAGE_SOURCE:
      return "SettingsPage";
  }
}

}  // namespace autofill
