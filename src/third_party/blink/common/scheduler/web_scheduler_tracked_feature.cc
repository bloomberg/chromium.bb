// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

#include <map>

namespace blink {
namespace scheduler {

namespace {

struct FeatureNames {
  std::string short_name;
  std::string human_readable;
};

FeatureNames FeatureToNames(WebSchedulerTrackedFeature feature) {
  switch (feature) {
    case WebSchedulerTrackedFeature::kWebSocket:
      return {"WebSocket", "WebSocket"};
    case WebSchedulerTrackedFeature::kWebTransport:
      return {"WebTransport", "WebTransport"};
    case WebSchedulerTrackedFeature::kWebRTC:
      return {"WebRTC", "WebRTC"};
    case WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoCache:
      return {"MainResourceHasCacheControlNoCache",
              "main resource has Cache-Control: No-Cache"};
    case WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore:
      return {"MainResourceHasCacheControlNoStore",
              "main resource has Cache-Control: No-Store"};
    case WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoCache:
      return {"SubresourceHasCacheControlNoCache",
              "subresource has Cache-Control: No-Cache"};
    case WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoStore:
      return {"SubresourceHasCacheControlNoStore",
              "subresource has Cache-Control: No-Store"};
    case WebSchedulerTrackedFeature::kContainsPlugins:
      return {"ContainsPlugins", "page contains plugins"};
    case WebSchedulerTrackedFeature::kDocumentLoaded:
      return {"DocumentLoaded", "document loaded"};
    case WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet:
      return {"DedicatedWorkerOrWorklet",
              "Dedicated worker or worklet present"};
    case WebSchedulerTrackedFeature::kSharedWorker:
      return {"SharedWorker", "Shared worker present"};
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestFetch:
      return {"OutstandingNetworkRequestFetch",
              "outstanding network request (fetch)"};
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestXHR:
      return {"OutstandingNetworkRequestXHR",
              "outstanding network request (XHR)"};
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestOthers:
      return {"OutstandingNetworkRequestOthers",
              "outstanding network request (others)"};
    case WebSchedulerTrackedFeature::kOutstandingIndexedDBTransaction:
      return {"OutstandingIndexedDBTransaction",
              "outstanding IndexedDB transaction"};
    case WebSchedulerTrackedFeature::kRequestedNotificationsPermission:
      return {"RequestedNotificationsPermission",
              "requested notifications permission"};
    case WebSchedulerTrackedFeature::kRequestedMIDIPermission:
      return {"RequestedMIDIPermission", "requested midi permission"};
    case WebSchedulerTrackedFeature::kRequestedAudioCapturePermission:
      return {"RequestedAudioCapturePermission",
              "requested audio capture permission"};
    case WebSchedulerTrackedFeature::kRequestedVideoCapturePermission:
      return {"RequestedVideoCapturePermission",
              "requested video capture permission"};
    case WebSchedulerTrackedFeature::kRequestedBackForwardCacheBlockedSensors:
      return {"RequestedBackForwardCacheBlockedSensors",
              "requested sensors permission"};
    case WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission:
      return {"RequestedBackgroundWorkPermission",
              "requested background work permission"};
    case WebSchedulerTrackedFeature::kBroadcastChannel:
      return {"BroadcastChannel", "requested broadcast channel permission"};
    case WebSchedulerTrackedFeature::kIndexedDBConnection:
      return {"IndexedDBConnection", "IndexedDB connection present"};
    case WebSchedulerTrackedFeature::kWebXR:
      return {"WebXR", "WebXR"};
    case WebSchedulerTrackedFeature::kWebLocks:
      return {"WebLocks", "WebLocks"};
    case WebSchedulerTrackedFeature::kWebHID:
      return {"WebHID", "WebHID"};
    case WebSchedulerTrackedFeature::kWebShare:
      return {"WebShare", "WebShare"};
    case WebSchedulerTrackedFeature::kRequestedStorageAccessGrant:
      return {"RequestedStorageAccessGrant",
              "requested storage access permission"};
    case WebSchedulerTrackedFeature::kWebNfc:
      return {"WebNfc", "WebNfc"};
    case WebSchedulerTrackedFeature::kAppBanner:
      return {"AppBanner", "AppBanner"};
    case WebSchedulerTrackedFeature::kPrinting:
      return {"Printing", "Printing"};
    case WebSchedulerTrackedFeature::kWebDatabase:
      return {"WebDatabase", "WebDatabase"};
    case WebSchedulerTrackedFeature::kPictureInPicture:
      return {"PictureInPicture", "PictureInPicture"};
    case WebSchedulerTrackedFeature::kPortal:
      return {"Portal", "Portal"};
    case WebSchedulerTrackedFeature::kSpeechRecognizer:
      return {"SpeechRecognizer", "SpeechRecognizer"};
    case WebSchedulerTrackedFeature::kIdleManager:
      return {"IdleManager", "IdleManager"};
    case WebSchedulerTrackedFeature::kPaymentManager:
      return {"PaymentManager", "PaymentManager"};
    case WebSchedulerTrackedFeature::kSpeechSynthesis:
      return {"SpeechSynthesis", "SpeechSynthesis"};
    case WebSchedulerTrackedFeature::kKeyboardLock:
      return {"KeyboardLock", "KeyboardLock"};
    case WebSchedulerTrackedFeature::kWebOTPService:
      return {"WebOTPService", "SMSService"};
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestDirectSocket:
      return {"OutstandingNetworkRequestDirectSocket",
              "outstanding network request (direct socket)"};
    case WebSchedulerTrackedFeature::kInjectedJavascript:
      return {"InjectedJavascript", "External javascript injected"};
    case WebSchedulerTrackedFeature::kInjectedStyleSheet:
      return {"InjectedStyleSheet", "External systesheet injected"};
    case WebSchedulerTrackedFeature::kDummy:
      return {"Dummy", "Dummy for testing"};
  }
  return {};
}

std::map<std::string, WebSchedulerTrackedFeature> MakeShortNameToFeature() {
  std::map<std::string, WebSchedulerTrackedFeature> short_name_to_feature;
  for (int i = 0; i <= static_cast<int>(WebSchedulerTrackedFeature::kMaxValue);
       i++) {
    WebSchedulerTrackedFeature feature =
        static_cast<WebSchedulerTrackedFeature>(i);
    FeatureNames strs = FeatureToNames(feature);
    if (strs.short_name.size())
      short_name_to_feature[strs.short_name] = feature;
  }
  return short_name_to_feature;
}

const std::map<std::string, WebSchedulerTrackedFeature>&
ShortStringToFeatureMap() {
  static const std::map<std::string, WebSchedulerTrackedFeature>
      short_name_to_feature = MakeShortNameToFeature();
  return short_name_to_feature;
}

}  // namespace

std::string FeatureToHumanReadableString(WebSchedulerTrackedFeature feature) {
  return FeatureToNames(feature).human_readable;
}

absl::optional<WebSchedulerTrackedFeature> StringToFeature(
    const std::string& str) {
  auto map = ShortStringToFeatureMap();
  auto it = map.find(str);
  if (it == map.end()) {
    return absl::nullopt;
  }
  return it->second;
}

bool IsFeatureSticky(WebSchedulerTrackedFeature feature) {
  return StickyFeatures().Has(feature);
}

WebSchedulerTrackedFeatures StickyFeatures() {
  constexpr WebSchedulerTrackedFeatures features = WebSchedulerTrackedFeatures(
      WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore,
      WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoCache,
      WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoStore,
      WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoCache,
      WebSchedulerTrackedFeature::kContainsPlugins,
      WebSchedulerTrackedFeature::kDocumentLoaded,
      WebSchedulerTrackedFeature::kRequestedNotificationsPermission,
      WebSchedulerTrackedFeature::kRequestedMIDIPermission,
      WebSchedulerTrackedFeature::kRequestedAudioCapturePermission,
      WebSchedulerTrackedFeature::kRequestedVideoCapturePermission,
      WebSchedulerTrackedFeature::kRequestedBackForwardCacheBlockedSensors,
      WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission,
      WebSchedulerTrackedFeature::kWebLocks,
      WebSchedulerTrackedFeature::kRequestedStorageAccessGrant,
      WebSchedulerTrackedFeature::kWebNfc,
      WebSchedulerTrackedFeature::kAppBanner,
      WebSchedulerTrackedFeature::kPrinting,
      WebSchedulerTrackedFeature::kPictureInPicture,
      WebSchedulerTrackedFeature::kIdleManager,
      WebSchedulerTrackedFeature::kPaymentManager,
      WebSchedulerTrackedFeature::kWebOTPService,
      WebSchedulerTrackedFeature::kInjectedJavascript,
      WebSchedulerTrackedFeature::kInjectedStyleSheet,
      WebSchedulerTrackedFeature::kDummy);
  return features;
}

}  // namespace scheduler
}  // namespace blink
