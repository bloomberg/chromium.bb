// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SCHEDULER_WEB_SCHEDULER_TRACKED_FEATURE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SCHEDULER_WEB_SCHEDULER_TRACKED_FEATURE_H_

namespace blink {
namespace scheduler {

// A list of features which influence scheduling behaviour (throttling /
// freezing / back-forward cache) and which might be sent to the browser process
// for metrics-related purposes.
enum class WebSchedulerTrackedFeature {
  kWebSocket = 0,
  kWebRTC = 1,

  kMainResourceHasCacheControlNoCache = 2,
  kMainResourceHasCacheControlNoStore = 3,
  kSubresourceHasCacheControlNoCache = 4,
  kSubresourceHasCacheControlNoStore = 5,

  kPageShowEventListener = 6,
  kPageHideEventListener = 7,
  kBeforeUnloadEventListener = 8,
  kUnloadEventListener = 9,
  kFreezeEventListener = 10,
  kResumeEventListener = 11,

  kContainsPlugins = 12,
  kDocumentLoaded = 13,
  kDedicatedWorkerOrWorklet = 14,
  kOutstandingNetworkRequest = 15,
  // TODO(altimin): This doesn't include service worker-controlled origins.
  // We need to track them too.
  kServiceWorkerControlledPage = 16,

  kMaxValue = kServiceWorkerControlledPage
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SCHEDULER_WEB_SCHEDULER_TRACKED_FEATURE_H_
