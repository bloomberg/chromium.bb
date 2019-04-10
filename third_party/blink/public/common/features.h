// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {
namespace features {

BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingDownloadsInAdFrameWithoutUserActivation;
BLINK_COMMON_EXPORT extern const base::Feature kAvoidFlashBetweenNavigation;
BLINK_COMMON_EXPORT extern const base::Feature
    kEagerCacheStorageSetupForServiceWorkers;
BLINK_COMMON_EXPORT extern const base::Feature
    kEnableGpuRasterizationViewportRestriction;
BLINK_COMMON_EXPORT extern const base::Feature kScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kFirstContentfulPaintPlusPlus;
BLINK_COMMON_EXPORT extern const base::Feature kFreezePurgeMemoryAllPagesFrozen;
BLINK_COMMON_EXPORT extern const base::Feature kImplicitRootScroller;
BLINK_COMMON_EXPORT extern const base::Feature kInvalidateFontCacheOnPurge;
BLINK_COMMON_EXPORT extern const base::Feature kJankTrackingSweepLine;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkGenPropertyTrees;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNG;
BLINK_COMMON_EXPORT extern const base::Feature kMixedContentAutoupgrade;
BLINK_COMMON_EXPORT extern const base::Feature kMojoBlobURLs;
BLINK_COMMON_EXPORT extern const base::Feature kNavigationPredictor;
BLINK_COMMON_EXPORT extern const base::Feature
    kOffMainThreadDedicatedWorkerScriptFetch;
BLINK_COMMON_EXPORT extern const base::Feature
    kOffMainThreadServiceWorkerScriptFetch;
BLINK_COMMON_EXPORT extern const base::Feature
    kOffMainThreadSharedWorkerScriptFetch;
BLINK_COMMON_EXPORT extern const base::Feature kOnionSoupDOMStorage;
BLINK_COMMON_EXPORT extern const base::Feature kPlzDedicatedWorker;
BLINK_COMMON_EXPORT extern const base::Feature kPortals;
BLINK_COMMON_EXPORT extern const base::Feature
    kPreviewsResourceLoadingHintsSpecificResourceTypes;
BLINK_COMMON_EXPORT extern const base::Feature
    kPurgeRendererMemoryWhenBackgrounded;
BLINK_COMMON_EXPORT extern const base::Feature kRTCGetDisplayMedia;
BLINK_COMMON_EXPORT extern const base::Feature kRTCUnifiedPlanByDefault;
BLINK_COMMON_EXPORT extern const base::Feature kRTCOfferExtmapAllowMixed;
BLINK_COMMON_EXPORT extern const base::Feature kResourceLoadViaDataPipe;
BLINK_COMMON_EXPORT extern const base::Feature
    kServiceWorkerImportedScriptUpdateCheck;
BLINK_COMMON_EXPORT extern const base::Feature
    kServiceWorkerParallelSideDataReading;
BLINK_COMMON_EXPORT extern const base::Feature
    kServiceWorkerAggressiveCodeCache;
BLINK_COMMON_EXPORT extern const base::Feature kServiceWorkerUpdateDelay;
BLINK_COMMON_EXPORT extern const base::Feature kStopInBackground;
BLINK_COMMON_EXPORT extern const base::Feature
    kFreezeBackgroundTabOnNetworkIdle;
BLINK_COMMON_EXPORT extern const base::Feature kStopNonTimersInBackground;
BLINK_COMMON_EXPORT extern const base::Feature kTextFragmentAnchor;
BLINK_COMMON_EXPORT extern const base::Feature kWasmCodeCache;
BLINK_COMMON_EXPORT extern const base::Feature kNativeFilesystemAPI;
BLINK_COMMON_EXPORT extern const base::Feature kForbidSyncXHRInPageDismissal;
BLINK_COMMON_EXPORT extern const base::Feature
    kRestrictDeviceSensorEventsToSecureContexts;

BLINK_COMMON_EXPORT extern const char kMixedContentAutoupgradeModeParamName[];
BLINK_COMMON_EXPORT extern const char kMixedContentAutoupgradeModeBlockable[];
BLINK_COMMON_EXPORT extern const char
    kMixedContentAutoupgradeModeOptionallyBlockable[];

BLINK_COMMON_EXPORT extern const base::Feature kDecodeLossyWebPImagesToYUV;
BLINK_COMMON_EXPORT extern const base::Feature kAlwaysAccelerateCanvas;

BLINK_COMMON_EXPORT extern const base::Feature
    kWebFontsCacheAwareTimeoutAdaption;

// Returns true when off-the-main-thread shared worker script fetch is enabled.
BLINK_COMMON_EXPORT bool IsOffMainThreadSharedWorkerScriptFetchEnabled();

// Returns true when PlzDedicatedWorker is enabled.
BLINK_COMMON_EXPORT bool IsPlzDedicatedWorkerEnabled();

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
