// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/delegated_ink_prediction_configuration.h"
#include "components/viz/common/switches.h"
#include "components/viz/common/viz_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/media_buildflags.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace {

// FieldTrialParams for `DynamicSchedulerForDraw` and
// `kDynamicSchedulerForClients`.
const char kDynamicSchedulerPercentile[] = "percentile";

}  // namespace

namespace features {

// Enables the use of power hint APIs on Android.
const base::Feature kAdpf{"Adpf", base::FEATURE_DISABLED_BY_DEFAULT};

// Target duration used for power hint on Android.
const base::FeatureParam<int> kAdpfTargetDurationMs{&kAdpf,
                                                    "AdpfTargetDurationMs", 12};

const base::Feature kEnableOverlayPrioritization {
  "EnableOverlayPrioritization",
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kDelegatedCompositing{"DelegatedCompositing",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSimpleFrameRateThrottling{
    "SimpleFrameRateThrottling", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SkiaRenderer.
const base::Feature kUseSkiaRenderer {
  "UseSkiaRenderer",
#if defined(OS_WIN) || defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    defined(OS_LINUX) || defined(OS_FUCHSIA) || defined(OS_MAC)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Kill-switch to disable de-jelly, even if flags/properties indicate it should
// be enabled.
const base::Feature kDisableDeJelly{"DisableDeJelly",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// On platform and configuration where viz controls the allocation of frame
// buffers (ie SkiaOutputDeviceBufferQueue is used), allocate and release frame
// buffers on demand.
const base::Feature kDynamicBufferQueueAllocation{
    "DynamicBufferQueueAllocation", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// When wide color gamut content from the web is encountered, promote our
// display to wide color gamut if supported.
const base::Feature kDynamicColorGamut{"DynamicColorGamut",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Uses glClear to composite solid color quads whenever possible.
const base::Feature kFastSolidColorDraw{"FastSolidColorDraw",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Submit CompositorFrame from SynchronousLayerTreeFrameSink directly to viz in
// WebView.
const base::Feature kVizFrameSubmissionForWebView{
    "VizFrameSubmissionForWebView", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUsePreferredIntervalForVideo{
  "UsePreferredIntervalForVideo",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Whether we should use the real buffers corresponding to overlay candidates in
// order to do a pageflip test rather than allocating test buffers.
const base::Feature kUseRealBuffersForPageFlipTest{
    "UseRealBuffersForPageFlipTest", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_FUCHSIA)
// Enables SkiaOutputDeviceBufferQueue instead of Vulkan swapchain on Fuchsia.
const base::Feature kUseSkiaOutputDeviceBufferQueue{
    "UseSkiaOutputDeviceBufferQueue", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Whether we should log extra debug information to webrtc native log.
const base::Feature kWebRtcLogCapturePipeline{
    "WebRtcLogCapturePipeline", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Enables swap chains to call SetPresentDuration to request DWM/OS to reduce
// vsync.
const base::Feature kUseSetPresentDuration{"UseSetPresentDuration",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // OS_WIN

// Enables platform supported delegated ink trails instead of Skia backed
// delegated ink trails.
const base::Feature kUsePlatformDelegatedInk{"UsePlatformDelegatedInk",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Used to debug Android WebView Vulkan composite. Composite to an intermediate
// buffer and draw the intermediate buffer to the secondary command buffer.
const base::Feature kWebViewVulkanIntermediateBuffer{
    "WebViewVulkanIntermediateBuffer", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Hardcoded as disabled for WebView to have a different default for
// UseSurfaceLayerForVideo from chrome.
const base::Feature kUseSurfaceLayerForVideoDefault{
    "UseSurfaceLayerForVideoDefault", base::FEATURE_ENABLED_BY_DEFAULT};

// Historically media on android hardcoded SRGB color space because of lack of
// color space support in surface control. This controls if we want to use real
// color space in DisplayCompositor.
const base::Feature kUseRealVideoColorSpaceForDisplay{
    "UseRealVideoColorSpaceForDisplay", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Used by CC to throttle frame production of older surfaces. Used by the
// Browser to batch SurfaceSync calls sent to the Renderer for properties can
// change in close proximity to each other.
const base::Feature kSurfaceSyncThrottling{"SurfaceSyncThrottling",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDrawPredictedInkPoint{"DrawPredictedInkPoint",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
const char kDraw1Point12Ms[] = "1-pt-12ms";
const char kDraw2Points6Ms[] = "2-pt-6ms";
const char kDraw1Point6Ms[] = "1-pt-6ms";
const char kDraw2Points3Ms[] = "2-pt-3ms";
const char kPredictorKalman[] = "kalman";
const char kPredictorLinearResampling[] = "linear-resampling";
const char kPredictorLinear1[] = "linear-1";
const char kPredictorLinear2[] = "linear-2";
const char kPredictorLsq[] = "lsq";

// Used by Viz to parameterize adjustments to scheduler deadlines.
const base::Feature kDynamicSchedulerForDraw{"DynamicSchedulerForDraw",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
// User to parameterize adjustments to clients' deadlines.
const base::Feature kDynamicSchedulerForClients{
    "DynamicSchedulerForClients", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_MAC)
const base::Feature kMacCAOverlayQuad{"MacCAOverlayQuads",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
// The maximum supported overlay quad number on Mac CALayerOverlay.
// The default is set to -1. When MaxNum is < 0, the default in CALayerOverlay
// will be used instead.
const base::FeatureParam<int> kMacCAOverlayQuadMaxNum{
    &kMacCAOverlayQuad, "MacCAOverlayQuadMaxNum", -1};
#endif

bool IsAdpfEnabled() {
  // TODO(crbug.com/1157620): Limit this to correct android version.
  return base::FeatureList::IsEnabled(kAdpf);
}

bool IsClipPrewalkDamageEnabled() {
  static constexpr base::Feature kClipPrewalkDamage{
      "ClipPrewalkDamage", base::FEATURE_DISABLED_BY_DEFAULT};

  return base::FeatureList::IsEnabled(kClipPrewalkDamage);
}

bool IsOverlayPrioritizationEnabled() {
  return base::FeatureList::IsEnabled(kEnableOverlayPrioritization);
}

bool IsDelegatedCompositingEnabled() {
  return base::FeatureList::IsEnabled(kDelegatedCompositing);
}

// If a synchronous IPC should used when destroying windows. This exists to test
// the impact of removing the sync IPC.
bool IsSyncWindowDestructionEnabled() {
  static constexpr base::Feature kSyncWindowDestruction{
      "SyncWindowDestruction", base::FEATURE_ENABLED_BY_DEFAULT};

  return base::FeatureList::IsEnabled(kSyncWindowDestruction);
}

bool IsSimpleFrameRateThrottlingEnabled() {
  return base::FeatureList::IsEnabled(kSimpleFrameRateThrottling);
}

bool IsUsingSkiaRenderer() {
#if defined(OS_ANDROID)
  // We don't support KitKat. Check for it before looking at the feature flag
  // so that KitKat doesn't show up in Control or Enabled experiment group.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <=
      base::android::SDK_VERSION_KITKAT)
    return false;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(https://crbug.com/1145180): SkiaRenderer isn't supported on Chrome
  // OS boards that still use the legacy video decoder.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kPlatformDisallowsChromeOSDirectVideoDecoder))
    return false;
#endif

  return base::FeatureList::IsEnabled(kUseSkiaRenderer) ||
         features::IsUsingVulkan();
}

#if defined(OS_ANDROID)
bool IsDynamicColorGamutEnabled() {
  if (viz::AlwaysUseWideColorGamut())
    return false;
  auto* build_info = base::android::BuildInfo::GetInstance();
  if (build_info->sdk_int() < base::android::SDK_VERSION_Q)
    return false;
  return base::FeatureList::IsEnabled(kDynamicColorGamut);
}
#endif

bool IsUsingFastPathForSolidColorQuad() {
  return base::FeatureList::IsEnabled(kFastSolidColorDraw);
}

bool IsUsingVizFrameSubmissionForWebView() {
  return base::FeatureList::IsEnabled(kVizFrameSubmissionForWebView);
}

bool IsUsingPreferredIntervalForVideo() {
  return base::FeatureList::IsEnabled(kUsePreferredIntervalForVideo);
}

bool IsVizHitTestingDebugEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVizHitTestDebug);
}

bool ShouldUseRealBuffersForPageFlipTest() {
  return base::FeatureList::IsEnabled(kUseRealBuffersForPageFlipTest);
}

bool ShouldWebRtcLogCapturePipeline() {
  return base::FeatureList::IsEnabled(kWebRtcLogCapturePipeline);
}

#if defined(OS_WIN)
bool ShouldUseSetPresentDuration() {
  return base::FeatureList::IsEnabled(kUseSetPresentDuration);
}
#endif  // OS_WIN

absl::optional<int> ShouldDrawPredictedInkPoints() {
  if (!base::FeatureList::IsEnabled(kDrawPredictedInkPoint))
    return absl::nullopt;

  std::string predicted_points = GetFieldTrialParamValueByFeature(
      kDrawPredictedInkPoint, "predicted_points");
  if (predicted_points == kDraw1Point12Ms)
    return viz::PredictionConfig::k1Point12Ms;
  else if (predicted_points == kDraw2Points6Ms)
    return viz::PredictionConfig::k2Points6Ms;
  else if (predicted_points == kDraw1Point6Ms)
    return viz::PredictionConfig::k1Point6Ms;
  else if (predicted_points == kDraw2Points3Ms)
    return viz::PredictionConfig::k2Points3Ms;

  NOTREACHED();
  return absl::nullopt;
}

std::string InkPredictor() {
  if (!base::FeatureList::IsEnabled(kDrawPredictedInkPoint))
    return "";

  return GetFieldTrialParamValueByFeature(kDrawPredictedInkPoint, "predictor");
}

bool ShouldUsePlatformDelegatedInk() {
  return base::FeatureList::IsEnabled(kUsePlatformDelegatedInk);
}

#if defined(OS_ANDROID)
bool UseSurfaceLayerForVideo() {
  // Allow enabling UseSurfaceLayerForVideo if webview is using surface control.
  if (::features::IsAndroidSurfaceControlEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kUseSurfaceLayerForVideoDefault);
}

bool UseRealVideoColorSpaceForDisplay() {
  // We need Android S for proper color space support in SurfaceControl.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_S)
    return false;

  return base::FeatureList::IsEnabled(
      features::kUseRealVideoColorSpaceForDisplay);
}
#endif

bool IsSurfaceSyncThrottling() {
  return base::FeatureList::IsEnabled(kSurfaceSyncThrottling);
}

// Used by Viz to determine if viz::DisplayScheduler should dynamically adjust
// its frame deadline. Returns the percentile of historic draw times to base the
// deadline on. Or absl::nullopt if the feature is disabled.
absl::optional<double> IsDynamicSchedulerEnabledForDraw() {
  if (!base::FeatureList::IsEnabled(kDynamicSchedulerForDraw))
    return absl::nullopt;
  double result = base::GetFieldTrialParamByFeatureAsDouble(
      kDynamicSchedulerForDraw, kDynamicSchedulerPercentile, -1.0);
  if (result < 0.0)
    return absl::nullopt;
  return result;
}

// Used by Viz to determine if the frame deadlines provided to CC should be
// dynamically adjusted. Returns the percentile of historic draw times to base
// the deadline on. Or absl::nullopt if the feature is disabled.
absl::optional<double> IsDynamicSchedulerEnabledForClients() {
  if (!base::FeatureList::IsEnabled(kDynamicSchedulerForClients))
    return absl::nullopt;
  double result = base::GetFieldTrialParamByFeatureAsDouble(
      kDynamicSchedulerForClients, kDynamicSchedulerPercentile, -1.0);
  if (result < 0.0)
    return absl::nullopt;
  return result;
}

}  // namespace features
