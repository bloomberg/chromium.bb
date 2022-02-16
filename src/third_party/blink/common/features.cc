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

// Apply lazy-loading to frames which have embeds likely impacting Core Web
// Vitals.
const base::Feature kAutomaticLazyFrameLoadingToEmbeds{
    "AutomaticLazyFrameLoadingToEmbeds", base::FEATURE_DISABLED_BY_DEFAULT};

// Allows pages with DedicatedWorker to stay eligible for the back/forward
// cache.
const base::Feature kBackForwardCacheDedicatedWorker{
    "BackForwardCacheDedicatedWorker", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable intervention for download that was initiated from or occurred in an ad
// frame without user activation.
const base::Feature kBlockingDownloadsInAdFrameWithoutUserActivation{
    "BlockingDownloadsInAdFrameWithoutUserActivation",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Support COEP on SharedWorker.
const base::Feature kCOEPForSharedWorker{"COEPForSharedWorker",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCOLRV1Fonts{"COLRV1Fonts",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// Enable CSS Container Queries. Also implies LayoutNGGrid and CSSContainSize1D.
const base::Feature kCSSContainerQueries{"CSSContainerQueries",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the Conversion Measurement API infrastructure is enabled.
const base::Feature kConversionMeasurement{"ConversionMeasurement",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kGMSCoreEmoji{"GMSCoreEmoji",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

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
// Disabled on Mobile to allow for a delayed Finch roll-out. Enabled on
// Desktop platforms.
const base::Feature kPaintHoldingCrossOrigin {
  "PaintHoldingCrossOrigin",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

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

// Controls off-thread code cache consumption.
const base::Feature kConsumeCodeCacheOffThread{
    "ConsumeCodeCacheOffThread", base::FEATURE_ENABLED_BY_DEFAULT};

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
    "ForceSynchronousHTMLParsing", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable EditingNG by default. This feature is for a kill switch.
const base::Feature kEditingNG{"EditingNG", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable LayoutNG.
const base::Feature kLayoutNG{"LayoutNG", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMixedContentAutoupgrade{"AutoupgradeMixedContent",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// An experimental replacement for the `User-Agent` header, defined in
// https://tools.ietf.org/html/draft-west-ua-client-hints.
const base::Feature kUserAgentClientHint{"UserAgentClientHint",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enable `sec-ch-ua-full-version-list` client hint.
const base::Feature kUserAgentClientHintFullVersionList{
    "UserAgentClientHintFullVersionList", base::FEATURE_ENABLED_BY_DEFAULT};

// Handle prefers-color-scheme user preference media feature via client hints.
const base::Feature kPrefersColorSchemeClientHintHeader{
    "PrefersColorSchemeClientHintHeader", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the Viewport Height client hint can be added to request
// headers.
const base::Feature kViewportHeightClientHintHeader{
    "ViewportHeightClientHintHeader", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Enable browser-initiated dedicated worker script loading
// (PlzDedicatedWorker). https://crbug.com/906991
const base::Feature kPlzDedicatedWorker{"PlzDedicatedWorker",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Portals. https://crbug.com/865123.
// Note that default enabling this does not expose the portal
// element on its own, but does allow its use with an origin trial. This was the
// case for the M85 Android only origin trial (https://crbug.com/1040212).
const base::Feature kPortals{"Portals", base::FEATURE_DISABLED_BY_DEFAULT};

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
                                  base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<FencedFramesImplementationType>::Option
    fenced_frame_implementation_types[] = {
        {FencedFramesImplementationType::kShadowDOM, "shadow_dom"},
        {FencedFramesImplementationType::kMPArch, "mparch"}};
const base::FeatureParam<FencedFramesImplementationType>
    kFencedFramesImplementationTypeParam{
        &kFencedFrames, "implementation_type",
        FencedFramesImplementationType::kShadowDOM,
        &fenced_frame_implementation_types};

// Enable the shared storage API. This base::Feature directly controls the
// corresponding runtime enabled feature.
// https://github.com/pythagoraskitty/shared-storage/blob/main/README.md
const base::Feature kSharedStorageAPI{"SharedStorageAPI",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<int>
    kSharedStorageURLSelectionOperationInputURLSizeLimit{
        &kSharedStorageAPI, "url_selection_operation_input_url_size_limit", 5};

// Enables the Prerender2 feature: https://crbug.com/1126305
// Note that default enabling this does not enable the Prerender2 features
// because kSetOnlyIfOverridden is used for setting WebRuntimeFeatures'
// Prerender2. To enable this feature, we need to force-enable this feature
// using chrome://flags/#enable-prerender2 or --enable-features=Prerender2
// command line or a valid Origin Trial token in the page.
const base::Feature kPrerender2 {
  "Prerender2",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kPrerender2MemoryControls{"Prerender2MemoryControls",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
const char kPrerender2MemoryThresholdParamName[] = "memory_threshold_in_mb";

bool IsPrerender2Enabled() {
  return base::FeatureList::IsEnabled(blink::features::kPrerender2);
}

bool IsFencedFramesEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kFencedFrames);
}

bool IsFencedFramesMPArchBased() {
  return blink::features::kFencedFramesImplementationTypeParam.Get() ==
         blink::features::FencedFramesImplementationType::kMPArch;
}

const base::Feature kInitialNavigationEntry{"InitialNavigationEntry",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

bool IsInitialNavigationEntryEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kInitialNavigationEntry);
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

// Enables new behavior for window.open() and BarProp properties.
const base::Feature kWindowOpenNewPopupBehavior{
    "WindowOpenNewPopupBehavior", base::FEATURE_ENABLED_BY_DEFAULT};

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
    base::FEATURE_ENABLED_BY_DEFAULT};

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

// Throttles Javascript timer wake ups on foreground pages.
const base::Feature kThrottleForegroundTimers{
    "ThrottleForegroundTimers", base::FEATURE_DISABLED_BY_DEFAULT};

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
const base::Feature kSpeculationRulesPrefetchProxy {
  "SpeculationRulesPrefetchProxy",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

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

// Enables CSS selector fragment anchors. https://crbug.com/1252460
const base::Feature kCssSelectorFragmentAnchor{
    "CssSelectorFragmentAnchor", base::FEATURE_DISABLED_BY_DEFAULT};

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
// RealtimeAudioWorkletThread object. This can be controlled by a field trial,
// it will use the NORMAL priority thread when disabled.
const base::Feature kAudioWorkletThreadRealtimePriority{
    "AudioWorkletThreadRealtimePriority", base::FEATURE_ENABLED_BY_DEFAULT};

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
// Range: 0 (do not invert any text) to 255 (invert all text)
// Can also set to -1 to let Blink's internal settings control the value
const base::FeatureParam<int> kForceDarkForegroundLightnessThresholdParam{
    &kForceWebContentsDarkMode, "foreground_lightness_threshold", -1};

// Do not invert backgrounds darker than this.
// Range: 0 (invert all backgrounds) to 255 (invert no backgrounds)
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

// When enabled, enforces new interoperable semantics for 3D transforms.
// See crbug.com/1008483.
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

// Let accelerated canvases remain accelerated after readback
// (crbug.com/1288118)
const base::Feature kCanvas2dStaysGPUOnReadback{
    "Canvas2dStaysGPUOnReadback", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, frees up CachedMetadata after consumption by script resources
// and modules. Needed for the experiment in http://crbug.com/1045052.
const base::Feature kDiscardCodeCacheAfterFirstUse{
    "DiscardCodeCacheAfterFirstUse", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCacheCodeOnIdle{"CacheCodeOnIdle",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<int> kCacheCodeOnIdleDelayParam{&kCacheCodeOnIdle,
                                                         "delay-in-ms", 0};

// Kill switch for the new element.offsetParent behavior.
// TODO(crbug.com/920069): Remove this once the feature has
// landed and no compat issues are reported.
const base::Feature kOffsetParentNewSpecBehavior{
    "OffsetParentNewSpecBehavior", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kKeepScriptResourceAlive{"KeepScriptResourceAlive",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

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
    "InputTargetClientHighPriority", base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enabling this will cause parkable strings to use Snappy for compression iff
// kCompressParkableStrings is enabled.
const base::Feature kUseSnappyForParkableStrings{
    "UseSnappyForParkableStrings", base::FEATURE_DISABLED_BY_DEFAULT};

// Enabling this will delay the first aging of strings by 60 seconds instead of
// the default. See comment around the use of the feature for the logic behind
// the delay.
const base::Feature kDelayFirstParkingOfStrings{
    "DelayFirstParkingOfStrings", base::FEATURE_DISABLED_BY_DEFAULT};

bool ParkableStringsUseSnappy() {
  return base::FeatureList::IsEnabled(kUseSnappyForParkableStrings);
}

bool IsParkableStringsToDiskEnabled() {
  // Always enabled as soon as compression is enabled.
  return base::FeatureList::IsEnabled(kCompressParkableStrings);
}

const base::Feature kCLSScrollAnchoring{"CLSScrollAnchoring",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enables web apps to provide theme color and background color overrides for
// dark mode.
const base::Feature kWebAppEnableDarkMode{"WebAppEnableDarkMode",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the "handle_links" manifest field for web apps.
// Explainer:
// https://github.com/WICG/pwa-url-handler/blob/main/handle_links/explainer.md
const base::Feature kWebAppEnableHandleLinks{"WebAppEnableHandleLinks",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables web apps to request isolated storage.
const base::Feature kWebAppEnableIsolatedStorage{
    "WebAppEnableIsolatedStorage", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the "launch_handler" manifest field for web apps.
// Explainer: https://github.com/WICG/sw-launch/blob/main/launch_handler.md
const base::Feature kWebAppEnableLaunchHandler{
    "WebAppEnableLaunchHandler", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables declarative link capturing in web apps.
// Explainer:
// https://github.com/WICG/sw-launch/blob/master/declarative_link_capturing.md
const base::Feature kWebAppEnableLinkCapturing{
    "WebAppEnableLinkCapturing", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Unique ID feature in web apps. Controls parsing of "id" field in web
// app manifests. See explainer for more information:
// https://github.com/philloooo/pwa-unique-id
const base::Feature kWebAppEnableManifestId{"WebAppEnableManifestId",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the "translations" manifest field for web apps.
const base::Feature kWebAppEnableTranslations{
    "WebAppEnableTranslations", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls URL handling feature in web apps. Controls parsing of "url_handlers"
// field in web app manifests. See explainer for more information:
// https://github.com/WICG/pwa-url-handler/blob/main/explainer.md
const base::Feature kWebAppEnableUrlHandlers{"WebAppEnableUrlHandlers",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Protocol handling feature in web apps. Controls parsing of
// "protocol_handlers" field in web app manifests. See explainer for more
// information:
// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/URLProtocolHandler/explainer.md
const base::Feature kWebAppEnableProtocolHandlers{
    "WebAppEnableProtocolHandlers", base::FEATURE_ENABLED_BY_DEFAULT};

// Makes network loading tasks unfreezable so that they can be processed while
// the page is frozen.
const base::Feature kLoadingTasksUnfreezable{"LoadingTasksUnfreezable",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// BackForwardCache:
// Only use per-process buffer limit and not per-request limt. When this flag is
// on network requests can continue buffering data as long as it is under per
// process limit.
// TODO(crbug.com/1243600): Remove this flag eventually.
const base::Feature kNetworkRequestUsesOnlyPerProcessBufferLimit{
    "NetworkRequestUsesOnlyPerProcessBufferLimit",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Kill switch for the new behavior whereby anchors with target=_blank get
// noopener behavior by default. TODO(crbug.com/898942): Remove in Chrome 95.
const base::Feature kTargetBlankImpliesNoOpener{
    "TargetBlankImpliesNoOpener", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls how max frame rates are enforced in MediaStreamTracks.
// TODO(crbug.com/1152307): Remove in M91.
const base::Feature kMediaStreamTrackUseConfigMaxFrameRate{
    "MediaStreamTrackUseConfigMaxFrameRate", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the SubresourceFilter receives calls from the ResourceLoader
// to perform additional checks against any aliases found from DNS CNAME records
// for the requested URL.
const base::Feature kSendCnameAliasesToSubresourceFilterFromRenderer{
    "SendCnameAliasesToSubresourceFilterFromRenderer",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for the InterestCohort API origin trial, i.e. if disabled, the
// API exposure will be disabled regardless of the OT config.
// (See https://github.com/WICG/floc.)
const base::Feature kInterestCohortAPIOriginTrial{
    "InterestCohortAPIOriginTrial", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the availability of the "interest-cohort" permissions policy.
const base::Feature kInterestCohortFeaturePolicy{
    "InterestCohortFeaturePolicy", base::FEATURE_DISABLED_BY_DEFAULT};

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
    "EnablePenetratingImageSelection", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, permits shared/root element transitions. See
// https://github.com/WICG/shared-element-transitions.
const base::Feature kDocumentTransition{"DocumentTransition",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, uses the code-path to drive shared/root element transitions
// from the renderer process.
const base::Feature kDocumentTransitionRenderer{
    "DocumentTransitionRenderer", base::FEATURE_DISABLED_BY_DEFAULT};

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
const base::Feature kBackgroundTracingPerformanceMark{
    "BackgroundTracingPerformanceMark", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<std::string>
    kBackgroundTracingPerformanceMark_AllowList{
        &kBackgroundTracingPerformanceMark, "allow_list", ""};

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

// Kill switch for the Interest Group API, i.e. if disabled, the
// API exposure will be disabled regardless of the OT config.
const base::Feature kInterestGroupStorage{"InterestGroupStorage",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
// TODO(crbug.com/1197209): Adjust these limits in response to usage.
const base::FeatureParam<int> kInterestGroupStorageMaxOwners{
    &kInterestGroupStorage, "max_owners", 1000};
const base::FeatureParam<int> kInterestGroupStorageMaxGroupsPerOwner{
    &kInterestGroupStorage, "max_groups_per_owner", 1000};
const base::FeatureParam<int> kInterestGroupStorageMaxOpsBeforeMaintenance{
    &kInterestGroupStorage, "max_ops_before_maintenance", 1000000};

// Enable the availability of the ad interest group API as part of the
// origin trial for FLEDGE or PARAKEET.
const base::Feature kAdInterestGroupAPI{"AdInterestGroupAPI",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Feature flag to enable PARAKEET implementation (See
// https://github.com/WICG/privacy-preserving-ads/blob/main/Parakeet.md).
// See also https://crbug.com/1249186.
const base::Feature kParakeet{"Parakeet", base::FEATURE_DISABLED_BY_DEFAULT};

// See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
// Enables FLEDGE implementation. See https://crbug.com/1186444.
const base::Feature kFledge{"Fledge", base::FEATURE_DISABLED_BY_DEFAULT};

// See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
// Changes default Permissions Policy for features join-ad-interest-group and
// run-ad-auction to a more restricted EnableForSelf.
const base::Feature kAdInterestGroupAPIRestrictedPolicyByDefault{
    "AdInterestGroupAPIRestrictedPolicyByDefault",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables URN URLs like those produced by FLEDGE auctions to be displayed by
// iframes (instead of requiring fenced frames). This is only intended to be
// enabled as part of the FLEDGE origin trial.
const base::Feature kAllowURNsInIframes{"AllowURNsInIframes",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

BLINK_COMMON_EXPORT bool IsAllowURNsInIframeEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kAllowURNsInIframes);
}

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
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Send all user interaction latency data from renderer to the browser process.
const base::Feature kSendAllUserInteractionLatencies{
    "SendAllUserInteractionLatencies", base::FEATURE_DISABLED_BY_DEFAULT};

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

// API that allows installed PWAs to add additional shortcuts by means of
// installing sub app components.
const base::Feature kDesktopPWAsSubApps{"DesktopPWAsSubApps",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Suppresses console errors for CORS problems which report an associated
// inspector issue anyway.
const base::Feature kCORSErrorsIssueOnly{"CORSErrorsIssueOnly",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPersistentQuotaIsTemporaryQuota{
    "PersistentQuotaIsTemporaryQuota", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDelayLowPriorityRequestsAccordingToNetworkState{
    "DelayLowPriorityRequestsAccordingToNetworkState",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIncludeInitiallyInvisibleImagesInLCP{
    "IncludeInitiallyInvisibleImagesInLCP", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIncludeBackgroundSVGInLCP{
    "IncludeBackgroundSVGInLCP", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kMaxNumOfThrottleableRequestsInTightMode{
    &kDelayLowPriorityRequestsAccordingToNetworkState,
    "MaxNumOfThrottleableRequestsInTightMode", 5};

const base::FeatureParam<base::TimeDelta> kHttpRttThreshold{
    &kDelayLowPriorityRequestsAccordingToNetworkState, "HttpRttThreshold",
    base::Milliseconds(450)};

const base::FeatureParam<double> kCostReductionOfMultiplexedRequests{
    &kDelayLowPriorityRequestsAccordingToNetworkState,
    "CostReductionOfMultiplexedRequests", 0.5};

const base::Feature kForceMajorVersion100InUserAgent{
    "ForceMajorVersion100InUserAgent", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kForceMinorVersion100InUserAgent{
    "ForceMinorVersion100InUserAgent", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kForceMajorVersionInMinorPositionInUserAgent{
    "ForceMajorVersionInMinorPositionInUserAgent",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable `sec-ch-device-memory` client hint.
const base::Feature kClientHintsDeviceMemory{"ClientHintsDeviceMemory",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enable `sec-ch-dpr` client hint.
const base::Feature kClientHintsDPR{"ClientHintsDPR",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable `sec-ch-width` client hint.
const base::Feature kClientHintsResourceWidth{"ClientHintsResourceWidth",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enable `sec-ch-viewport-width` client hint.
const base::Feature kClientHintsViewportWidth{"ClientHintsViewportWidth",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Allows third party use of WebSQL (`DOMWindowWebDatabase::openDatabase`).
const base::Feature kWebSQLInThirdPartyContextEnabled{
    "WebSQLInThirdPartyContextEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable legacy `device-memory` client hint.
const base::Feature kClientHintsDeviceMemory_DEPRECATED{
    "ClientHintsDeviceMemory_DEPRECATED", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable legacy `dpr` client hint.
const base::Feature kClientHintsDPR_DEPRECATED{
    "ClientHintsDPR_DEPRECATED", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable legacy `width` client hint.
const base::Feature kClientHintsResourceWidth_DEPRECATED{
    "ClientHintsResourceWidth_DEPRECATED", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable legacy `viewport-width` client hint.
const base::Feature kClientHintsViewportWidth_DEPRECATED{
    "ClientHintsViewportWidth_DEPRECATED", base::FEATURE_ENABLED_BY_DEFAULT};

// https://drafts.csswg.org/css-cascade-5/#layering
const base::Feature kCSSCascadeLayers{"CSSCascadeLayers",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the setTimeout(..., 0) will not clamp to 1ms.
// Tracking bug: https://crbug.com/402694.
const base::Feature kSetTimeoutWithoutClamp{"SetTimeoutWithoutClamp",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
bool IsSetTimeoutWithoutClampEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kSetTimeoutWithoutClamp);
}

const base::Feature kTabSwitchMetrics2{"TabSwitchMetrics2",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables reporting and web-exposure (respectively) of the time the first frame
// of an animated image was painted.
const base::Feature kLCPAnimatedImagesReporting{
    "LCPAnimatedImagesReporting", base::FEATURE_DISABLED_BY_DEFAULT};

// Throws when `kWebSQLInThirdPartyContextEnabled` is disabled.
const base::Feature kWebSQLInThirdPartyContextThrowsWhenDisabled{
    "WebSQLInThirdPartyContextThrowsWhenDisabled",
    base::FEATURE_ENABLED_BY_DEFAULT};

// https://blog.whatwg.org/newline-normalizations-in-form-submission
const base::Feature kLateFormNewlineNormalization{
    "LateFormNewlineNormalization", base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(crbug.com/1185950): Remove this flag when the feature is fully launched
// and released to stable with no issues.
const base::Feature kAutoExpandDetailsElement{"AutoExpandDetailsElement",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables loading the response body earlier in navigation.
const base::Feature kEarlyBodyLoad{"EarlyBodyLoad",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables fetching the code cache earlier in navigation.
const base::Feature kEarlyCodeCache{"EarlyCodeCache",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Allow use of an http-equiv meta tag to set client hints.
const base::Feature kClientHintsMetaHTTPEquivAcceptCH{
    "ClientHintsMetaHTTPEquivAcceptCH", base::FEATURE_ENABLED_BY_DEFAULT};

// Allow use of a named meta tag to set client hints.
const base::Feature kClientHintsMetaNameAcceptCH{
    "ClientHintsMetaNameAcceptCH", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOriginAgentClusterDefaultEnabled{
    "OriginAgentClusterDefaultEnable", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOriginAgentClusterDefaultWarning{
    "OriginAgentClusterDefaultWarning", base::FEATURE_DISABLED_BY_DEFAULT};

// Allow third-party delegation of client hint information.
const base::Feature kClientHintThirdPartyDelegation{
    "ClientHintThirdPartyDelegation", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Enables prefetching Android fonts on renderer startup.
const base::Feature kPrefetchAndroidFonts{"PrefetchAndroidFonts",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Lazy initialize TimeZoneController.
const base::Feature kLazyInitializeTimeZoneController{
    "LazyInitializeTimeZoneController", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCompositedCaret{"CompositedCaret",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Initialize CSSDefaultStyleSheets early in renderer startup.
const base::Feature kDefaultStyleSheetsEarlyInit{
    "DefaultStyleSheetsEarlyInit", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace blink
