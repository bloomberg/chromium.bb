// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace send_tab_to_self {

namespace {

// State of the send tab to self option in the UI.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ClickResult {
  // kShowItem = 0,
  kClickItem = 1,
  // kShowDeviceList = 2,
  kMaxValue = kClickItem,
};

// Status of received STTS notifications.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with SendTabToSelfNotificationStatus in enums.xml.
enum class NotificationStatus {
  kShown = 0,
  kDismissed = 1,
  kOpened = 2,
  kTimedOut = 3,
  kSent = 4,
  kDismissReasonUnknown = 5,
  kMaxValue = kDismissReasonUnknown,
};

std::string GetEntryPointHistogramString(ShareEntryPoint entry_point) {
  switch (entry_point) {
    case ShareEntryPoint::kShareSheet:
      return "ShareSheet";
    case ShareEntryPoint::kOmniboxIcon:
      return "OmniboxIcon";
    case ShareEntryPoint::kContentMenu:
      return "ContentMenu";
    case ShareEntryPoint::kLinkMenu:
      return "LinkMenu";
    case ShareEntryPoint::kOmniboxMenu:
      return "OmniboxMenu";
    case ShareEntryPoint::kTabMenu:
      return "TabMenu";
    case ShareEntryPoint::kShareMenu:
      return "ShareMenu";
  }
}

}  // namespace

void RecordDeviceClicked(ShareEntryPoint entry_point) {
  // TODO(crbug.com/956722): Only kClickItem is used today, so we should replace
  // this with a histogram which doesn't use the ClickResult enum.
  base::UmaHistogramEnumeration(
      base::StrCat({"SendTabToSelf.", GetEntryPointHistogramString(entry_point),
                    ".ClickResult"}),
      ClickResult::kClickItem);
  RecordNotificationSent();
}

void RecordNotificationShown() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kShown);
}

void RecordNotificationDismissed() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kDismissed);
}

void RecordNotificationOpened() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kOpened);
}

void RecordNotificationTimedOut() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kTimedOut);
}

void RecordNotificationSent() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kSent);
}

void RecordNotificationDismissReasonUnknown() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kDismissReasonUnknown);
}

}  // namespace send_tab_to_self
