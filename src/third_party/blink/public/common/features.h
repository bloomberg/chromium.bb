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
BLINK_COMMON_EXPORT extern const base::Feature kCOLRV1Fonts;
BLINK_COMMON_EXPORT extern const base::Feature kPaintHolding;
BLINK_COMMON_EXPORT extern const base::Feature kPaintHoldingCrossOrigin;
BLINK_COMMON_EXPORT extern const base::Feature
    kEagerCacheStorageSetupForServiceWorkers;
BLINK_COMMON_EXPORT extern const base::Feature kScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kSmallScriptStreaming;
BLINK_COMMON_EXPORT extern const base::Feature kUserLevelMemoryPressureSignal;
BLINK_COMMON_EXPORT extern const base::Feature kFreezePurgeMemoryAllPagesFrozen;
BLINK_COMMON_EXPORT extern const base::Feature kFreezeUserAgent;
BLINK_COMMON_EXPORT extern const base::Feature
    kFrequencyCappingForOverlayPopupDetection;
BLINK_COMMON_EXPORT extern const base::Feature
    kFrequencyCappingForLargeStickyAdDetection;
BLINK_COMMON_EXPORT extern const base::Feature kFtpProtocol;
BLINK_COMMON_EXPORT extern const base::Feature kInsertKeyToggleMode;
BLINK_COMMON_EXPORT extern const base::Feature kDisplayLocking;
BLINK_COMMON_EXPORT extern const base::Feature kJSONModules;
BLINK_COMMON_EXPORT extern const base::Feature kForceSynchronousHTMLParsing;
BLINK_COMMON_EXPORT extern const base::Feature kTopLevelAwait;
BLINK_COMMON_EXPORT extern const base::Feature kEditingNG;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNG;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNGTable;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNGFieldset;
BLINK_COMMON_EXPORT extern const base::Feature kLayoutNGTextControl;
BLINK_COMMON_EXPORT extern const base::Feature kMixedContentAutoupgrade;
BLINK_COMMON_EXPORT extern const base::Feature kNavigationPredictor;
BLINK_COMMON_EXPORT extern const base::Feature kNavigatorPluginsEmpty;
BLINK_COMMON_EXPORT extern const base::Feature kPlzDedicatedWorker;
BLINK_COMMON_EXPORT extern const base::Feature kPortals;
BLINK_COMMON_EXPORT extern const base::Feature kPortalsCrossOrigin;

// Prerender2:
BLINK_COMMON_EXPORT extern const base::Feature kPrerender2;
enum class Prerender2Implementation { kWebContents, kMPArch };
BLINK_COMMON_EXPORT extern const base::FeatureParam<Prerender2Implementation>
    kPrerender2ImplementationParam;

// Returns true when Prerender2 feature is enabled.
BLINK_COMMON_EXPORT bool IsPrerender2Enabled();

BLINK_COMMON_EXPORT extern const base::Feature
    kPreviewsResourceLoadingHintsSpecificResourceTypes;
BLINK_COMMON_EXPORT extern const base::Feature
    kPurgeRendererMemoryWhenBackgrounded;
BLINK_COMMON_EXPORT extern const base::Feature kRawClipboard;
BLINK_COMMON_EXPORT extern const base::Feature
    kRTCGetCurrentBrowsingContextMedia;
BLINK_COMMON_EXPORT extern const base::Feature kRTCUnifiedPlanByDefault;
BLINK_COMMON_EXPORT extern const base::Feature kRTCOfferExtmapAllowMixed;
BLINK_COMMON_EXPORT extern const base::Feature kRTCGpuCodecSupportWaiter;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kRTCGpuCodecSupportWaiterTimeoutParam;
BLINK_COMMON_EXPORT extern const base::Feature kV8OptimizeWorkersForPerformance;
BLINK_COMMON_EXPORT extern const base::Feature
    kWebMeasureMemoryViaPerformanceManager;
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcMultiplexCodec;
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcHideLocalIpsWithMdns;
BLINK_COMMON_EXPORT extern const base::Feature
    kWebRtcIgnoreUnspecifiedColorSpace;

BLINK_COMMON_EXPORT extern const base::Feature kIntensiveWakeUpThrottling;
BLINK_COMMON_EXPORT extern const char
    kIntensiveWakeUpThrottling_GracePeriodSeconds_Name[];

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
BLINK_COMMON_EXPORT extern const base::Feature kWebRtcH264WithOpenH264FFmpeg;
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

BLINK_COMMON_EXPORT extern const base::Feature kResourceLoadViaDataPipe;
BLINK_COMMON_EXPORT extern const base::Feature kServiceWorkerUpdateDelay;
BLINK_COMMON_EXPORT extern const base::Feature kStopInBackground;
BLINK_COMMON_EXPORT extern const base::Feature
    kFreezeBackgroundTabOnNetworkIdle;
BLINK_COMMON_EXPORT extern const base::Feature kStorageAccessAPI;
BLINK_COMMON_EXPORT extern const base::Feature kTextFragmentAnchor;
BLINK_COMMON_EXPORT extern const base::Feature kFontAccess;
BLINK_COMMON_EXPORT extern const base::Feature kFontAccessPersistent;
BLINK_COMMON_EXPORT extern const base::Feature kComputePressure;
BLINK_COMMON_EXPORT extern const base::Feature kFileHandlingAPI;
BLINK_COMMON_EXPORT extern const base::Feature kAllowSyncXHRInPageDismissal;
BLINK_COMMON_EXPORT extern const base::Feature kPrefetchPrivacyChanges;

BLINK_COMMON_EXPORT extern const base::Feature kDecodeJpeg420ImagesToYUV;
BLINK_COMMON_EXPORT extern const base::Feature kDecodeLossyWebPImagesToYUV;

BLINK_COMMON_EXPORT extern const base::Feature
    kWebFontsCacheAwareTimeoutAdaption;
BLINK_COMMON_EXPORT extern const base::Feature
    kBlockingFocusWithoutUserActivation;

BLINK_COMMON_EXPORT extern const base::Feature kAudioWorkletRealtimeThread;
BLINK_COMMON_EXPORT extern const base::Feature
    kAudioWorkletThreadRealtimePriority;

BLINK_COMMON_EXPORT extern const base::Feature kLightweightNoStatePrefetch;

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

BLINK_COMMON_EXPORT extern const base::Feature kDisableForceDeferInChildFrames;

BLINK_COMMON_EXPORT extern const base::Feature kTransformInterop;

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

BLINK_COMMON_EXPORT extern const base::Feature kWebviewAccelerateSmallCanvases;

BLINK_COMMON_EXPORT extern const base::Feature kDiscardCodeCacheAfterFirstUse;

BLINK_COMMON_EXPORT extern const base::Feature kBlockHTMLParserOnStyleSheets;

BLINK_COMMON_EXPORT extern const base::Feature kLinkDisabledNewSpecBehavior;

BLINK_COMMON_EXPORT extern const base::Feature kFontPreloadingDelaysRendering;
BLINK_COMMON_EXPORT extern const base::FeatureParam<int>
    kFontPreloadingDelaysRenderingParam;

BLINK_COMMON_EXPORT extern const base::Feature kKeepScriptResourceAlive;

BLINK_COMMON_EXPORT extern const base::Feature kDelayAsyncScriptExecution;
enum class DelayAsyncScriptDelayType {
  kFinishedParsing,
  kFirstPaintOrFinishedParsing,
  kUseOptimizationGuide,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<DelayAsyncScriptDelayType>
    kDelayAsyncScriptExecutionDelayParam;

BLINK_COMMON_EXPORT extern const base::Feature
    kDelayCompetingLowPriorityRequests;
enum class DelayCompetingLowPriorityRequestsDelayType {
  kFirstPaint,
  kFirstContentfulPaint,
  kAlways,
  kUseOptimizationGuide,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    DelayCompetingLowPriorityRequestsDelayType>
    kDelayCompetingLowPriorityRequestsDelayParam;
enum class DelayCompetingLowPriorityRequestsThreshold {
  kMedium,
  kHigh,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<
    DelayCompetingLowPriorityRequestsThreshold>
    kDelayCompetingLowPriorityRequestsThresholdParam;

BLINK_COMMON_EXPORT extern const base::Feature kAppCache;
BLINK_COMMON_EXPORT extern const base::Feature kAppCacheRequireOriginTrial;

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

// Enables storing and loading security policies (for now, referrer policy) in
// the policy container. The policy container for the current document is
// attached to the RenderFrameHost and mirrored to the LocalFrame in Blink.
BLINK_COMMON_EXPORT extern const base::Feature kPolicyContainer;

// This flag is used to set field parameters to choose predictor we use when
// kResamplingInputEvents is disabled. It's used for gatherig accuracy metrics
// on finch and also for choosing predictor type for predictedEvents API without
// enabling resampling. It does not have any effect when the resampling flag is
// enabled.
BLINK_COMMON_EXPORT extern const base::Feature kInputPredictorTypeChoice;

// Enables resampling input events on main thread.
BLINK_COMMON_EXPORT extern const base::Feature kResamplingInputEvents;

// Enables resampling GestureScroll events on compositor thread.
// Uses the kPredictorName* values in ui_base_features.h as the 'predictor'
// feature param.
BLINK_COMMON_EXPORT extern const base::Feature kResamplingScrollEvents;

// Enables the device-memory, resource-width, viewport-width and DPR client
// hints to be sent to third-party origins if the first-party has opted in to
// receiving client hints, regardless of Feature Policy.
BLINK_COMMON_EXPORT extern const base::Feature kAllowClientHintsToThirdParty;

// Enables filtering of predicted scroll events on compositor thread.
// Uses the kFilterName* values in ui_base_features.h as the 'filter' feature
// param.
BLINK_COMMON_EXPORT extern const base::Feature kFilteringScrollPrediction;

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

BLINK_COMMON_EXPORT extern const base::Feature kCompressParkableStrings;
BLINK_COMMON_EXPORT extern const base::Feature kParkableStringsToDisk;
BLINK_COMMON_EXPORT bool IsParkableStringsToDiskEnabled();

BLINK_COMMON_EXPORT extern const base::Feature kCrOSAutoSelect;

BLINK_COMMON_EXPORT extern const base::Feature kCompositingOptimizations;

BLINK_COMMON_EXPORT extern const base::Feature kReducedReferrerGranularity;

BLINK_COMMON_EXPORT extern const base::Feature kContentCaptureConstantStreaming;

BLINK_COMMON_EXPORT extern const base::Feature
    kContentCaptureUserActivatedDelay;

BLINK_COMMON_EXPORT extern const base::Feature kCheckOfflineCapability;
enum class CheckOfflineCapabilityMode {
  kWarnOnly,
  kEnforce,
};
BLINK_COMMON_EXPORT extern const base::FeatureParam<CheckOfflineCapabilityMode>
    kCheckOfflineCapabilityParam;

BLINK_COMMON_EXPORT extern const base::Feature
    kBackForwardCacheABExperimentControl;
BLINK_COMMON_EXPORT
extern const char kBackForwardCacheABExperimentGroup[];

BLINK_COMMON_EXPORT extern const base::Feature kPreferCompositingToLCDText;

BLINK_COMMON_EXPORT extern const base::Feature
    kLogUnexpectedIPCPostedToBackForwardCachedDocuments;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableLinkCapturing;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableUrlHandlers;

BLINK_COMMON_EXPORT extern const base::Feature kWebAppEnableProtocolHandlers;

BLINK_COMMON_EXPORT extern const base::Feature kWebRtcLibvpxEncodeNV12;

BLINK_COMMON_EXPORT extern const base::Feature kLoadingTasksUnfreezable;

BLINK_COMMON_EXPORT extern const base::Feature kTargetBlankImpliesNoOpener;

BLINK_COMMON_EXPORT extern const base::Feature
    kMediaStreamTrackUseConfigMaxFrameRate;

BLINK_COMMON_EXPORT extern const base::Feature kCloneSessionStorageForNoOpener;

BLINK_COMMON_EXPORT extern const base::Feature kWebRtcDistinctWorkerThread;

// Performs additional SubresourceFilter checks when CNAME aliases are found
// for the host of a requested URL.
BLINK_COMMON_EXPORT extern const base::Feature
    kSendCnameAliasesToSubresourceFilterFromRenderer;

BLINK_COMMON_EXPORT extern const base::Feature kDeclarativeShadowDOM;

BLINK_COMMON_EXPORT extern const base::Feature kInterestCohortAPIOriginTrial;

BLINK_COMMON_EXPORT extern const base::Feature kInterestCohortFeaturePolicy;

BLINK_COMMON_EXPORT extern const base::Feature kTextFragmentColorChange;

BLINK_COMMON_EXPORT extern const base::Feature kDisableDocumentDomainByDefault;

BLINK_COMMON_EXPORT extern const base::Feature kScopeMemoryCachePerContext;

BLINK_COMMON_EXPORT extern const base::Feature kEnablePenetratingImageSelection;

BLINK_COMMON_EXPORT extern const base::Feature kCLSM90Improvements;

}  // namespace features
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURES_H_
