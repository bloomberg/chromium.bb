// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FAVICON_REQUEST_METRICS_H_
#define COMPONENTS_FAVICON_BASE_FAVICON_REQUEST_METRICS_H_

// Header used for collecting metrics associated with favicon retrieval by UI.

namespace favicon {
// The UI origin of an icon request.
enum class FaviconRequestOrigin {
  // Unknown origin.
  UNKNOWN,
  // chrome://history.
  HISTORY,
  // chrome://history/syncedTabs.
  HISTORY_SYNCED_TABS,
  // Recently closed tabs menu.
  RECENTLY_CLOSED_TABS,
};

enum class FaviconAvailability {
  kLocal = 0,
  kSync = 1,
  kNotAvailable = 2,
  kMaxValue = kNotAvailable,
};

void RecordFaviconRequestMetric(favicon::FaviconRequestOrigin origin,
                                favicon::FaviconAvailability availability);

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_BASE_FAVICON_REQUEST_METRICS_H_
