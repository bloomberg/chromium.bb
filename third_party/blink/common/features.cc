// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"

#include "build/build_config.h"
#include "services/network/public/cpp/features.h"

namespace blink {
namespace features {

// Enable defer commits a bit to avoid flash.
const base::Feature kAvoidFlashBetweenNavigation{
    "AvoidFlashBetweenNavigation", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable eagerly setting up a CacheStorage interface pointer and
// passing it to service workers on startup as an optimization.
const base::Feature kEagerCacheStorageSetupForServiceWorkers{
    "EagerCacheStorageSetupForServiceWorkers",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls the user-specified viewport restriction for GPU Rasterization on
// mobile. See https://crbug.com/899399
const base::Feature kEnableGpuRasterizationViewportRestriction{
    "EnableGpuRasterizationViewportRestriction",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls script streaming.
const base::Feature kScriptStreaming{"ScriptStreaming",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enable FCP++ by experiment. See https://crbug.com/869924
const base::Feature kFirstContentfulPaintPlusPlus{
    "FirstContentfulPaintPlusPlus", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the experimental sweep-line algorithm for tracking "jank" from
// layout objects changing their visual location between animation frames.
const base::Feature kJankTrackingSweepLine{"JankTrackingSweepLine",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enable a new compositing mode called BlinkGenPropertyTrees where Blink
// generates the compositor property trees. See: https://crbug.com/836884.
const base::Feature kBlinkGenPropertyTrees{"BlinkGenPropertyTrees",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enable LayoutNG.
const base::Feature kLayoutNG{"LayoutNG", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMixedContentAutoupgrade{"AutoupgradeMixedContent",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enable mojo Blob URL interface and better blob URL lifetime management.
// Can be enabled independently of NetworkService.
const base::Feature kMojoBlobURLs{"MojoBlobURLs",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Used to control the collection of anchor element metrics (crbug.com/856683).
// If kNavigationPredictor is enabled, then metrics of anchor elements
// in the first viewport after the page load and the metrics of the clicked
// anchor element will be extracted and recorded. Additionally, navigation
// predictor may preconnect/prefetch to resources/origins to make the
// future navigations faster.
const base::Feature kNavigationPredictor{"NavigationPredictor",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enable off-the-main-thread dedicated worker script fetch.
// (https://crbug.com/835717)
const base::Feature kOffMainThreadDedicatedWorkerScriptFetch{
    "OffMainThreadDedicatedWorkerScriptFetch",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable off-the-main-thread service worker script fetch.
// (https://crbug.com/924043)
const base::Feature kOffMainThreadServiceWorkerScriptFetch{
    "OffMainThreadServiceWorkerScriptFetch", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable off-the-main-thread shared worker script fetch.
// (https://crbug.com/924041)
const base::Feature kOffMainThreadSharedWorkerScriptFetch{
    "OffMainThreadSharedWorkerScriptFetch", base::FEATURE_DISABLED_BY_DEFAULT};

// Onion souping for all DOMStorage. https://crbug.com/781870
const base::Feature kOnionSoupDOMStorage{"OnionSoupDOMStorage",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enable browser-initiated dedicated worker script loading
// (PlzDedicatedWorker). https://crbug.com/906991
const base::Feature kPlzDedicatedWorker{"PlzDedicatedWorker",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Portals. https://crbug.com/865123.
const base::Feature kPortals{"Portals", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable limiting previews loading hints to specific resource types.
const base::Feature kPreviewsResourceLoadingHintsSpecificResourceTypes{
    "PreviewsResourceLoadingHintsSpecificResourceTypes",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Implicit Root Scroller. https://crbug.com/903260.
const base::Feature kImplicitRootScroller{"ImplicitRootScroller",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables usage of getDisplayMedia() that allows capture of web content, see
// https://crbug.com/865060.
const base::Feature kRTCGetDisplayMedia{"RTCGetDisplayMedia",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Changes the default RTCPeerConnection constructor behavior to use Unified
// Plan as the SDP semantics. When the feature is enabled, Unified Plan is used
// unless the default is overridden (by passing {sdpSemantics:'plan-b'} as the
// argument).
const base::Feature kRTCUnifiedPlanByDefault{"RTCUnifiedPlanByDefault",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Determines if the SDP attrbute extmap-allow-mixed should be offered by
// default or not. The default value can be overridden by passing
// {offerExtmapAllowMixed:true} as an argument to the RTCPeerConnection
// constructor.
const base::Feature kRTCOfferExtmapAllowMixed{
    "RTCOfferExtmapAllowMixed", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables to load the response body through Mojo data pipe passed by
// WebURLLoaderClient::DidStartLoadingResponseBody() instead of
// WebURLLoaderClient::DidReceiveData().
const base::Feature kResourceLoadViaDataPipe{"ResourceLoadViaDataPipe",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kServiceWorkerImportedScriptUpdateCheck{
    "ServiceWorkerImportedScriptUpdateCheck",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables reading a subresource's body data and side data in parallel.
const base::Feature kServiceWorkerParallelSideDataReading{
    "ServiceWorkerParallelSideDataReading", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kServiceWorkerAggressiveCodeCache{
    "ServiceWorkerAggressiveCodeCache", base::FEATURE_DISABLED_BY_DEFAULT};

// Freeze scheduler task queues in background after allowed grace time.
// "stop" is a legacy name.
const base::Feature kStopInBackground {
  "stop-in-background",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Freeze non-timer task queues in background, after allowed grace time.
// "stop" is a legacy name.
const base::Feature kStopNonTimersInBackground {
  "stop-non-timers-in-background",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enable text snippets in URL fragments. https://crbug.com/919204.
const base::Feature kTextFragmentAnchor{"TextFragmentAnchor",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the site isolated Wasm code cache that is keyed on the resource URL
// and the origin lock of the renderer that is requesting the resource. When
// this flag is enabled, content/GeneratedCodeCache handles code cache requests.
const base::Feature kWasmCodeCache = {"WasmCodeCache",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Writable files and native filesystem access. https://crbug.com/853326
const base::Feature kNativeFilesystemAPI{"NativeFilesystemAPI",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Allows for synchronous XHR requests during page dismissal
const base::Feature kForbidSyncXHRInPageDismissal{
    "ForbidSyncXHRInPageDismissal", base::FEATURE_DISABLED_BY_DEFAULT};

// Emergency lever that can be used to restore DeviceOrientationEvent and
// DeviceMotionEvent functionality in non-secure browsing contexts.
// See: https://crbug.com/932078.
const base::Feature kRestrictDeviceSensorEventsToSecureContexts{
    "RestrictDeviceSensorEventsToSecureContexts",
    base::FEATURE_ENABLED_BY_DEFAULT};

const char kMixedContentAutoupgradeModeParamName[] = "mode";
const char kMixedContentAutoupgradeModeBlockable[] = "blockable";
const char kMixedContentAutoupgradeModeOptionallyBlockable[] =
    "optionally-blockable";

// Decodes lossy WebP images to YUV instead of RGBX and stores in this format
// in the image decode cache. See crbug.com/900264 for details on the feature.
const base::Feature kDecodeLossyWebPImagesToYUV{
    "DecodeLossyWebPImagesToYUV", base::FEATURE_DISABLED_BY_DEFAULT};

// Use accelerated canvases whenever possible see https://crbug.com/909937
const base::Feature kAlwaysAccelerateCanvas{"AlwaysAccelerateCanvas",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

bool IsOffMainThreadSharedWorkerScriptFetchEnabled() {
  // Off-the-main-thread shared worker script fetch depends on PlzSharedWorker
  // (NetworkService).
  DCHECK(!base::FeatureList::IsEnabled(
             features::kOffMainThreadSharedWorkerScriptFetch) ||
         base::FeatureList::IsEnabled(network::features::kNetworkService))
      << "OffMainThreadSharedWorkerScriptFetch is enabled but NetworkService "
      << "isn't. OffMainThreadSharedWorkerScriptFetch requires NetworkService.";
  return base::FeatureList::IsEnabled(network::features::kNetworkService) &&
         base::FeatureList::IsEnabled(
             features::kOffMainThreadSharedWorkerScriptFetch);
}

bool IsPlzDedicatedWorkerEnabled() {
  // PlzDedicatedWorker depends on off-the-main-thread dedicated worker script
  // fetch and NetworkService.
#if DCHECK_IS_ON()
  if (base::FeatureList::IsEnabled(features::kPlzDedicatedWorker)) {
    DCHECK(base::FeatureList::IsEnabled(
        features::kOffMainThreadDedicatedWorkerScriptFetch))
        << "PlzDedicatedWorker is enabled but "
        << "OffMainThreadDedicatedWorkerScriptFetch isn't. PlzDedicatedWorker "
        << "requires OffMainThreadDedicatedWorkerScriptFetch.";
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService))
        << "PlzDedicatedWorker is enabled but NetworkService isn't. "
        << "PlzDedicatedWorker requires NetworkService.";
  }
#endif  // DCHECK_IS_ON()
  return base::FeatureList::IsEnabled(
             features::kOffMainThreadDedicatedWorkerScriptFetch) &&
         base::FeatureList::IsEnabled(network::features::kNetworkService) &&
         base::FeatureList::IsEnabled(features::kPlzDedicatedWorker);
}

}  // namespace features
}  // namespace blink
