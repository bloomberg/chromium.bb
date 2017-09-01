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

#ifndef WebRuntimeFeatures_h
#define WebRuntimeFeatures_h

#include "WebCommon.h"
#include "WebString.h"

#include <string>

namespace blink {
// This class is used to enable runtime features of Blink.
// Stable features are enabled by default.
class WebRuntimeFeatures {
 public:
  // Enable or disable features with status=experimental listed in
  // Source/platform/RuntimeEnabledFeatures.in.
  BLINK_PLATFORM_EXPORT static void EnableExperimentalFeatures(bool);

  // Enable or disable features with status=test listed in
  // Source/platform/RuntimeEnabledFeatures.in.
  BLINK_PLATFORM_EXPORT static void EnableTestOnlyFeatures(bool);

  // Enable or disable features with non-empty origin_trial_feature_name in
  // Source/platform/RuntimeEnabledFeatures.in.
  BLINK_PLATFORM_EXPORT static void EnableOriginTrialControlledFeatures(bool);

  // Enables or disables a feature by its string identifier from
  // Source/platform/RuntimeEnabledFeatures.in.
  // Note: We use std::string instead of WebString because this API can
  // be called before blink::Initalize(). We can't create WebString objects
  // before blink::Initialize().
  BLINK_PLATFORM_EXPORT static void EnableFeatureFromString(
      const std::string& name,
      bool enable);

  BLINK_PLATFORM_EXPORT static void EnableCompositedSelectionUpdate(bool);
  BLINK_PLATFORM_EXPORT static bool IsCompositedSelectionUpdateEnabled();

  BLINK_PLATFORM_EXPORT static void EnableCompositorTouchAction(bool);

  BLINK_PLATFORM_EXPORT static void EnableDisplayList2dCanvas(bool);
  BLINK_PLATFORM_EXPORT static void ForceDisplayList2dCanvas(bool);

  BLINK_PLATFORM_EXPORT static void EnableOriginTrials(bool);
  BLINK_PLATFORM_EXPORT static bool IsOriginTrialsEnabled();

  BLINK_PLATFORM_EXPORT static void EnableAccelerated2dCanvas(bool);
  BLINK_PLATFORM_EXPORT static void EnableAudioOutputDevices(bool);
  BLINK_PLATFORM_EXPORT static void EnableCanvas2dImageChromium(bool);
  BLINK_PLATFORM_EXPORT static void EnableColorCorrectRendering(bool);
  BLINK_PLATFORM_EXPORT static void EnableCSSHexAlphaColor(bool);
  BLINK_PLATFORM_EXPORT static void EnableScrollTopLeftInterop(bool);
  BLINK_PLATFORM_EXPORT static void EnableDatabase(bool);
  BLINK_PLATFORM_EXPORT static void EnableDecodeToYUV(bool);
  BLINK_PLATFORM_EXPORT static void EnableExperimentalCanvasFeatures(bool);
  BLINK_PLATFORM_EXPORT static void EnableFastMobileScrolling(bool);
  BLINK_PLATFORM_EXPORT static void EnableFeaturePolicy(bool);
  BLINK_PLATFORM_EXPORT static void EnableFileSystem(bool);
  BLINK_PLATFORM_EXPORT static void EnableForceTallerSelectPopup(bool);
  BLINK_PLATFORM_EXPORT static void EnableGamepadExtensions(bool);
  BLINK_PLATFORM_EXPORT static void EnableGenericSensor(bool);
  BLINK_PLATFORM_EXPORT static void EnableGenericSensorExtraClasses(bool);
  BLINK_PLATFORM_EXPORT static void EnableHeapCompaction(bool);
  BLINK_PLATFORM_EXPORT static void EnableInputMultipleFieldsUI(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyParseCSS(bool);
  BLINK_PLATFORM_EXPORT static void EnableLoadingWithMojo(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaCapture(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaSession(bool);
  BLINK_PLATFORM_EXPORT static void EnableMiddleClickAutoscroll(bool);
  BLINK_PLATFORM_EXPORT static void EnableModuleScripts(bool);
  BLINK_PLATFORM_EXPORT static void EnableMojoBlobs(bool);
  BLINK_PLATFORM_EXPORT static void EnableNavigatorContentUtils(bool);
  BLINK_PLATFORM_EXPORT static void EnableNetInfoDownlinkMax(bool);
  BLINK_PLATFORM_EXPORT static void EnableNetworkService(bool);
  BLINK_PLATFORM_EXPORT static void EnableNotificationConstructor(bool);
  BLINK_PLATFORM_EXPORT static void EnableNotificationContentImage(bool);
  BLINK_PLATFORM_EXPORT static void EnableNotifications(bool);
  BLINK_PLATFORM_EXPORT static void EnableOffMainThreadFetch(bool);
  BLINK_PLATFORM_EXPORT static void EnableOnDeviceChange(bool);
  BLINK_PLATFORM_EXPORT static void EnableOrientationEvent(bool);
  BLINK_PLATFORM_EXPORT static void EnableOverlayScrollbars(bool);
  BLINK_PLATFORM_EXPORT static void EnableOutOfBlinkCORS(bool);
  BLINK_PLATFORM_EXPORT static void EnablePagePopup(bool);
  BLINK_PLATFORM_EXPORT static void EnablePassiveDocumentEventListeners(bool);
  BLINK_PLATFORM_EXPORT static void EnablePaymentRequest(bool);
  BLINK_PLATFORM_EXPORT static void EnablePermissionsAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnablePreciseMemoryInfo(bool);
  BLINK_PLATFORM_EXPORT static void EnablePreloadDefaultIsMetadata(bool);
  BLINK_PLATFORM_EXPORT static void EnablePrintBrowser(bool);
  BLINK_PLATFORM_EXPORT static void EnablePresentationAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnablePushMessaging(bool);
  BLINK_PLATFORM_EXPORT static void EnableReducedReferrerGranularity(bool);
  BLINK_PLATFORM_EXPORT static void EnableRemotePlaybackAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnableRenderingPipelineThrottling(bool);
  BLINK_PLATFORM_EXPORT static void EnableResourceLoadScheduler(bool);
  BLINK_PLATFORM_EXPORT static void EnableRootLayerScrolling(bool);
  BLINK_PLATFORM_EXPORT static void EnableScriptedSpeech(bool);
  BLINK_PLATFORM_EXPORT static void EnableScrollAnchoring(bool);
  BLINK_PLATFORM_EXPORT static void EnableServiceWorkerNavigationPreload(bool);
  BLINK_PLATFORM_EXPORT static void EnableServiceWorkerScriptStreaming(bool);
  BLINK_PLATFORM_EXPORT static void EnableSharedArrayBuffer(bool);
  BLINK_PLATFORM_EXPORT static void EnableSharedWorker(bool);
  BLINK_PLATFORM_EXPORT static void EnableSkipCompositingSmallScrollers(bool);
  BLINK_PLATFORM_EXPORT static void EnableSlimmingPaintV2(bool);
  BLINK_PLATFORM_EXPORT static void EnableTouchEventFeatureDetection(bool);
  BLINK_PLATFORM_EXPORT static void EnableTouchpadAndWheelScrollLatching(bool);
  BLINK_PLATFORM_EXPORT static void
  EnableTurnOff2DAndOpacityCompositorAnimations(bool);
  BLINK_PLATFORM_EXPORT static void EnableV8IdleTasks(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebAssemblyStreaming(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebAuth(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebBluetooth(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebFontsInterventionV2With2G(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebFontsInterventionV2With3G(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebFontsInterventionV2WithSlow2G(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableWebFontsInterventionTrigger(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebGLDraftExtensions(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebGLImageChromium(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebNfc(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebShare(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebUsb(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebVR(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebVRExperimentalRendering(bool);
  BLINK_PLATFORM_EXPORT static void EnableXSLT(bool);
  BLINK_PLATFORM_EXPORT static void ForceOverlayFullscreenVideo(bool);
  BLINK_PLATFORM_EXPORT static void EnableAutoplayMutedVideos(bool);
  BLINK_PLATFORM_EXPORT static void EnableTimerThrottlingForBackgroundTabs(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableTimerThrottlingForHiddenFrames(bool);
  BLINK_PLATFORM_EXPORT static void EnableExpensiveBackgroundTimerThrottling(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableCanvas2dDynamicRenderingModeSwitching(
      bool);
  BLINK_PLATFORM_EXPORT static void
  EnableSendBeaconThrowForBlobWithNonSimpleType(bool);
  BLINK_PLATFORM_EXPORT static void EnableBackgroundVideoTrackOptimization(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableNewRemotePlaybackPipeline(bool);
  BLINK_PLATFORM_EXPORT static void EnableVideoFullscreenOrientationLock(bool);
  BLINK_PLATFORM_EXPORT static void EnableVideoRotateToFullscreen(bool);
  BLINK_PLATFORM_EXPORT static void EnableVideoFullscreenDetection(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaControlsOverlayPlayButton(bool);
  BLINK_PLATFORM_EXPORT static void EnableRemotePlaybackBackend(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaCastOverlayButton(bool);
  BLINK_PLATFORM_EXPORT static void EnableClientPlaceholdersForServerLoFi(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyInitializeMediaControls(bool);
  BLINK_PLATFORM_EXPORT static void EnableClientHintsPersistent(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaEngagementBypassAutoplayPolicies(
      bool);

 private:
  WebRuntimeFeatures();
};

}  // namespace blink

#endif
