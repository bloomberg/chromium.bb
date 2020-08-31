// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "device/fido/features.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "net/base/features.h"
#include "services/device/public/cpp/device_features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/blink_features.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using blink::WebRuntimeFeatures;

namespace {

// Sets blink runtime features for specific platforms.
// This should be a last resort vs runtime_enabled_features.json5.
void SetRuntimeFeatureDefaultsForPlatform(
    const base::CommandLine& command_line) {
  // Please consider setting up feature defaults for different platforms
  // in runtime_enabled_features.json5 instead of here
  // TODO(rodneyding): Move the more common cases here
  // to baseFeature/switch functions below and move more complex
  // ones to special case functions.
#if defined(USE_AURA)
  WebRuntimeFeatures::EnableCompositedSelectionUpdate(true);
#endif
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::Version::WIN10)
    WebRuntimeFeatures::EnableWebBluetooth(true);
#endif

#if defined(SUPPORT_WEBGL2_COMPUTE_CONTEXT)
  if (command_line.HasSwitch(switches::kEnableWebGL2ComputeContext)) {
    WebRuntimeFeatures::EnableWebGL2ComputeContext(true);
  }
#endif

#if defined(OS_MACOSX)
  const bool enable_canvas_2d_image_chromium =
      command_line.HasSwitch(
          switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisable2dCanvasImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu) &&
      base::FeatureList::IsEnabled(features::kCanvas2DImageChromium);
#else
  constexpr bool enable_canvas_2d_image_chromium = false;
#endif
  WebRuntimeFeatures::EnableCanvas2dImageChromium(
      enable_canvas_2d_image_chromium);

#if defined(OS_MACOSX)
  const bool enable_web_gl_image_chromium =
      command_line.HasSwitch(
          switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisableWebGLImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu) &&
      base::FeatureList::IsEnabled(features::kWebGLImageChromium);
#else
  const bool enable_web_gl_image_chromium =
      command_line.HasSwitch(switches::kEnableWebGLImageChromium);
#endif
  WebRuntimeFeatures::EnableWebGLImageChromium(enable_web_gl_image_chromium);

#if defined(OS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableMediaSessionAPI))
    WebRuntimeFeatures::EnableMediaSession(false);
#endif

#if defined(OS_ANDROID)
  // APIs for Web Authentication are not available prior to N.
  WebRuntimeFeatures::EnableWebAuth(
      base::FeatureList::IsEnabled(features::kWebAuth) &&
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_NOUGAT);
#else
  WebRuntimeFeatures::EnableWebAuth(
      base::FeatureList::IsEnabled(features::kWebAuth));
#endif

#if defined(OS_ANDROID)
  WebRuntimeFeatures::EnablePictureInPictureAPI(
      base::FeatureList::IsEnabled(media::kPictureInPictureAPI));
#endif

#if defined(OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P) {
    // Display Cutout is limited to Android P+.
    WebRuntimeFeatures::EnableDisplayCutoutAPI(true);
  }
#endif

#if defined(OS_ANDROID)
  WebRuntimeFeatures::EnableMediaControlsExpandGesture(
      base::FeatureList::IsEnabled(media::kMediaControlsExpandGesture));
#endif
}

enum RuntimeFeatureEnableOptions {
  // Always set the Blink feature to the enabled state of the base::Feature.
  // Example: A run time feature that is completely controlled
  // by base::Feature.
  kUseFeatureState,
  // Enables the Blink feature when the base::Feature is enabled,
  // otherwise no change.
  kEnableOnly,
  // Enables the Blink feature when the base::Feature is enabled
  // via an override on the command-line, otherwise no change.
  kEnableOnlyIfOverriddenFromCommandLine,
  // Disables the Blink feature when the base::Feature is *disabled*,
  // otherwise no change.
  kDisableOnly,
};

template <typename T>
// Helper class that describes the desired actions for the runtime feature
// depending on a check for chromium base::Feature.
struct RuntimeFeatureToChromiumFeatureMap {
  // This can be either an enabler function defined in web_runtime_features.cc
  // or the string name of the feature in runtime_enabled_features.json5.
  T feature_enabler;
  // The chromium base::Feature to check.
  const base::Feature& chromium_feature;
  const RuntimeFeatureEnableOptions option;
};

template <typename Enabler>
void SetRuntimeFeatureFromChromiumFeature(const base::Feature& chromium_feature,
                                          RuntimeFeatureEnableOptions option,
                                          const Enabler& enabler) {
  using FeatureList = base::FeatureList;
  const bool feature_enabled = FeatureList::IsEnabled(chromium_feature);
  switch (option) {
    case kEnableOnly:
      if (feature_enabled)
        enabler(true);
      break;
    case kEnableOnlyIfOverriddenFromCommandLine:
      if (FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
              chromium_feature.name, FeatureList::OVERRIDE_ENABLE_FEATURE)) {
        DCHECK(feature_enabled);
        enabler(true);
      }
      break;
    case kDisableOnly:
      if (!feature_enabled)
        enabler(false);
      break;
    case kUseFeatureState:
      enabler(feature_enabled);
      break;
  }
}

// Sets blink runtime features that are either directly
// controlled by Chromium base::Feature or are overridden
// by base::Feature states.
void SetRuntimeFeaturesFromChromiumFeatures() {
  using wf = WebRuntimeFeatures;
  // To add a runtime feature control, add a new
  // RuntimeFeatureToChromiumFeatureMap entry here if there is a custom
  // enabler function defined. Otherwise add the entry with string name
  // in the next list.
  const RuntimeFeatureToChromiumFeatureMap<void (*)(bool)>
      blinkFeatureToBaseFeatureMapping[] =
  {
    // TODO(rodneyding): Sort features in alphabetical order
    {wf::EnableWebUsb, features::kWebUsb, kDisableOnly},
    {wf::EnableBlockingFocusWithoutUserActivation,
     blink::features::kBlockingFocusWithoutUserActivation, kEnableOnly},
    {wf::EnableNotificationContentImage, features::kNotificationContentImage,
     kDisableOnly},
    {wf::EnablePeriodicBackgroundSync, features::kPeriodicBackgroundSync,
     kEnableOnly},
    {wf::EnableWebXR, features::kWebXr, kUseFeatureState},
    {wf::EnableWebXRARModule, features::kWebXrArModule, kUseFeatureState},
    {wf::EnableWebXRHitTest, features::kWebXrHitTest, kUseFeatureState},
    {wf::EnableWebXRIncubations, features::kWebXrIncubations, kEnableOnly},
    {wf::EnableWebXRAnchors, features::kWebXrIncubations, kEnableOnly},
    {wf::EnableWebXRLightEstimation, features::kWebXrIncubations, kEnableOnly},
    {wf::EnableUserActivationPostMessageTransfer,
     features::kUserActivationPostMessageTransfer, kUseFeatureState},
    {wf::EnableUserActivationSameOriginVisibility,
     features::kUserActivationSameOriginVisibility, kUseFeatureState},
    {wf::EnableExpensiveBackgroundTimerThrottling,
     features::kExpensiveBackgroundTimerThrottling, kUseFeatureState},
    {wf::EnableTimerThrottlingForHiddenFrames,
     features::kTimerThrottlingForHiddenFrames, kUseFeatureState},
    {wf::EnableSendBeaconThrowForBlobWithNonSimpleType,
     features::kSendBeaconThrowForBlobWithNonSimpleType, kEnableOnly},
    {wf::EnablePaymentRequest, features::kWebPayments, kUseFeatureState},
    {wf::EnablePaymentHandlerMinimalUI, features::kWebPaymentsMinimalUI,
     kEnableOnly},
    {wf::EnablePaymentApp, features::kServiceWorkerPaymentApps, kEnableOnly},
    {wf::EnableGenericSensorExtraClasses, features::kGenericSensorExtraClasses,
     kEnableOnly},
    {wf::EnableMediaCastOverlayButton, media::kMediaCastOverlayButton,
     kUseFeatureState},
    {wf::EnableLazyInitializeMediaControls,
     features::kLazyInitializeMediaControls, kUseFeatureState},
    {wf::EnableMediaEngagementBypassAutoplayPolicies,
     media::kMediaEngagementBypassAutoplayPolicies, kUseFeatureState},
    {wf::EnableOverflowIconsForMediaControls,
     media::kOverflowIconsForMediaControls, kUseFeatureState},
    {wf::EnableAllowActivationDelegationAttr,
     features::kAllowActivationDelegationAttr, kUseFeatureState},
    {wf::EnableLazyFrameLoading, features::kLazyFrameLoading, kUseFeatureState},
    {wf::EnableLazyFrameVisibleLoadTimeMetrics,
     features::kLazyFrameVisibleLoadTimeMetrics, kUseFeatureState},
    {wf::EnableLazyImageLoading, features::kLazyImageLoading, kUseFeatureState},
    {wf::EnableLazyImageVisibleLoadTimeMetrics,
     features::kLazyImageVisibleLoadTimeMetrics, kUseFeatureState},
    {wf::EnablePictureInPicture, media::kPictureInPicture, kUseFeatureState},
    {wf::EnableCacheInlineScriptCode, features::kCacheInlineScriptCode,
     kUseFeatureState},
    {wf::EnableExperimentalProductivityFeatures,
     features::kExperimentalProductivityFeatures, kEnableOnly},
    {wf::EnableFeaturePolicyForSandbox, features::kFeaturePolicyForSandbox,
     kEnableOnly},
    {wf::EnableAccessibilityExposeARIAAnnotations,
     features::kEnableAccessibilityExposeARIAAnnotations, kEnableOnly},
    {wf::EnableAccessibilityExposeDisplayNone,
     features::kEnableAccessibilityExposeDisplayNone, kEnableOnly},
    {wf::EnableAccessibilityExposeHTMLElement,
     features::kEnableAccessibilityExposeHTMLElement, kUseFeatureState},
    {wf::EnableAllowSyncXHRInPageDismissal,
     blink::features::kAllowSyncXHRInPageDismissal, kEnableOnly},
    {wf::EnableAutoplayIgnoresWebAudio, media::kAutoplayIgnoreWebAudio,
     kUseFeatureState},
    {wf::EnablePortals, blink::features::kPortals,
     kEnableOnlyIfOverriddenFromCommandLine},
    {wf::EnableImplicitRootScroller, blink::features::kImplicitRootScroller,
     kUseFeatureState},
    {wf::EnableCSSOMViewScrollCoordinates,
     blink::features::kCSSOMViewScrollCoordinates, kEnableOnly},
    {wf::EnableTextFragmentAnchor, blink::features::kTextFragmentAnchor,
     kUseFeatureState},
    {wf::EnableBackgroundFetch, features::kBackgroundFetch, kDisableOnly},
    {wf::EnableForcedColors, features::kForcedColors, kUseFeatureState},
    {wf::EnableFractionalScrollOffsets, features::kFractionalScrollOffsets,
     kUseFeatureState},
    {wf::EnableGetDisplayMedia, blink::features::kRTCGetDisplayMedia,
     kUseFeatureState},
    {wf::EnableSignedExchangePrefetchCacheForNavigations,
     features::kSignedExchangePrefetchCacheForNavigations, kUseFeatureState},
    {wf::EnableSignedExchangeSubresourcePrefetch,
     features::kSignedExchangeSubresourcePrefetch, kUseFeatureState},
    {wf::EnableIdleDetection, features::kIdleDetection, kDisableOnly},
    {wf::EnableSkipTouchEventFilter, blink::features::kSkipTouchEventFilter,
     kUseFeatureState},
    {wf::EnableSmsReceiver, features::kSmsReceiver, kDisableOnly},
    {wf::EnableConsolidatedMovementXY, features::kConsolidatedMovementXY,
     kUseFeatureState},
    {wf::EnableCooperativeScheduling, features::kCooperativeScheduling,
     kUseFeatureState},
    {wf::EnableMouseSubframeNoImplicitCapture,
     features::kMouseSubframeNoImplicitCapture, kUseFeatureState},
    {wf::EnableCookieDeprecationMessages, features::kCookieDeprecationMessages,
     kEnableOnly},
    {wf::EnableSameSiteByDefaultCookies,
     net::features::kSameSiteByDefaultCookies, kEnableOnly},
    {wf::EnableCookiesWithoutSameSiteMustBeSecure,
     net::features::kCookiesWithoutSameSiteMustBeSecure, kEnableOnly},
    {wf::EnablePointerLockOptions, features::kPointerLockOptions, kEnableOnly},
    {wf::EnableDocumentPolicy, features::kDocumentPolicy, kUseFeatureState},
    {wf::EnableScrollUnification, features::kScrollUnification,
     kUseFeatureState},
    {wf::EnableNeverSlowMode, features::kNeverSlowMode, kUseFeatureState},
    {wf::EnableShadowDOMV0, blink::features::kWebComponentsV0Enabled,
     kEnableOnly},
    {wf::EnableCustomElementsV0, blink::features::kWebComponentsV0Enabled,
     kEnableOnly},
    {wf::EnableHTMLImports, blink::features::kWebComponentsV0Enabled,
     kEnableOnly},
    {wf::EnableVideoPlaybackQuality, features::kVideoPlaybackQuality,
     kUseFeatureState},
    {wf::EnableBrowserVerifiedUserActivationKeyboard,
     features::kBrowserVerifiedUserActivationKeyboard, kEnableOnly},
    {wf::EnableBrowserVerifiedUserActivationMouse,
     features::kBrowserVerifiedUserActivationMouse, kEnableOnly},
    {wf::EnablePercentBasedScrolling, features::kPercentBasedScrolling,
     kUseFeatureState},
#if defined(OS_ANDROID)
    {wf::EnableWebNfc, features::kWebNfc, kDisableOnly},
#endif
    {wf::EnableInstalledApp, features::kInstalledApp, kDisableOnly},
    {wf::EnableWebAuthenticationGetAssertionFeaturePolicy,
     device::kWebAuthGetAssertionFeaturePolicy, kUseFeatureState},
    {wf::EnableVideoWakeLockOptimisationHiddenMuted,
     media::kWakeLockOptimisationHiddenMuted, kUseFeatureState},
  };
  for (const auto& mapping : blinkFeatureToBaseFeatureMapping) {
    SetRuntimeFeatureFromChromiumFeature(
        mapping.chromium_feature, mapping.option, mapping.feature_enabler);
  }

  // TODO(crbug/832393): Cleanup the inconsistency between custom WRF enabler
  // function and using feature string name with EnableFeatureFromString.
  const RuntimeFeatureToChromiumFeatureMap<const char*>
      runtimeFeatureNameToChromiumFeatureMapping[] = {
          {"AddressSpace", network::features::kBlockNonSecureExternalRequests,
           kEnableOnly},
          {"AllowContentInitiatedDataUrlNavigations",
           features::kAllowContentInitiatedDataUrlNavigations,
           kUseFeatureState},
          {"AudioWorkletRealtimeThread",
           blink::features::kAudioWorkletRealtimeThread, kEnableOnly},
          {"BlockCredentialedSubresources",
           features::kBlockCredentialedSubresources, kDisableOnly},
          {"BlockHTMLParserOnStyleSheets",
           blink::features::kBlockHTMLParserOnStyleSheets, kUseFeatureState},
          {"ConversionMeasurement", features::kConversionMeasurement,
           kEnableOnly},
          {"CSSReducedFontLoadingInvalidations",
           blink::features::kCSSReducedFontLoadingInvalidations,
           kUseFeatureState},
          {"CustomElementsV0", blink::features::kWebComponentsV0Enabled,
           kEnableOnly},
          {"FeaturePolicyForClientHints",
           features::kFeaturePolicyForClientHints, kUseFeatureState},
          {"FlexGaps", blink::features::kFlexGaps, kEnableOnly},
          {"FontAccess", blink::features::kFontAccess, kUseFeatureState},
          {"FontSrcLocalMatching", features::kFontSrcLocalMatching,
           kUseFeatureState},
          {"HTMLImports", blink::features::kWebComponentsV0Enabled,
           kEnableOnly},
          {"IgnoreCrossOriginWindowWhenNamedAccessOnWindow",
           blink::features::kIgnoreCrossOriginWindowWhenNamedAccessOnWindow,
           kEnableOnly},
          {"LayoutNG", blink::features::kLayoutNG, kUseFeatureState},
          {"LayoutNGFlexBox", blink::features::kFlexNG, kEnableOnly},
          {"LegacyWindowsDWriteFontFallback",
           features::kLegacyWindowsDWriteFontFallback, kUseFeatureState},
          {"OriginPolicy", features::kOriginPolicy, kUseFeatureState},
          {"OriginIsolationHeader", features::kOriginIsolationHeader,
           kUseFeatureState},
          {"RawClipboard", blink::features::kRawClipboard, kEnableOnly},
          {"ShadowDOMV0", blink::features::kWebComponentsV0Enabled,
           kEnableOnly},
          {"StorageAccessAPI", blink::features::kStorageAccessAPI, kEnableOnly},
          {"TrustedDOMTypes", features::kTrustedDOMTypes, kEnableOnly},
          {"UserAgentClientHint", features::kUserAgentClientHint,
           kUseFeatureState},

      };
  for (const auto& mapping : runtimeFeatureNameToChromiumFeatureMapping) {
    SetRuntimeFeatureFromChromiumFeature(
        mapping.chromium_feature, mapping.option, [&mapping](bool enabled) {
          wf::EnableFeatureFromString(mapping.feature_enabler, enabled);
        });
  }
}

// Helper class that describes the desired enable/disable action
// for a runtime feature when a command line switch exists.
struct SwitchToFeatureMap {
  // The enabler function defined in web_runtime_features.cc.
  void (*feature_enabler)(bool);
  // The switch to check for on command line.
  const char* switch_name;
  // This is the desired state for the runtime feature if the
  // switch exists on command line.
  bool target_enabled_state;
};

// Sets blink runtime features controlled by command line switches.
void SetRuntimeFeaturesFromCommandLine(const base::CommandLine& command_line) {
  // To add a new switch-controlled runtime feature, add a new
  // SwitchToFeatureMap entry to the initializer list below.
  // Note: command line switches are now discouraged, please consider
  // using base::Feature instead.
  // https://chromium.googlesource.com/chromium/src/+/refs/heads/master/docs/configuration.md#switches
  using wrf = WebRuntimeFeatures;
  const SwitchToFeatureMap switchToFeatureMapping[] = {
      // Stable Features
      {wrf::EnablePermissionsAPI, switches::kDisablePermissionsAPI, false},
      {wrf::EnablePresentationAPI, switches::kDisablePresentationAPI, false},
      {wrf::EnableRemotePlaybackAPI, switches::kDisableRemotePlaybackAPI,
       false},
      {wrf::EnableTimerThrottlingForBackgroundTabs,
       switches::kDisableBackgroundTimerThrottling, false},
      // End of Stable Features
      {wrf::EnableDatabase, switches::kDisableDatabases, false},
      {wrf::EnableNotifications, switches::kDisableNotifications, false},
      // Chrome's Push Messaging implementation relies on Web Notifications.
      {wrf::EnablePushMessaging, switches::kDisableNotifications, false},
      {wrf::EnableSharedWorker, switches::kDisableSharedWorkers, false},
      {wrf::EnableScriptedSpeechRecognition, switches::kDisableSpeechAPI,
       false},
      {wrf::EnableScriptedSpeechSynthesis, switches::kDisableSpeechAPI, false},
      {wrf::EnableScriptedSpeechSynthesis, switches::kDisableSpeechSynthesisAPI,
       false},
      {wrf::EnableFileSystem, switches::kDisableFileSystem, false},
      {wrf::EnableWebGLDraftExtensions, switches::kEnableWebGLDraftExtensions,
       true},
      {wrf::EnableAutomationControlled, switches::kEnableAutomation, true},
      {wrf::EnableAutomationControlled, switches::kHeadless, true},
      {wrf::EnableAutomationControlled, switches::kRemoteDebuggingPipe, true},
      {wrf::EnableAutomationControlled, switches::kRemoteDebuggingPort, true},
      {wrf::ForceOverlayFullscreenVideo, switches::kForceOverlayFullscreenVideo,
       true},
      {wrf::EnablePreciseMemoryInfo, switches::kEnablePreciseMemoryInfo, true},
      {wrf::EnableNetInfoDownlinkMax,
       switches::kEnableNetworkInformationDownlinkMax, true},
      {wrf::EnablePermissionsAPI, switches::kDisablePermissionsAPI, false},
      {wrf::EnableWebGPU, switches::kEnableUnsafeWebGPU, true},
      {wrf::EnablePresentationAPI, switches::kDisablePresentationAPI, false},
      {wrf::EnableTextFragmentAnchor, switches::kDisableScrollToTextFragment,
       false},
      {wrf::EnableRemotePlaybackAPI, switches::kDisableRemotePlaybackAPI,
       false},
      {wrf::EnableTimerThrottlingForBackgroundTabs,
       switches::kDisableBackgroundTimerThrottling, false},
      {wrf::EnableAccessibilityObjectModel,
       switches::kEnableAccessibilityObjectModel, true},
      {wrf::EnableAllowSyncXHRInPageDismissal,
       switches::kAllowSyncXHRInPageDismissal, true},
      {wrf::EnableShadowDOMV0, switches::kWebComponentsV0Enabled, true},
      {wrf::EnableCustomElementsV0, switches::kWebComponentsV0Enabled, true},
      {wrf::EnableHTMLImports, switches::kWebComponentsV0Enabled, true},
  };
  for (const auto& mapping : switchToFeatureMapping) {
    if (command_line.HasSwitch(mapping.switch_name))
      mapping.feature_enabler(mapping.target_enabled_state);
  }
}

// Sets blink runtime features controlled by FieldTrial parameter values.
void SetRuntimeFeaturesFromFieldTrialParams() {
  // Automatic lazy frame loading by default is enabled and restricted to users
  // with Lite Mode (aka Data Saver) turned on. Note that in practice, this also
  // restricts automatic lazy loading by default to Android, since Lite Mode is
  // only accessible through UI on Android.
  WebRuntimeFeatures::EnableAutomaticLazyFrameLoading(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyFrameLoading, "automatic-lazy-load-frames-enabled",
          true));
  WebRuntimeFeatures::EnableRestrictAutomaticLazyFrameLoadingToDataSaver(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyFrameLoading,
          "restrict-lazy-load-frames-to-data-saver-only", true));
  WebRuntimeFeatures::EnableAutoLazyLoadOnReloads(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyFrameLoading, "enable-lazy-load-on-reload", false));

  // Automatic lazy image loading by default is enabled and restricted to users
  // with Lite Mode (aka Data Saver) turned on. Note that in practice, this also
  // restricts automatic lazy loading by default to Android, since Lite Mode is
  // only accessible through UI on Android.
  WebRuntimeFeatures::EnableAutomaticLazyImageLoading(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyImageLoading, "automatic-lazy-load-images-enabled",
          true));
  WebRuntimeFeatures::EnableRestrictAutomaticLazyImageLoadingToDataSaver(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kLazyImageLoading,
          "restrict-lazy-load-images-to-data-saver-only", true));
}

// Sets blink runtime features that depend on a combination
// of args rather than a single check of base::Feature or switch.
// This can be a combination of both or custom checking logic
// not covered by other functions. In short, this should be used
// as a last resort.
void SetCustomizedRuntimeFeaturesFromCombinedArgs(
    const base::CommandLine& command_line,
    bool enable_experimental_web_platform_features) {
  // CAUTION: Only add custom enabling logic here if it cannot
  // be covered by the other functions.

  if (!command_line.HasSwitch(switches::kDisableYUVImageDecoding) &&
      base::FeatureList::IsEnabled(
          blink::features::kDecodeJpeg420ImagesToYUV)) {
    WebRuntimeFeatures::EnableDecodeJpeg420ImagesToYUV(true);
  }
  if (!command_line.HasSwitch(switches::kDisableYUVImageDecoding) &&
      base::FeatureList::IsEnabled(
          blink::features::kDecodeLossyWebPImagesToYUV)) {
    WebRuntimeFeatures::EnableDecodeLossyWebPImagesToYUV(true);
  }

  WebRuntimeFeatures::EnableSharedArrayBuffer(
      base::FeatureList::IsEnabled(features::kSharedArrayBuffer) ||
      base::FeatureList::IsEnabled(features::kWebAssemblyThreads));

  // These checks are custom wrappers around base::FeatureList::IsEnabled
  // They're moved here to distinguish them from actual base checks
  WebRuntimeFeatures::EnableOverlayScrollbars(ui::IsOverlayScrollbarEnabled());

  if (base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          blink::features::kNativeFileSystemAPI.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    WebRuntimeFeatures::EnableFeatureFromString("NativeFileSystem", true);
    WebRuntimeFeatures::EnableFeatureFromString(
        "CloneableNativeFileSystemHandles", true);
    WebRuntimeFeatures::EnableFeatureFromString("WritableFileStream", true);
  }
  if (base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI) &&
      base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI)) {
    WebRuntimeFeatures::EnableFeatureFromString("FileHandling", true);
  }

  // TODO(rodneyding): This is a rare case for a stable feature
  // Need to investigate more to determine whether to refactor it.
  if (command_line.HasSwitch(switches::kDisableV8IdleTasks))
    WebRuntimeFeatures::EnableV8IdleTasks(false);
  else
    WebRuntimeFeatures::EnableV8IdleTasks(true);

  // This is a hack to get the tests passing as they require
  // these blink features to be enabled while they are disabled
  // by base::Feature controls earlier in code.
  // TODO(rodneyding): Investigate more on proper treatments of
  // these features.
  if (enable_experimental_web_platform_features) {
    WebRuntimeFeatures::EnableNetInfoDownlinkMax(true);
  }

  WebRuntimeFeatures::EnableBackForwardCache(
      content::IsBackForwardCacheEnabled());

  // Gate the ReducedReferrerGranularity runtime feature depending on whether
  // content is configured to force a no-referrer-when-downgrade default policy.
  // TODO(crbug.com/1016541): After M82, remove when the corresponding
  // enterprise policy has been deleted.
  WebRuntimeFeatures::EnableReducedReferrerGranularity(
      base::FeatureList::IsEnabled(features::kReducedReferrerGranularity) &&
      !content::Referrer::ShouldForceLegacyDefaultReferrerPolicy());

  if (base::FeatureList::IsEnabled(
          blink::features::kAppCacheRequireOriginTrial)) {
    // The kAppCacheRequireOriginTrial is a flag that controls whether or not
    // the renderer AppCache api and backend is gated by an origin trial.  If
    // on, then AppCache is disabled but can be re-enabled by the origin trial.
    // The origin trial will not turn on the feature if the base::Feature
    // AppCache is disabled.
    WebRuntimeFeatures::EnableFeatureFromString("AppCache", false);
  } else {
    // If the origin trial is not required, then the kAppCache feature /
    // about:flag is a disable-only kill switch to allow developers to test
    // their application with AppCache fully disabled.
    if (!base::FeatureList::IsEnabled(blink::features::kAppCache))
      WebRuntimeFeatures::EnableFeatureFromString("AppCache", false);
  }

  if (base::FeatureList::IsEnabled(network::features::kTrustTokens)) {
    // See https://bit.ly/configuring-trust-tokens.
    using network::features::TrustTokenOriginTrialSpec;
    switch (
        network::features::kTrustTokenOperationsRequiringOriginTrial.Get()) {
      case TrustTokenOriginTrialSpec::kOriginTrialNotRequired:
        // Setting TrustTokens=true enables the Trust Tokens interface;
        // TrustTokensAlwaysAllowIssuance disables a runtime check during
        // issuance that the origin trial is active (see
        // blink/.../trust_token_issuance_authorization.h).
        WebRuntimeFeatures::EnableTrustTokens(true);
        WebRuntimeFeatures::EnableTrustTokensAlwaysAllowIssuance(true);
        break;
      case TrustTokenOriginTrialSpec::kAllOperationsRequireOriginTrial:
        // The origin trial itself will be responsible for enabling the
        // TrustTokens RuntimeEnabledFeature.
        WebRuntimeFeatures::EnableTrustTokens(false);
        WebRuntimeFeatures::EnableTrustTokensAlwaysAllowIssuance(false);
        break;
      case TrustTokenOriginTrialSpec::kOnlyIssuanceRequiresOriginTrial:
        // At issuance, a runtime check will be responsible for checking that
        // the origin trial is present.
        WebRuntimeFeatures::EnableTrustTokens(true);
        WebRuntimeFeatures::EnableTrustTokensAlwaysAllowIssuance(false);
        break;
    }
  }
}

}  // namespace

namespace content {

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const base::CommandLine& command_line) {
  // Sets experimental features.
  bool enable_experimental_web_platform_features =
      command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  if (enable_experimental_web_platform_features)
    WebRuntimeFeatures::EnableExperimentalFeatures(true);

  SetRuntimeFeatureDefaultsForPlatform(command_line);

  // Sets origin trial features.
  if (command_line.HasSwitch(
          switches::kDisableOriginTrialControlledBlinkFeatures)) {
    WebRuntimeFeatures::EnableOriginTrialControlledFeatures(false);
  }

  if (!command_line.HasSwitch(
          network::switches::kForceToDisableOutOfBlinkCors) &&
      base::FeatureList::IsEnabled(network::features::kOutOfBlinkCors)) {
    WebRuntimeFeatures::EnableOutOfBlinkCors(true);
  }

  // TODO(rodneyding): add doc explaining ways to add new runtime features
  // controls in the following functions.

  SetRuntimeFeaturesFromChromiumFeatures();

  SetRuntimeFeaturesFromCommandLine(command_line);

  SetRuntimeFeaturesFromFieldTrialParams();

  SetCustomizedRuntimeFeaturesFromCombinedArgs(
      command_line, enable_experimental_web_platform_features);

  // Enable explicitly enabled features, and then disable explicitly disabled
  // ones.
  for (const std::string& feature :
       FeaturesFromSwitch(command_line, switches::kEnableBlinkFeatures)) {
    WebRuntimeFeatures::EnableFeatureFromString(feature, true);
  }
  for (const std::string& feature :
       FeaturesFromSwitch(command_line, switches::kDisableBlinkFeatures)) {
    WebRuntimeFeatures::EnableFeatureFromString(feature, false);
  }
}

}  // namespace content
