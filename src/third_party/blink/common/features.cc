// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"

namespace blink {
namespace features {

// Enable intervention for download that was initiated from or occurred in an ad
// frame without user activation.
const base::Feature kBlockingDownloadsInAdFrameWithoutUserActivation{
    "BlockingDownloadsInAdFrameWithoutUserActivation",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Support COEP on SharedWorker.
const base::Feature kCOEPForSharedWorker{"COEPForSharedWorker",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCOLRV1Fonts{"COLRV1Fonts",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// Enable CSS Container Queries. Also implies LayoutNGGrid and CSSContainSize1D.
const base::Feature kCSSContainerQueries{"CSSContainerQueries",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the Conversion Measurement API infrastructure is enabled.
const base::Feature kConversionMeasurement{"ConversionMeasurement",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kGMSCoreEmoji{"GMSCoreEmoji",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Whether the HandwritingRecognition API can be enabled by origin trial.
// Disabling this feature disables both the origin trial and the mojo interface.
const base::Feature kHandwritingRecognitionWebPlatformApiFinch{
    "HandwritingRecognitionWebPlatformApiFinch",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable defer commits to avoid flash of unstyled content, for same origin
// navigation only.
const base::Feature kPaintHolding{"PaintHolding",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enable defer commits to avoid flash of unstyled content, for all navigation.
const base::Feature kPaintHoldingCrossOrigin{"PaintHoldingCrossOrigin",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enable eagerly setting up a CacheStorage interface pointer and
// passing it to service workers on startup as an optimization.
// TODO(crbug/1077916): Re-enable once the issue with COOP/COEP is fixed.
const base::Feature kEagerCacheStorageSetupForServiceWorkers{
    "EagerCacheStorageSetupForServiceWorkers",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls script streaming.
const base::Feature kScriptStreaming{"ScriptStreaming",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Allow streaming small (<30kB) scripts.
const base::Feature kSmallScriptStreaming{"SmallScriptStreaming",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables user level memory pressure signal generation on Android.
const base::Feature kUserLevelMemoryPressureSignal{
    "UserLevelMemoryPressureSignal", base::FEATURE_DISABLED_BY_DEFAULT};

// Perform memory purges after freezing only if all pages are frozen.
const base::Feature kFreezePurgeMemoryAllPagesFrozen{
    "FreezePurgeMemoryAllPagesFrozen", base::FEATURE_DISABLED_BY_DEFAULT};

// Freezes the user-agent as part of https://github.com/WICG/ua-client-hints.
const base::Feature kReduceUserAgent{"ReduceUserAgent",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the frequency capping for detecting overlay popups. Overlay-popups
// are the interstitials that pop up and block the main content of the page.
const base::Feature kFrequencyCappingForOverlayPopupDetection{
    "FrequencyCappingForOverlayPopupDetection",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the frequency capping for detecting large sticky ads.
// Large-sticky-ads are those ads that stick to the bottom of the page
// regardless of a user’s efforts to scroll, and take up more than 30% of the
// screen’s real estate.
const base::Feature kFrequencyCappingForLargeStickyAdDetection{
    "FrequencyCappingForLargeStickyAdDetection",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable Display Locking JavaScript APIs.
const base::Feature kDisplayLocking{"DisplayLocking",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kJSONModules{"JSONModules",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kForceSynchronousHTMLParsing{
    "ForceSynchronousHTMLParsing", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables top-level await in modules.
const base::Feature kTopLevelAwait{"TopLevelAwait",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Enable EditingNG by default. This feature is for a kill switch.
const base::Feature kEditingNG{"EditingNG", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable LayoutNG.
const base::Feature kLayoutNG{"LayoutNG", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable TableNG
const base::Feature kLayoutNGTable{"LayoutNGTable",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMixedContentAutoupgrade{"AutoupgradeMixedContent",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Used to control the collection of anchor element metrics (crbug.com/856683).
// If kNavigationPredictor is enabled, then metrics of anchor elements
// in the first viewport after the page load and the metrics of the clicked
// anchor element will be extracted and recorded. Additionally, navigation
// predictor may preconnect/prefetch to resources/origins to make the
// future navigations faster.
const base::Feature kNavigationPredictor {
  "NavigationPredictor",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// This feature returns empty arrays for navigator.plugins and
// navigator.mimeTypes. It is disabled for now, due to compat issues.
const base::Feature kNavigatorPluginsEmpty{"NavigatorPluginsEmpty",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enable browser-initiated dedicated worker script loading
// (PlzDedicatedWorker). https://crbug.com/906991
const base::Feature kPlzDedicatedWorker{"PlzDedicatedWorker",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Portals. https://crbug.com/865123.
// Note that default enabling this does not expose the portal
// element on its own, but does allow its use with an origin trial. This was the
// case for the M85 Android only origin trial (https://crbug.com/1040212).
const base::Feature kPortals {
  "Portals",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// When kPortals is enabled, allow portals to load content that is third-party
// (cross-origin) to the hosting page. Otherwise has no effect.
//
// https://crbug.com/1013389
const base::Feature kPortalsCrossOrigin{"PortalsCrossOrigin",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the <fencedframe> element; see crbug.com/1123606. Note that enabling
// this feature does not automatically expose this element to the web, it only
// allows the element to be enabled by the runtime enabled feature, for origin
// trials.
const base::Feature kFencedFrames{"FencedFrames",
                                  base::FEATURE_ENABLED_BY_DEFAULT};
const base::FeatureParam<FencedFramesImplementationType>::Option
    fenced_frame_implementation_types[] = {
        {FencedFramesImplementationType::kShadowDOM, "shadow_dom"},
        {FencedFramesImplementationType::kMPArch, "mparch"}};
const base::FeatureParam<FencedFramesImplementationType>
    kFencedFramesImplementationTypeParam{
        &kFencedFrames, "implementation_type",
        FencedFramesImplementationType::kShadowDOM,
        &fenced_frame_implementation_types};

// Enable the prerender2. https://crbug.com/1126305.
// Note that default enabling this does not enable the Prerender2 features
// because kSetOnlyIfOverridden is used for setting WebRuntimeFeatures'
// Prerender2. To enable this feature, we need to force-enable this feature
// using chrome://flags/#enable-prerender2 or --enable-features=Prerender2
// command line or a valid Origin Trial token in the page after default-enabling
// this feature.
const base::Feature kPrerender2{"Prerender2",
                                base::FEATURE_DISABLED_BY_DEFAULT};

bool IsPrerender2Enabled() {
  return base::FeatureList::IsEnabled(blink::features::kPrerender2);
}

bool IsFencedFramesEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kFencedFrames);
}

// Enable limiting previews loading hints to specific resource types.
const base::Feature kPreviewsResourceLoadingHintsSpecificResourceTypes{
    "PreviewsResourceLoadingHintsSpecificResourceTypes",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Perform a memory purge after a renderer is backgrounded. Formerly labelled as
// the "PurgeAndSuspend" experiment.
//
// TODO(https://crbug.com/926186): Disabled by default on Android for historical
// reasons. Consider enabling by default if experiment results are positive.
const base::Feature kPurgeRendererMemoryWhenBackgrounded {
  "PurgeRendererMemoryWhenBackgrounded",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enables Raw Clipboard. https://crbug.com/897289.
const base::Feature kRawClipboard{"RawClipboard",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for getCurrentBrowsingContextMedia(), which allows capturing of
// web content from the tab from which it is called. (crbug.com/1136940)
const base::Feature kRTCGetCurrentBrowsingContextMedia{
    "RTCGetCurrentBrowsingContextMedia", base::FEATURE_ENABLED_BY_DEFAULT};

// Changes the default RTCPeerConnection constructor behavior to use Unified
// Plan as the SDP semantics. When the feature is enabled, Unified Plan is used
// unless the default is overridden (by passing {sdpSemantics:'plan-b'} as the
// argument). This was shipped in M72.
// The feature is still used by virtual test suites exercising Plan B.
const base::Feature kRTCUnifiedPlanByDefault{"RTCUnifiedPlanByDefault",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
// When enabled, throw an exception when an RTCPeerConnection is constructed
// with {sdpSemantics:"plan-b"} and the Deprecation Trial is not enabled.
const base::Feature kRTCDisallowPlanBOutsideDeprecationTrial{
    "RTCDisallowPlanBOutsideDeprecationTrial",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Determines if the SDP attrbute extmap-allow-mixed should be offered by
// default or not. The default value can be overridden by passing
// {offerExtmapAllowMixed:false} as an argument to the RTCPeerConnection
// constructor.
const base::Feature kRTCOfferExtmapAllowMixed{"RTCOfferExtmapAllowMixed",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables waiting for codec support status notification from GPU factory in RTC
// codec factories.
const base::Feature kRTCGpuCodecSupportWaiter{"kRTCGpuCodecSupportWaiter",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<int> kRTCGpuCodecSupportWaiterTimeoutParam{
    &kRTCGpuCodecSupportWaiter, "timeout_ms", 3000};

// Prevents workers from sending IsolateInBackgroundNotification to V8
// and thus instructs V8 to favor performance over memory on workers.
const base::Feature kV8OptimizeWorkersForPerformance{
    "V8OptimizeWorkersForPerformance", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the implementation of the performance.measureMemory
// web API uses PerformanceManager or not.
const base::Feature kWebMeasureMemoryViaPerformanceManager{
    "WebMeasureMemoryViaPerformanceManager", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables negotiation of experimental multiplex codec in SDP.
const base::Feature kWebRtcMultiplexCodec{"WebRTC-MultiplexCodec",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Causes WebRTC to replace host ICE candidate IP addresses with generated
// names ending in ".local" and resolve them using mDNS.
// http://crbug.com/878465
const base::Feature kWebRtcHideLocalIpsWithMdns{
    "WebRtcHideLocalIpsWithMdns", base::FEATURE_ENABLED_BY_DEFAULT};

// Causes WebRTC to not set the color space of video frames on the receive side
// in case it's unspecified. Otherwise we will guess that the color space is
// BT709. http://crbug.com/1129243
const base::Feature kWebRtcIgnoreUnspecifiedColorSpace{
    "WebRtcIgnoreUnspecifiedColorSpace", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, wake ups from throttleable TaskQueues are limited to 1 per
// minute in a page that has been backgrounded for 5 minutes.
//
// Intensive wake up throttling is enforced in addition to other throttling
// mechanisms:
//  - 1 wake up per second in a background page or hidden cross-origin frame
//  - 1% CPU time in a page that has been backgrounded for 10 seconds
//
// Feature tracking bug: https://crbug.com/1075553
//
// The base::Feature should not be read from; rather the provided accessors
// should be used, which also take into account the managed policy override of
// the feature.
//
// The base::Feature is enabled by default on all platforms. However, on
// Android, it has no effect because page freezing kicks in at the same time. It
// would have an effect if the grace period ("grace_period_seconds" param) was
// reduced.
const base::Feature kIntensiveWakeUpThrottling{
    "IntensiveWakeUpThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Name of the parameter that controls the grace period during which there is no
// intensive wake up throttling after a page is hidden. Defined here to allow
// access from about_flags.cc. The FeatureParam is defined in
// third_party/blink/renderer/platform/scheduler/common/features.cc.
const char kIntensiveWakeUpThrottling_GracePeriodSeconds_Name[] =
    "grace_period_seconds";

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
// Run-time feature for the |rtc_use_h264| encoder/decoder.
const base::Feature kWebRtcH264WithOpenH264FFmpeg{
    "WebRTC-H264WithOpenH264FFmpeg", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

// Experiment of the delay from navigation to starting an update of a service
// worker's script.
const base::Feature kServiceWorkerUpdateDelay{
    "ServiceWorkerUpdateDelay", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the use of Speculation Rules in access the private prefetch proxy
// (chrome/browser/prefetch/prefetch_proxy/).
// https://crbug.com/1190167
const base::Feature kSpeculationRulesPrefetchProxy{
    "SpeculationRulesPrefetchProxy", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Freeze scheduler task queues in background on network idle.
// This feature only works if stop-in-background is enabled.
const base::Feature kFreezeBackgroundTabOnNetworkIdle{
    "freeze-background-tab-on-network-idle", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the Storage Access API. https://crbug.com/989663.
const base::Feature kStorageAccessAPI{"StorageAccessAPI",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable text snippets in URL fragments. https://crbug.com/919204.
const base::Feature kTextFragmentAnchor{"TextFragmentAnchor",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// File handling integration. https://crbug.com/829689
const base::Feature kFileHandlingAPI{"FileHandlingAPI",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// File handling icons. https://crbug.com/1218213
const base::Feature kFileHandlingIcons{"FileHandlingIcons",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Allows for synchronous XHR requests during page dismissal
const base::Feature kAllowSyncXHRInPageDismissal{
    "AllowSyncXHRInPageDismissal", base::FEATURE_DISABLED_BY_DEFAULT};

// Font enumeration and data access. https://crbug.com/535764
const base::Feature kFontAccess{"FontAccess",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Font enumeration and data access. https://crbug.com/1173275
const base::Feature kFontAccessPersistent{"FontAccessPersistent",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for the Compute Pressure API. https://crbug.com/1067627
const base::Feature kComputePressure{"ComputePressure",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Prefetch request properties are updated to be privacy-preserving. See
// crbug.com/988956.
const base::Feature kPrefetchPrivacyChanges{"PrefetchPrivacyChanges",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Decodes jpeg 4:2:0 formatted images to YUV instead of RGBX and stores in this
// format in the image decode cache. See crbug.com/919627 for details on the
// feature.
const base::Feature kDecodeJpeg420ImagesToYUV{"DecodeJpeg420ImagesToYUV",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Decodes lossy WebP images to YUV instead of RGBX and stores in this format
// in the image decode cache. See crbug.com/900264 for details on the feature.
const base::Feature kDecodeLossyWebPImagesToYUV{
    "DecodeLossyWebPImagesToYUV", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables cache-aware WebFonts loading. See https://crbug.com/570205.
// The feature is disabled on Android for WebView API issue discussed at
// https://crbug.com/942440.
const base::Feature kWebFontsCacheAwareTimeoutAdaption {
  "WebFontsCacheAwareTimeoutAdaption",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enabled to block programmatic focus in subframes when not triggered by user
// activation (see htpps://crbug.com/954349).
const base::Feature kBlockingFocusWithoutUserActivation{
    "BlockingFocusWithoutUserActivation", base::FEATURE_DISABLED_BY_DEFAULT};

// A server-side switch for the REALTIME_AUDIO thread priority of
// RealtimeAudioWorkletThread object. When disabled, it will use the NORMAL
// priority thread.
const base::Feature kAudioWorkletThreadRealtimePriority{
    "AudioWorkletThreadRealtimePriority", base::FEATURE_DISABLED_BY_DEFAULT};

// A feature to reduce the set of resources fetched by No-State Prefetch.
const base::Feature kLightweightNoStatePrefetch {
  "LightweightNoStatePrefetch",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Automatically convert light-themed pages to use a Blink-generated dark theme
const base::Feature kForceWebContentsDarkMode{
    "WebContentsForceDark", base::FEATURE_DISABLED_BY_DEFAULT};

// A feature to enable using the smallest image specified within image srcset
// for users with Save Data enabled.
const base::Feature kSaveDataImgSrcset{"SaveDataImgSrcset",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Which algorithm should be used for color inversion?
const base::FeatureParam<ForceDarkInversionMethod>::Option
    forcedark_inversion_method_options[] = {
        {ForceDarkInversionMethod::kUseBlinkSettings,
         "use_blink_settings_for_method"},
        {ForceDarkInversionMethod::kHslBased, "hsl_based"},
        {ForceDarkInversionMethod::kCielabBased, "cielab_based"},
        {ForceDarkInversionMethod::kRgbBased, "rgb_based"}};

const base::FeatureParam<ForceDarkInversionMethod>
    kForceDarkInversionMethodParam{&kForceWebContentsDarkMode,
                                   "inversion_method",
                                   ForceDarkInversionMethod::kUseBlinkSettings,
                                   &forcedark_inversion_method_options};

// Should images be inverted?
const base::FeatureParam<ForceDarkImageBehavior>::Option
    forcedark_image_behavior_options[] = {
        {ForceDarkImageBehavior::kUseBlinkSettings,
         "use_blink_settings_for_images"},
        {ForceDarkImageBehavior::kInvertNone, "none"},
        {ForceDarkImageBehavior::kInvertSelectively, "selective"}};

const base::FeatureParam<ForceDarkImageBehavior> kForceDarkImageBehaviorParam{
    &kForceWebContentsDarkMode, "image_behavior",
    ForceDarkImageBehavior::kUseBlinkSettings,
    &forcedark_image_behavior_options};

// Do not invert text lighter than this.
// Range: 0 (do not invert any text) to 256 (invert all text)
// Can also set to -1 to let Blink's internal settings control the value
const base::FeatureParam<int> kForceDarkTextLightnessThresholdParam{
    &kForceWebContentsDarkMode, "text_lightness_threshold", -1};

// Do not invert backgrounds darker than this.
// Range: 0 (invert all backgrounds) to 256 (invert no backgrounds)
// Can also set to -1 to let Blink's internal settings control the value
const base::FeatureParam<int> kForceDarkBackgroundLightnessThresholdParam{
    &kForceWebContentsDarkMode, "background_lightness_threshold", -1};

const base::FeatureParam<ForceDarkIncreaseTextContrast>::Option
    forcedark_increase_text_contrast_options[] = {
        {ForceDarkIncreaseTextContrast::kUseBlinkSettings,
         "use_blink_settings_for_method"},
        {ForceDarkIncreaseTextContrast::kFalse, "false"},
        {ForceDarkIncreaseTextContrast::kTrue, "true"}};

// Should text contrast be increased.
const base::FeatureParam<ForceDarkIncreaseTextContrast>
    kForceDarkIncreaseTextContrastParam{
        &kForceWebContentsDarkMode, "increase_text_contrast",
        ForceDarkIncreaseTextContrast::kUseBlinkSettings,
        &forcedark_increase_text_contrast_options};

// Instructs WebRTC to honor the Min/Max Video Encode Accelerator dimensions.
const base::Feature kWebRtcUseMinMaxVEADimensions {
  "WebRtcUseMinMaxVEADimensions",
  // TODO(crbug.com/1008491): enable other platforms.
#if BUILDFLAG(IS_CHROMEOS_ASH)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Blink garbage collection.
// Enables compaction of backing stores on Blink's heap.
const base::Feature kBlinkHeapCompaction{"BlinkHeapCompaction",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
// Enables concurrently marking Blink's heap.
const base::Feature kBlinkHeapConcurrentMarking{
    "BlinkHeapConcurrentMarking", base::FEATURE_ENABLED_BY_DEFAULT};
// Enables concurrently sweeping Blink's heap.
const base::Feature kBlinkHeapConcurrentSweeping{
    "BlinkHeapConcurrentSweeping", base::FEATURE_ENABLED_BY_DEFAULT};
// Enables incrementally marking Blink's heap.
const base::Feature kBlinkHeapIncrementalMarking{
    "BlinkHeapIncrementalMarking", base::FEATURE_ENABLED_BY_DEFAULT};
// Enables a marking stress mode that schedules more garbage collections and
// also adds additional verification passes.
const base::Feature kBlinkHeapIncrementalMarkingStress{
    "BlinkHeapIncrementalMarkingStress", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables removing AppCache delays when triggering requests when the HTML was
// not fetched from AppCache.
const base::Feature kVerifyHTMLFetchedFromAppCacheBeforeDelay{
    "VerifyHTMLFetchedFromAppCacheBeforeDelay",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we use ThreadPriority::DISPLAY for renderer
// compositor & IO threads.
const base::Feature kBlinkCompositorUseDisplayThreadPriority {
  "BlinkCompositorUseDisplayThreadPriority",
#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Ignores cross origin windows in the named property interceptor of Window.
// https://crbug.com/538562
const base::Feature kIgnoreCrossOriginWindowWhenNamedAccessOnWindow{
    "IgnoreCrossOriginWindowWhenNamedAccessOnWindow",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables redirecting subresources in the page to better compressed and
// optimized versions to provide data savings.
const base::Feature kSubresourceRedirect{"SubresourceRedirect",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables redirecting src videos in the page to better compressed and optimized
// versions to provide data savings.
const base::Feature kSubresourceRedirectSrcVideo{
    "SubresourceRedirectSrcVideo", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, enforces new interoperable semantics for 3D transforms.
// See crbug.com/1008483.
const base::Feature kTransformInterop{"TransformInterop",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kBackfaceVisibilityInterop{
    "BackfaceVisibilityInterop", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, beacons (and friends) have ResourceLoadPriority::kLow,
// not ResourceLoadPriority::kVeryLow.
const base::Feature kSetLowPriorityForBeacon{"SetLowPriorityForBeacon",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled allows the header name used in the blink
// CacheStorageCodeCacheHint runtime feature to be modified.  This runtime
// feature disables generating full code cache for responses stored in
// cache_storage during a service worker install event.  The runtime feature
// must be enabled via the blink runtime feature mechanism, however.
const base::Feature kCacheStorageCodeCacheHintHeader{
    "CacheStorageCodeCacheHintHeader", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<std::string> kCacheStorageCodeCacheHintHeaderName{
    &kCacheStorageCodeCacheHintHeader, "name", "x-CacheStorageCodeCacheHint"};

// When enabled, the beforeunload handler is dispatched when a frame is frozen.
// This allows the browser to know whether discarding the frame could result in
// lost user data, at the cost of extra CPU usage. The feature will be removed
// once we have determine whether the CPU cost is acceptable.
const base::Feature kDispatchBeforeUnloadOnFreeze{
    "DispatchBeforeUnloadOnFreeze", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the use of GpuMemoryBuffer images for low latency 2d canvas.
// TODO(khushalsagar): Enable this if we're using SurfaceControl and GMBs allow
// us to overlay these resources.
const base::Feature kLowLatencyCanvas2dImageChromium {
  "LowLatencyCanvas2dImageChromium",
#if BUILDFLAG(IS_CHROMEOS_ASH)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // OS_CHROMEOS
};

// Enables the use of shared image swap chains for low latency 2d canvas.
const base::Feature kLowLatencyCanvas2dSwapChain{
    "LowLatencyCanvas2dSwapChain", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the use of shared image swap chains for low latency webgl canvas.
const base::Feature kLowLatencyWebGLSwapChain{"LowLatencyWebGLSwapChain",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Dawn-accelerated 2D canvas.
const base::Feature kDawn2dCanvas{"Dawn2dCanvas",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Enables small accelerated canvases for webview (crbug.com/1004304)
const base::Feature kWebviewAccelerateSmallCanvases{
    "WebviewAccelerateSmallCanvases", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, frees up CachedMetadata after consumption by script resources
// and modules. Needed for the experiment in http://crbug.com/1045052.
const base::Feature kDiscardCodeCacheAfterFirstUse{
    "DiscardCodeCacheAfterFirstUse", base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for the new element.offsetParent behavior.
// TODO(crbug.com/920069): Remove this once the feature has
// landed and no compat issues are reported.
const base::Feature kOffsetParentNewSpecBehavior{
    "OffsetParentNewSpecBehavior", base::FEATURE_DISABLED_BY_DEFAULT};

// Slightly delays rendering if there are fonts being preloaded, so that
// they don't miss the first paint if they can be loaded fast enough (e.g.,
// from the disk cache)
const base::Feature kFontPreloadingDelaysRendering{
    "FontPreloadingDelaysRendering", base::FEATURE_ENABLED_BY_DEFAULT};
// 50ms is the overall best performing value in our experiments.
const base::FeatureParam<int> kFontPreloadingDelaysRenderingParam{
    &kFontPreloadingDelaysRendering, "delay-in-ms", 50};

const base::Feature kKeepScriptResourceAlive{"KeepScriptResourceAlive",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kDelayAsyncScriptExecution{
    "DelayAsyncScriptExecution", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<DelayAsyncScriptDelayType>::Option
    delay_async_script_execution_delay_types[] = {
        {DelayAsyncScriptDelayType::kFinishedParsing, "finished_parsing"},
        {DelayAsyncScriptDelayType::kFirstPaintOrFinishedParsing,
         "first_paint_or_finished_parsing"},
        {DelayAsyncScriptDelayType::kUseOptimizationGuide,
         "use_optimization_guide"}};
const base::FeatureParam<DelayAsyncScriptDelayType>
    kDelayAsyncScriptExecutionDelayParam{
        &kDelayAsyncScriptExecution, "delay_type",
        DelayAsyncScriptDelayType::kFinishedParsing,
        &delay_async_script_execution_delay_types};

// Feature and parameters for delaying low priority requests behind "important"
// (either high or medium priority requests). There are two parameters
// highlighted below.
const base::Feature kDelayCompetingLowPriorityRequests{
    "DelayCompetingLowPriorityRequests", base::FEATURE_DISABLED_BY_DEFAULT};
// The delay type: We don't want to delay low priority requests behind
// "important" requests forever. Rather, it makes sense to have this behavior up
// *until* some relevant loading milestone, which this parameter specifies.
const base::FeatureParam<DelayCompetingLowPriorityRequestsDelayType>::Option
    delay_competing_low_priority_requests_delay_types[] = {
        {DelayCompetingLowPriorityRequestsDelayType::kFirstPaint,
         "first_paint"},
        {DelayCompetingLowPriorityRequestsDelayType::kFirstContentfulPaint,
         "first_contentful_paint"},
        {DelayCompetingLowPriorityRequestsDelayType::kAlways, "always"},
        {DelayCompetingLowPriorityRequestsDelayType::kUseOptimizationGuide,
         "use_optimization_guide"}};
const base::FeatureParam<DelayCompetingLowPriorityRequestsDelayType>
    kDelayCompetingLowPriorityRequestsDelayParam{
        &kDelayCompetingLowPriorityRequests, "until",
        DelayCompetingLowPriorityRequestsDelayType::kFirstContentfulPaint,
        &delay_competing_low_priority_requests_delay_types};
// The priority threshold: indicates which ResourceLoadPriority should be
// considered "important", such that low priority requests are delayed behind
// in-flight "important" requests.
const base::FeatureParam<DelayCompetingLowPriorityRequestsThreshold>::Option
    delay_competing_low_priority_requests_thresholds[] = {
        {DelayCompetingLowPriorityRequestsThreshold::kMedium, "medium"},
        {DelayCompetingLowPriorityRequestsThreshold::kHigh, "high"}};
const base::FeatureParam<DelayCompetingLowPriorityRequestsThreshold>
    kDelayCompetingLowPriorityRequestsThresholdParam{
        &kDelayCompetingLowPriorityRequests, "priority_threshold",
        DelayCompetingLowPriorityRequestsThreshold::kHigh,
        &delay_competing_low_priority_requests_thresholds};

// The AppCache feature is a kill-switch for the entire AppCache feature,
// both backend and API.  If disabled, then it will turn off the backend and
// api, regardless of the presence of valid origin trial tokens.  Disabling
// AppCache will also delete any AppCache data from the profile directory.
const base::Feature kAppCache{"AppCache", base::FEATURE_ENABLED_BY_DEFAULT};
// If AppCacheRequireOriginTrial is enabled, then the AppCache backend in the
// browser will require origin trial tokens in order to load or store manifests
// and their contents.
const base::Feature kAppCacheRequireOriginTrial{
    "AppCacheRequireOriginTrial", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the JPEG XL Image File Format (JXL).
const base::Feature kJXL{"JXL", base::FEATURE_DISABLED_BY_DEFAULT};

// Make all pending 'display: auto' web fonts enter the swap or failure period
// immediately before reaching the LCP time limit (~2500ms), so that web fonts
// do not become a source of bad LCP.
const base::Feature kAlignFontDisplayAutoTimeoutWithLCPGoal{
    "AlignFontDisplayAutoTimeoutWithLCPGoal", base::FEATURE_ENABLED_BY_DEFAULT};

// The amount of time allowed for 'display: auto' web fonts to load without
// intervention, counted from navigation start.
const base::FeatureParam<int>
    kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam{
        &kAlignFontDisplayAutoTimeoutWithLCPGoal, "lcp-limit-in-ms", 2000};

const base::FeatureParam<AlignFontDisplayAutoTimeoutWithLCPGoalMode>::Option
    align_font_display_auto_timeout_with_lcp_goal_modes[] = {
        {AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToFailurePeriod,
         "failure"},
        {AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToSwapPeriod, "swap"}};
const base::FeatureParam<AlignFontDisplayAutoTimeoutWithLCPGoalMode>
    kAlignFontDisplayAutoTimeoutWithLCPGoalModeParam{
        &kAlignFontDisplayAutoTimeoutWithLCPGoal, "intervention-mode",
        AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToSwapPeriod,
        &align_font_display_auto_timeout_with_lcp_goal_modes};

// Enable throttling of fetch() requests from service workers in the
// installing state.  The limit of 3 was chosen to match the limit
// in background main frames.  In addition, trials showed that this
// did not cause excessive timeouts and resulted in a net improvement
// in successful install rate on some platforms.
const base::Feature kThrottleInstallingServiceWorker{
    "ThrottleInstallingServiceWorker", base::FEATURE_ENABLED_BY_DEFAULT};
const base::FeatureParam<int> kInstallingServiceWorkerOutstandingThrottledLimit{
    &kThrottleInstallingServiceWorker, "limit", 3};

const base::Feature kInputPredictorTypeChoice{
    "InputPredictorTypeChoice", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kResamplingInputEvents{"ResamplingInputEvents",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kInputTargetClientHighPriority{
    "InputTargetClientHighPriority", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kResamplingScrollEvents{"ResamplingScrollEvents",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the device-memory, resource-width, viewport-width and DPR client
// hints to be sent to third-party origins if the first-party has opted in to
// receiving client hints, regardless of Permissions Policy.
const base::Feature kAllowClientHintsToThirdParty {
  "AllowClientHintsToThirdParty",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kFilteringScrollPrediction{
    "FilteringScrollPrediction", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kKalmanHeuristics{"KalmanHeuristics",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kKalmanDirectionCutOff{"KalmanDirectionCutOff",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSkipTouchEventFilter{"SkipTouchEventFilter",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
const char kSkipTouchEventFilterTypeParamName[] = "type";
const char kSkipTouchEventFilterTypeParamValueDiscrete[] = "discrete";
const char kSkipTouchEventFilterTypeParamValueAll[] = "all";
const char kSkipTouchEventFilterFilteringProcessParamName[] =
    "skip_filtering_process";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowser[] = "browser";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer[] =
    "browser_and_renderer";

// Disabling this will cause parkable strings to never be compressed.
// This is useful for headless mode + virtual time. Since virtual time advances
// quickly, strings may be parked too eagerly in that mode.
const base::Feature kCompressParkableStrings{"CompressParkableStrings",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Whether ParkableStrings can be written out to disk.
// Depends on compression above.
const base::Feature kParkableStringsToDisk{"ParkableStringsToDisk",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

bool IsParkableStringsToDiskEnabled() {
  return base::FeatureList::IsEnabled(kParkableStringsToDisk) &&
         base::FeatureList::IsEnabled(kCompressParkableStrings);
}

// Controls whether to auto select on contextual menu click in Chrome OS.
const base::Feature kCrOSAutoSelect{"CrOSAutoSelect",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCLSScrollAnchoring{"CLSScrollAnchoring",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Reduce the amount of information in the default 'referer' header for
// cross-origin requests.
const base::Feature kReducedReferrerGranularity{
    "ReducedReferrerGranularity", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the constant streaming in the ContentCapture task.
const base::Feature kContentCaptureConstantStreaming = {
    "ContentCaptureConstantStreaming", base::FEATURE_ENABLED_BY_DEFAULT};

// Dispatches a fake fetch event to a service worker to check the offline
// capability of the site before promoting installation.
// See https://crbug.com/965802 for more details.
const base::Feature kCheckOfflineCapability{"CheckOfflineCapability",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<CheckOfflineCapabilityMode>::Option
    check_offline_capability_types[] = {
        {CheckOfflineCapabilityMode::kWarnOnly, "warn_only"},
        {CheckOfflineCapabilityMode::kEnforce, "enforce"}};
const base::FeatureParam<CheckOfflineCapabilityMode>
    kCheckOfflineCapabilityParam{&kCheckOfflineCapability, "check_mode",
                                 CheckOfflineCapabilityMode::kWarnOnly,
                                 &check_offline_capability_types};

// The "BackForwardCacheABExperimentControl" feature indicates the state of the
// same-site BackForwardCache experiment. This information is used when sending
// the "Sec-bfcache-experiment" HTTP Header on resource requests. The header
// value is determined by the value of the "experiment_group_for_http_header"
// feature parameter.
const base::Feature kBackForwardCacheABExperimentControl{
    "BackForwardCacheABExperimentControl", base::FEATURE_DISABLED_BY_DEFAULT};
const char kBackForwardCacheABExperimentGroup[] =
    "experiment_group_for_http_header";

// Whether we should composite a PLSA (paint layer scrollable area) even if it
// means losing lcd text.
const base::Feature kPreferCompositingToLCDText = {
    "PreferCompositingToLCDText", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLogUnexpectedIPCPostedToBackForwardCachedDocuments{
    "LogUnexpectedIPCPostedToBackForwardCachedDocuments",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables web apps to request isolated storage.
const base::Feature kWebAppEnableIsolatedStorage{
    "WebAppEnableIsolatedStorage", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables declarative link capturing in web apps.
// Explainer:
// https://github.com/WICG/sw-launch/blob/master/declarative_link_capturing.md
const base::Feature kWebAppEnableLinkCapturing{
    "WebAppEnableLinkCapturing", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Unique ID feature in web apps. Controls parsing of "id" field in web
// app manifests. See explainer for more information:
// https://github.com/philloooo/pwa-unique-id
const base::Feature kWebAppEnableManifestId{"WebAppEnableManifestId",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Controls URL handling feature in web apps. Controls parsing of "url_handlers"
// field in web app manifests. See explainer for more information:
// https://github.com/WICG/pwa-url-handler/blob/master/explainer.md
const base::Feature kWebAppEnableUrlHandlers{"WebAppEnableUrlHandlers",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Protocol handling feature in web apps. Controls parsing of
// "protocol_handlers" field in web app manifests. See explainer for more
// information:
// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/URLProtocolHandler/explainer.md
const base::Feature kWebAppEnableProtocolHandlers{
    "WebAppEnableProtocolHandlers", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether web apps are able to be treated as note-taking apps. Controls parsing
// of "note_taking" dictionary field and "new_note_url" entry in web app
// manifests. Also controls whether the parsed field is used in browser. See
// incubation spec:
// https://wicg.github.io/manifest-incubations/#note_taking-member
const base::Feature kWebAppNoteTaking{"WebAppNoteTaking",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Makes network loading tasks unfreezable so that they can be processed while
// the page is frozen.
const base::Feature kLoadingTasksUnfreezable{"LoadingTasksUnfreezable",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Makes freezing of frame-associated task queues happen even when KeepActive is
// true.
const base::Feature kFreezeWhileKeepActive{"FreezeWhileKeepActive",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for the new behavior whereby anchors with target=_blank get
// noopener behavior by default. TODO(crbug.com/898942): Remove in Chrome 95.
const base::Feature kTargetBlankImpliesNoOpener{
    "TargetBlankImpliesNoOpener", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls how max frame rates are enforced in MediaStreamTracks.
// TODO(crbug.com/1152307): Remove in M91.
const base::Feature kMediaStreamTrackUseConfigMaxFrameRate{
    "MediaStreamTrackUseConfigMaxFrameRate", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, WebRTC's worker thread will run on a thread context distinct
// from the WebRTC signaling and network threads.
const base::Feature kWebRtcDistinctWorkerThread{
    "WebRtcDistinctWorkerThread", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the SubresourceFilter receives calls from the ResourceLoader
// to perform additional checks against any aliases found from DNS CNAME records
// for the requested URL.
const base::Feature kSendCnameAliasesToSubresourceFilterFromRenderer{
    "SendCnameAliasesToSubresourceFilterFromRenderer",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the declarative Shadow DOM feature.
const base::Feature kDeclarativeShadowDOM{"DeclarativeShadowDOM",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Kill switch for the InterestCohort API origin trial, i.e. if disabled, the
// API exposure will be disabled regardless of the OT config.
// (See https://github.com/WICG/floc.)
const base::Feature kInterestCohortAPIOriginTrial{
    "InterestCohortAPIOriginTrial", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the availability of the "interest-cohort" permissions policy.
const base::Feature kInterestCohortFeaturePolicy{
    "InterestCohortFeaturePolicy", base::FEATURE_DISABLED_BY_DEFAULT};

// Changes the default background color of the Text Fragment from
// bright yellow rgb(255, 255, 0) to light purple rgb(233, 210, 253)
const base::Feature kTextFragmentColorChange{"TextFragmentColorChange",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDisableDocumentDomainByDefault{
    "DisableDocumentDomainByDefault", base::FEATURE_DISABLED_BY_DEFAULT};

// Scopes the memory cache to a fetcher i.e. document/frame. Any resource cached
// in the blink cache will only be reused if the most recent fetcher that
// fetched it was the same as the current document.
const base::Feature kScopeMemoryCachePerContext{
    "ScopeMemoryCachePerContext", base::FEATURE_DISABLED_BY_DEFAULT};

// Allow image context menu selections to penetrate through transparent
// elements.
const base::Feature kEnablePenetratingImageSelection{
    "EnablePenetratingImageSelection", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, permits shared/root element transitions. See
// https://github.com/WICG/shared-element-transitions.
const base::Feature kDocumentTransition{"DocumentTransition",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
const base::Feature kBackgroundTracingPerformanceMark{
    "BackgroundTracingPerformanceMark", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<std::string>
    kBackgroundTracingPerformanceMark_AllowList{
        &kBackgroundTracingPerformanceMark, "allow_list", ""};

// New compositing algorithm. See renderer/core/paint/README.md.
const base::Feature kCompositeAfterPaint{"CompositeAfterPaint",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the Sanitizer API is available.
const base::Feature kSanitizerAPI{"SanitizerAPI",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for the blocking of the navigation of top from a cross origin
// iframe to a different protocol. TODO(https://crbug.com/1151507): Remove in
// M92.
const base::Feature kBlockCrossOriginTopNavigationToDiffentScheme{
    "BlockCrossOriginTopNavigationToDiffentScheme",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables a Web API for websites to access admin-provided configuration.
const base::Feature kManagedConfiguration{"ManagedConfiguration",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Causes all cross-origin iframes, both same-process and out-of-process, to
// have their rendering throttled on display:none or zero-area.
const base::Feature kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes{
    "ThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for the Fledge Interest Group API, i.e. if disabled, the
// API exposure will be disabled regardless of the OT config.
// (See https://github.com/WICG/turtledove/blob/main/FLEDGE.md.)
// Enables FLEDGE implementation. See https://crbug.com/1186444.
const base::Feature kFledgeInterestGroups{"FledgeInterestGroups",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the availability of the Fledge interest group API as part of the
// origin trial.
const base::Feature kFledgeInterestGroupAPI{"FledgeInterestGroupAPI",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the ability to minimize processing in the WebRTC APM when all audio
// tracks are disabled. If disabled, the APM in WebRTC will ignore attempts to
// set it in a low-processing mode when all audio tracks are disabled.
const base::Feature kMinimizeAudioProcessingForUnusedOutput{
    "MinimizeAudioProcessingForUnusedOutput",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When <dialog>s are closed, this focuses the "previously focused" element
// which had focus when the <dialog> was first opened.
// TODO(crbug.com/649162): Remove DialogFocusNewSpecBehavior after
// the feature is in stable with no issues.
const base::Feature kDialogFocusNewSpecBehavior{
    "DialogFocusNewSpecBehavior", base::FEATURE_ENABLED_BY_DEFAULT};

// Makes autofill look across shadow boundaries when collecting form controls to
// fill.
const base::Feature kAutofillShadowDOM{"AutofillShadowDOM",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
// Allows read/write of custom formats with unsanitized clipboard content. See
// crbug.com/106449.
const base::Feature kClipboardCustomFormats{"ClipboardCustomFormats",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
const base::Feature kUsePageViewportInLCP{"UsePageViewportInLCP",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enable `Sec-CH-UA-Platform` client hint and request header to be sent by
// default
const base::Feature kUACHPlatformEnabledByDefault{
    "UACHPlatformEnabledByDefault", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, allow dropping alpha on media streams for rendering sinks if
// other sinks connected do not use alpha.
const base::Feature kAllowDropAlphaForMediaStream{
    "AllowDropAlphaForMediaStream", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables partitioning of third party storage (IndexedDB, CacheStorage, etc.)
// by the top level site to reduce fingerprinting.
const base::Feature kThirdPartyStoragePartitioning{
    "ThirdPartyStoragePartitioning", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace blink
