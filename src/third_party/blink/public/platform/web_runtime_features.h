/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RUNTIME_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RUNTIME_FEATURES_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

#include <string>

namespace blink {
// This class is used to enable runtime features of Blink.
// Stable features are enabled by default.
class WebRuntimeFeatures {
 public:
  // Enable or disable features with status=experimental listed in
  // Source/platform/runtime_enabled_features.json5.
  BLINK_PLATFORM_EXPORT static void EnableExperimentalFeatures(bool);

  // Enable or disable features with status=test listed in
  // Source/platform/runtime_enabled_features.json5.
  BLINK_PLATFORM_EXPORT static void EnableTestOnlyFeatures(bool);

  // Enable or disable features with non-empty origin_trial_feature_name in
  // Source/platform/runtime_enabled_features.json5.
  BLINK_PLATFORM_EXPORT static void EnableOriginTrialControlledFeatures(bool);

  // Enables or disables a feature by its string identifier from
  // Source/platform/runtime_enabled_features.json5.
  // Note: We use std::string instead of WebString because this API can
  // be called before blink::Initalize(). We can't create WebString objects
  // before blink::Initialize().
  BLINK_PLATFORM_EXPORT static void EnableFeatureFromString(
      const std::string& name,
      bool enable);

  BLINK_PLATFORM_EXPORT static void EnableForcedColors(bool);

  BLINK_PLATFORM_EXPORT static void EnableFractionalScrollOffsets(bool);

  BLINK_PLATFORM_EXPORT static void EnableCompositedSelectionUpdate(bool);

  BLINK_PLATFORM_EXPORT static void EnableAOMAriaRelationshipProperties(bool);
  BLINK_PLATFORM_EXPORT static void EnableAccelerated2dCanvas(bool);
  BLINK_PLATFORM_EXPORT static void EnableAccessibilityAriaVirtualContent(bool);
  BLINK_PLATFORM_EXPORT static void EnableAccessibilityExposeDisplayNone(bool);
  BLINK_PLATFORM_EXPORT static void EnableAccessibilityExposeHTMLElement(bool);
  BLINK_PLATFORM_EXPORT static void EnableAccessibilityExposeIgnoredNodes(bool);
  BLINK_PLATFORM_EXPORT static void EnableAccessibilityObjectModel(bool);
  BLINK_PLATFORM_EXPORT static void EnableAccessibilityPageZoom(bool);
  BLINK_PLATFORM_EXPORT static void
  EnableAccessibilityUseAXPositionForDocumentMarkers(bool);
  BLINK_PLATFORM_EXPORT static void EnableAdTagging(bool);
  BLINK_PLATFORM_EXPORT static void EnableAllowActivationDelegationAttr(bool);
  BLINK_PLATFORM_EXPORT static void EnableBackgroundFetch(bool);
  BLINK_PLATFORM_EXPORT static void EnableBrowserVerifiedUserActivationKeyboard(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableBrowserVerifiedUserActivationMouse(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableBlockingFocusWithoutUserActivation(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableCacheInlineScriptCode(bool);
  BLINK_PLATFORM_EXPORT static void EnableCapabilityDelegationPaymentRequest(
      bool enable);
  BLINK_PLATFORM_EXPORT static void EnableClickPointerEvent(bool enable);
  BLINK_PLATFORM_EXPORT static void EnableCompositeBGColorAnimation(
      bool enable);
  BLINK_PLATFORM_EXPORT static void EnableCanvas2dImageChromium(bool);
  BLINK_PLATFORM_EXPORT static void EnableCooperativeScheduling(bool);
  BLINK_PLATFORM_EXPORT static void EnableCSSHexAlphaColor(bool);
  BLINK_PLATFORM_EXPORT static void EnableScrollTopLeftInterop(bool);
  BLINK_PLATFORM_EXPORT static void EnableKeyboardAccessibleTooltip(bool);
  BLINK_PLATFORM_EXPORT static void EnableKeyboardFocusableScrollers(bool);
  BLINK_PLATFORM_EXPORT static void EnableDatabase(bool);
  BLINK_PLATFORM_EXPORT static void EnableDecodeJpeg420ImagesToYUV(bool);
  BLINK_PLATFORM_EXPORT static void EnableDecodeLossyWebPImagesToYUV(bool);
  BLINK_PLATFORM_EXPORT static void EnableDevicePosture(bool enable);
  BLINK_PLATFORM_EXPORT static void EnableRemoveMobileViewportDoubleTap(
      bool enable);
  BLINK_PLATFORM_EXPORT static void EnableDisplayCutoutAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnableDocumentPolicy(bool);
  BLINK_PLATFORM_EXPORT static void EnableDocumentPolicyNegotiation(bool);
  BLINK_PLATFORM_EXPORT static void EnableFencedFrames(bool);
  BLINK_PLATFORM_EXPORT static bool IsFencedFramesEnabled();
  BLINK_PLATFORM_EXPORT static void EnableFileSystem(bool);
  BLINK_PLATFORM_EXPORT static void EnableGenericSensorExtraClasses(bool);
  BLINK_PLATFORM_EXPORT static void EnableImplicitRootScroller(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyFrameLoading(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyFrameVisibleLoadTimeMetrics(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyImageLoading(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyImageVisibleLoadTimeMetrics(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaSession(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaSessionWebRTC(bool);
  BLINK_PLATFORM_EXPORT static void EnableNetInfoDownlinkMax(bool);
  BLINK_PLATFORM_EXPORT static void EnableNeverSlowMode(bool);
  BLINK_PLATFORM_EXPORT static void EnableNotificationContentImage(bool);
  BLINK_PLATFORM_EXPORT static void EnableNotifications(bool);
  BLINK_PLATFORM_EXPORT static void EnableOverlayScrollbars(bool);
  BLINK_PLATFORM_EXPORT static void EnablePaymentApp(bool);
  BLINK_PLATFORM_EXPORT static void EnablePaymentRequest(bool);
  BLINK_PLATFORM_EXPORT static void EnablePaymentRequestBasicCard(bool);
  BLINK_PLATFORM_EXPORT static void EnablePaymentRequestRequiresUserActivation(
      bool);
  BLINK_PLATFORM_EXPORT static void EnablePercentBasedScrolling(bool);
  BLINK_PLATFORM_EXPORT static void EnablePerformanceManagerInstrumentation(
      bool);
  BLINK_PLATFORM_EXPORT static void EnablePeriodicBackgroundSync(bool);
  BLINK_PLATFORM_EXPORT static void EnablePermissionsAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnablePictureInPicture(bool);
  BLINK_PLATFORM_EXPORT static void EnablePictureInPictureAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnablePointerLockOptions(bool);
  BLINK_PLATFORM_EXPORT static void EnablePortals(bool);
  BLINK_PLATFORM_EXPORT static bool IsPortalsEnabled();
  BLINK_PLATFORM_EXPORT static void EnablePreciseMemoryInfo(bool);
  BLINK_PLATFORM_EXPORT static void EnablePrerender2(bool);
  BLINK_PLATFORM_EXPORT static bool IsPrerender2Enabled();
  BLINK_PLATFORM_EXPORT static void EnablePrerender2RelatedFeatures(bool);
  BLINK_PLATFORM_EXPORT static void EnablePresentationAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnablePushMessaging(bool);
  BLINK_PLATFORM_EXPORT static void EnablePushSubscriptionChangeEvent(bool);
  BLINK_PLATFORM_EXPORT static void EnableDirectSockets(bool);
  BLINK_PLATFORM_EXPORT static void EnableRemotePlaybackAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnableSecurePaymentConfirmation(bool);
  BLINK_PLATFORM_EXPORT static void EnableSecurePaymentConfirmationDebug(bool);
  BLINK_PLATFORM_EXPORT static void EnableScriptedSpeechRecognition(bool);
  BLINK_PLATFORM_EXPORT static void EnableScriptedSpeechSynthesis(bool);
  BLINK_PLATFORM_EXPORT static void EnableAutoLazyLoadOnReloads(bool);
  BLINK_PLATFORM_EXPORT static void EnableSharedArrayBuffer(bool);
  BLINK_PLATFORM_EXPORT static void EnableSharedAutofill(bool);
  BLINK_PLATFORM_EXPORT static void EnableSharedStorageAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnableSharedWorker(bool);
  BLINK_PLATFORM_EXPORT static void EnableTextFragmentAnchor(bool);
  BLINK_PLATFORM_EXPORT static void EnableCSSSelectorFragmentAnchor(bool);
  BLINK_PLATFORM_EXPORT static void EnableTouchDragAndContextMenu(bool enable);
  BLINK_PLATFORM_EXPORT static void EnableTouchEventFeatureDetection(bool);
  BLINK_PLATFORM_EXPORT static void EnableScrollUnification(bool);
  BLINK_PLATFORM_EXPORT static void EnableUserActivationSameOriginVisibility(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableV8IdleTasks(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebAppManifestId(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebAuth(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebBluetooth(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebBluetoothGetDevices(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebBluetoothWatchAdvertisements(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebGLDeveloperExtensions(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebGLDraftExtensions(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebGLImageChromium(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebGPU(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebID(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebNfc(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebOTPAssertionFeaturePolicy(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebShare(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebUsb(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXR(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRAnchors(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRARModule(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRCameraAccess(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRDepth(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRHandInput(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRHitTest(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRImageTracking(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRLightEstimation(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRPlaneDetection(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRViewportScale(bool);
  BLINK_PLATFORM_EXPORT static void ForceOverlayFullscreenVideo(bool);
  BLINK_PLATFORM_EXPORT static void EnableTimerThrottlingForBackgroundTabs(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableExpensiveBackgroundTimerThrottling(
      bool);
  BLINK_PLATFORM_EXPORT static void
  EnableSendBeaconThrowForBlobWithNonSimpleType(bool);
  BLINK_PLATFORM_EXPORT static void EnableSharedArrayBufferOnDesktop(bool);
  BLINK_PLATFORM_EXPORT static void
  EnableSharedArrayBufferUnrestrictedAccessAllowed(bool);
  BLINK_PLATFORM_EXPORT static void EnableDisplayCapturePermissionsPolicy(bool);
  BLINK_PLATFORM_EXPORT static void EnableVideoPlaybackQuality(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaControlsOverlayPlayButton(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaCastOverlayButton(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyInitializeMediaControls(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaEngagementBypassAutoplayPolicies(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableAutomationControlled(bool);
  BLINK_PLATFORM_EXPORT static void EnableAutoplayIgnoresWebAudio(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaControlsExpandGesture(bool);
  BLINK_PLATFORM_EXPORT static void EnableGetDisplayMedia(bool);
  BLINK_PLATFORM_EXPORT static void EnableAllowSyncXHRInPageDismissal(bool);
  BLINK_PLATFORM_EXPORT static void EnableSignedExchangeSubresourcePrefetch(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableSubresourceWebBundles(bool);
  BLINK_PLATFORM_EXPORT static void EnableIdleDetection(bool);
  BLINK_PLATFORM_EXPORT static void EnableSkipTouchEventFilter(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebOTP(bool);
  BLINK_PLATFORM_EXPORT static void EnableConsolidatedMovementXY(bool);
  BLINK_PLATFORM_EXPORT static void EnableMouseSubframeNoImplicitCapture(bool);
  BLINK_PLATFORM_EXPORT static void EnableBackForwardCache(bool);

  BLINK_PLATFORM_EXPORT static void EnableTrustTokens(bool);
  BLINK_PLATFORM_EXPORT static void EnableTrustTokensAlwaysAllowIssuance(bool);

  BLINK_PLATFORM_EXPORT static void EnableInstalledApp(bool);
  BLINK_PLATFORM_EXPORT static void EnableBackfaceVisibilityInterop(bool);
  BLINK_PLATFORM_EXPORT static void EnableVideoWakeLockOptimisationHiddenMuted(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableContentIndex(bool);
  BLINK_PLATFORM_EXPORT static void EnableRestrictGamepadAccess(bool);

  BLINK_PLATFORM_EXPORT static void EnableTargetBlankImpliesNoOpener(bool);

  BLINK_PLATFORM_EXPORT static void EnableParseUrlProtocolHandler(bool);

  BLINK_PLATFORM_EXPORT static void EnableDocumentTransition(bool);
  BLINK_PLATFORM_EXPORT static void EnableCLSScrollAnchoring(bool);

 private:
  WebRuntimeFeatures();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RUNTIME_FEATURES_H_
