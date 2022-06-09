// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_METRICS_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_METRICS_H_

// Enums for histograms:
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SideSearchAvailabilityChangeType {
  kBecomeAvailable = 0,
  kBecomeUnavailable = 1,
  kMaxValue = kBecomeUnavailable
};

enum class SideSearchOpenActionType {
  kTapOnSideSearchToolbarButton = 0,
  kMaxValue = kTapOnSideSearchToolbarButton
};

enum class SideSearchCloseActionType {
  kTapOnSideSearchToolbarButton = 0,
  kTapOnSideSearchCloseButton = 1,
  kMaxValue = kTapOnSideSearchCloseButton
};

enum class SideSearchNavigationType {
  kNavigationCommittedWithinSideSearch = 0,
  kRedirectionToTab = 1,
  kMaxValue = kRedirectionToTab
};
// End of enums for histograms.

void RecordSideSearchAvailabilityChanged(SideSearchAvailabilityChangeType type);
void RecordSideSearchOpenAction(SideSearchOpenActionType action);
void RecordSideSearchCloseAction(SideSearchCloseActionType action);
void RecordSideSearchNavigation(SideSearchNavigationType type);
void RecordNavigationCommittedWithinSideSearchCountPerJourney(int count);
void RecordRedirectionToTabCountPerJourney(int count);

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_METRICS_H_
