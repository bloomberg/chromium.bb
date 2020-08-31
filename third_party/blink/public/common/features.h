// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"

namespace blink {
namespace features {

BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingDownloadsInAdFrameWithoutUserActivation;
BLINK_COMMON_EXPORT extern const base::Feature kPaintHolding;
BLINK_COMMON_EXPORT extern const base::Feature
    kEagerCacheStorageSetupForServiceWorkers;
BLINK_COMMON_EXPORT extern const base::Feature kScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kSmallScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kUserLevelMemoryPressureSignal;
BLINK_COMMON_EXPORT extern const base::Feature kFreezePurgeMemoryAllPagesFrozen;
BLINK_COMMON_EXPORT extern const base::Feature kFreezeUserAgent;
BLINK_COMMON_EXPORT extern const base::Feature kImplicitRootScroller;
BLINK_COMMON_EXPORT extern const base::Feature kCSSOMViewScrollCoordinates;
BLINK_COMMON_EXPORT extern const base::Feature kDisplayLocking;
BLINK_COMMON_EXPORT extern const base::Feature kMaxOverlapBoundsForFixed;
BLINK_COMMON_EXPORT extern const base::Feature kJSONModules;
BLINK_COMMON_EXPORT extern const base::Feature kTopLevelAwait;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNG;
BLINK_COMMON_EXPORT extern const base::Feature kMixedContentAutoupgrade;
BLINK_COMMON_EXPORT extern const base::Feature kNavigationPredictor;
BLINK_COMMON_EXPORT extern const base::Feature kPlzDedicatedWorker;
BLINK_COMMON_EXPORT extern const base::Feature kPortals;
BLINK_COMMON_EXPORT extern const base::Feature kPortalsCrossOrigin;
BLINK_COMMON_EXPORT extern const base::Feature
    kPreviewsResourceLoadingHintsSpecificResourceTypes;
BLINK_COMMON_EXPORT extern const base::Feature
    kPurgeRendererMemoryWhenBackgrounded;
BLINK_COMMON_EXPORT extern const base::Feature kRawClipboard;
BLINK_COMMON_EXPORT extern const base::Feature kRTCGetDisplayMedia;
BLINK_COMMON_EXPORT extern const base::Feature kRTCUnifiedPlanByDefault;
BLINK_COMMON_EXPORT extern const base::Feature kRTCOfferExtmapAllowMixed;
BLINK_COMMON_EXPORT extern const base::Feature kV8OptimizeWorkersForPerformance;
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcMultiplexCodec;
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcHideLocalIpsWithMdns;

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcH264WithOpenH264FFmpeg;
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

BLINK_COMMON_EXPORT extern const base::Feature kResourceLoadViaDataPipe;
BLINK_COMMON_EXPORT extern const base::Feature kServiceWorkerUpdateDelay;
BLINK_COMMON_EXPORT extern const base::Feature kStopInBackground;
BLINK_COMMON_EXPORT extern const base::Feature
    kFreezeBackgroundTabOnNetworkIdle;
BLINK_COMMON_EXPORT extern const base::Feature kStopNonTimersInBackground;
BLINK_COMMON_EXPORT extern const base::Feature kStorageAccessAPI;
BLINK_COMMON_EXPORT extern const base::Feature kTextFragmentAnchor;
BLINK_COMMON_EXPORT extern const base::Feature kFontAccess;
BLINK_COMMON_EXPORT extern const base::Feature kNativeFileSystemAPI;
BLINK_COMMON_EXPORT extern const base::Feature kFileHandlingAPI;
BLINK_COMMON_EXPORT extern const base::Feature kAllowSyncXHRInPageDismissal;
BLINK_COMMON_EXPORT extern const base::Feature kPrefetchPrivacyChanges;

BLINK_COMMON_EXPORT extern const base::Feature kWebComponentsV0Enabled;

BLINK_COMMON_EXPORT extern const char kMixedContentAutoupgradeModeParamName[];
BLINK_COMMON_EXPORT extern const char kMixedContentAutoupgradeModeAllPassive[];

BLINK_COMMON_EXPORT extern const base::Feature kDecodeJpeg420ImagesToYUV;
BLINK_COMMON_EXPORT extern const base::Feature kDecodeLossyWebPImagesToYUV;

BLINK_COMMON_EXPORT extern const base::Feature
    kWebFontsCacheAwareTimeoutAdaption;
BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingFocusWithoutUserActivation;

BLINK_COMMON_EXPORT extern const base::Feature kAudioWorkletRealtimeThread;

BLINK_COMMON_EXPORT extern const base::Feature kLightweightNoStatePrefetch;
BLINK_COMMON_EXPORT extern const base::Feature
    kLightweightNoStatePrefetch_FetchFonts;

BLINK_COMMON_EXPORT extern const base::Feature kSaveDataImgSrcset;

BLINK_COMMON_EXPORT extern const base::Feature kForceWebContentsDarkMode;
BLINK_COMMON_EXPORT extern const base::FeatureParam<ForceDarkInversionMethod>
    kForceDarkInversionMethodParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<ForceDarkImageBehavior>
    kForceDarkImageBehaviorParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kForceDarkTextLightnessThresholdParam;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kForceDarkBackgroundLightnessThresholdParam;

// Returns true when PlzDedicatedWorker is enabled.
BLINK_COMMON_EXPORT bool IsPlzDedicatedWorkerEnabled();

BLINK_COMMON_EXPORT extern const base::Feature kWebRtcUseMinMaxVEADimensions;

// Blink garbage collection.
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapCompaction;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapConcurrentMarking;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapConcurrentSweeping;
BLINK_COMMON_EXPORT extern const base::Feature kBlinkHeapIncrementalMarking;
BLINK_COMMON_EXPORT extern const base::Feature
    kBlinkHeapIncrementalMarkingStress;

BLINK_COMMON_EXPORT extern const base::Feature
    kVerifyHTMLFetchedFromAppCacheBeforeDelay;

BLINK_COMMON_EXPORT extern const base::Feature
    kBlinkCompositorUseDisplayThreadPriority;

BLINK_COMMON_EXPORT extern const base::Feature
    kIgnoreCrossOriginWindowWhenNamedAccessOnWindow;

BLINK_COMMON_EXPORT extern const base::Feature
    kLowerJavaScriptPriorityWhenForceDeferred;

BLINK_COMMON_EXPORT extern const base::Feature kDisableForceDeferInChildFrames;

BLINK_COMMON_EXPORT extern const base::Feature kARIAAnnotations;

BLINK_COMMON_EXPORT extern const base::Feature kCompositeCrossOriginIframes;

BLINK_COMMON_EXPORT extern const base::Feature kSubresourceRedirect;

BLINK_COMMON_EXPORT extern const base::Feature kSetLowPriorityForBeacon;

BLINK_COMMON_EXPORT extern const base::Feature kCacheStorageCodeCacheHintHeader;
BLINK_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kCacheStorageCodeCacheHintHeaderName;

BLINK_COMMON_EXPORT extern const base::Feature kDispatchBeforeUnloadOnFreeze;

BLINK_COMMON_EXPORT extern const base::Feature kLowLatencyCanvas2dImageChromium;
BLINK_COMMON_EXPORT extern const base::Feature kLowLatencyCanvas2dSwapChain;
BLINK_COMMON_EXPORT extern const base::Feature kLowLatencyWebGLSwapChain;

BLINK_COMMON_EXPORT extern const base::Feature kDawn2dCanvas;

BLINK_COMMON_EXPORT extern const base::Feature
    kForceExtraRenderingToTrackStickyFrame;

BLINK_COMMON_EXPORT extern const base::Feature
    kCSSReducedFontLoadingInvalidations;

BLINK_COMMON_EXPORT extern const base::Feature kDiscardCodeCacheAfterFirstUse;

BLINK_COMMON_EXPORT extern const base::Feature
    kSuppressContentTypeForBeaconMadeWithArrayBufferView;

BLINK_COMMON_EXPORT extern const base::Feature kBlockHTMLParserOnStyleSheets;

BLINK_COMMON_EXPORT extern const base::Feature kFontPreloadingDelaysRendering;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kFontPreloadingDelaysRenderingParam;

BLINK_COMMON_EXPORT extern const base::Feature kFlexGaps;
BLINK_COMMON_EXPORT extern const base::Feature kFlexNG;

BLINK_COMMON_EXPORT extern const base::Feature kKeepScriptResourceAlive;

BLINK_COMMON_EXPORT extern const base::Feature kAppCache;
BLINK_COMMON_EXPORT extern const base::Feature kAppCacheRequireOriginTrial;

BLINK_COMMON_EXPORT extern const base::Feature kAVIF;

BLINK_COMMON_EXPORT extern const base::Feature
    kAlignFontDisplayAutoTimeoutWithLCPGoal;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam;
enum class AlignFontDisplayAutoTimeoutWithLCPGoalMode {
  kToFailurePeriod,
  kToSwapPeriod
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    AlignFontDisplayAutoTimeoutWithLCPGoalMode>
    kAlignFontDisplayAutoTimeoutWithLCPGoalModeParam;

BLINK_COMMON_EXPORT extern const base::Feature kThrottleInstallingServiceWorker;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kInstallingServiceWorkerOutstandingThrottledLimit;

// Enables resampling GestureScroll events on compositor thread.
BLINK_COMMON_EXPORT extern const base::Feature kResamplingScrollEvents;

// Enables the device-memory, resource-width, viewport-width and DPR client
// hints to be sent to third-party origins if the first-party has opted in to
// receiving client hints, regardless of Feature Policy.
BLINK_COMMON_EXPORT extern const base::Feature kAllowClientHintsToThirdParty;

// The type of scroll predictor to use for the resampling scroll events. These
// values are used as the 'predictor' feature param for
// |kResamplingScrollEvents|.
BLINK_COMMON_EXPORT extern const char kScrollPredictorNameLsq[];
BLINK_COMMON_EXPORT extern const char kScrollPredictorNameKalman[];
BLINK_COMMON_EXPORT extern const char kScrollPredictorNameLinearFirst[];
BLINK_COMMON_EXPORT extern const char kScrollPredictorNameLinearSecond[];
BLINK_COMMON_EXPORT extern const char kScrollPredictorNameLinearResampling[];
BLINK_COMMON_EXPORT extern const char kScrollPredictorNameEmpty[];

// Enables filtering of predicted scroll events on compositor thread.
BLINK_COMMON_EXPORT extern const base::Feature kFilteringScrollPrediction;

// The type of filter to use for the filtering scroll events. These
// values are used as the 'filter' feature param for
// |kFilteringScrollPrediction|.
BLINK_COMMON_EXPORT extern const char kFilterNameEmpty[];
BLINK_COMMON_EXPORT extern const char kFilterNameOneEuro[];

// Enables changing the influence of acceleration based on change of direction.
BLINK_COMMON_EXPORT extern const base::Feature kKalmanHeuristics;

// Enables discarding the prediction if the predicted direction is opposite from
// the current direction.
BLINK_COMMON_EXPORT extern const base::Feature kKalmanDirectionCutOff;

// Skips the browser touch event filter, ensuring that events that reach the
// queue and would otherwise be filtered out will instead be passed onto the
// renderer compositor process as long as the page hasn't timed out. If
// skip_filtering_process is browser_and_renderer, also skip the renderer cc
// touch event filter, ensuring that events will be passed onto the renderer
// main thread. Which event types will be always forwarded is controlled by the
// "type" FeatureParam, which can be either "discrete" (default) or "all".
BLINK_COMMON_EXPORT
extern const base::Feature kSkipTouchEventFilter;
BLINK_COMMON_EXPORT
extern const char kSkipTouchEventFilterTypeParamName[];
BLINK_COMMON_EXPORT
extern const char kSkipTouchEventFilterTypeParamValueDiscrete[];
BLINK_COMMON_EXPORT
extern const char kSkipTouchEventFilterTypeParamValueAll[];
BLINK_COMMON_EXPORT
extern const char kSkipTouchEventFilterFilteringProcessParamName[];
BLINK_COMMON_EXPORT
extern const char kSkipTouchEventFilterFilteringProcessParamValueBrowser[];
BLINK_COMMON_EXPORT
extern const char
    kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer[];

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
