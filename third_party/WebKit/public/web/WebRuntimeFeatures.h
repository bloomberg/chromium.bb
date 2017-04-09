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

#include "../platform/WebCommon.h"
#include "../platform/WebString.h"

#include <string>

namespace blink {

// This class is used to enable runtime features of Blink.
// Stable features are enabled by default.
class WebRuntimeFeatures {
 public:
  // Enable features with status=experimental listed in
  // Source/platform/RuntimeEnabledFeatures.in.
  BLINK_EXPORT static void EnableExperimentalFeatures(bool);

  // Enable features with status=test listed in
  // Source/platform/RuntimeEnabledFeatures.in.
  BLINK_EXPORT static void EnableTestOnlyFeatures(bool);

  // Enables a feature by its string identifier from
  // Source/platform/RuntimeEnabledFeatures.in.
  // Note: We use std::string instead of WebString because this API can
  // be called before blink::initalize(). We can't create WebString objects
  // before blink::initialize().
  BLINK_EXPORT static void EnableFeatureFromString(const std::string& name,
                                                   bool enable);

  BLINK_EXPORT static void EnableCompositedSelectionUpdate(bool);
  BLINK_EXPORT static bool IsCompositedSelectionUpdateEnabled();

  BLINK_EXPORT static void EnableDisplayList2dCanvas(bool);
  BLINK_EXPORT static void ForceDisplayList2dCanvas(bool);
  BLINK_EXPORT static void ForceDisable2dCanvasCopyOnWrite(bool);

  BLINK_EXPORT static void EnableOriginTrials(bool);
  BLINK_EXPORT static bool IsOriginTrialsEnabled();

  BLINK_EXPORT static void EnableAccelerated2dCanvas(bool);
  BLINK_EXPORT static void EnableAccessibilityObjectModel(bool);
  BLINK_EXPORT static void EnableAudioOutputDevices(bool);
  BLINK_EXPORT static void EnableCanvas2dImageChromium(bool);
  BLINK_EXPORT static void EnableColorCorrectRendering(bool);
  BLINK_EXPORT static void EnableColorCorrectRenderingDefaultMode(bool);
  BLINK_EXPORT static void EnableCredentialManagerAPI(bool);
  BLINK_EXPORT static void EnableDatabase(bool);
  BLINK_EXPORT static void EnableDecodeToYUV(bool);
  BLINK_EXPORT static void EnableDocumentWriteEvaluator(bool);
  BLINK_EXPORT static void EnableExperimentalCanvasFeatures(bool);
  BLINK_EXPORT static void EnableFastMobileScrolling(bool);
  BLINK_EXPORT static void EnableFeaturePolicy(bool);
  BLINK_EXPORT static void EnableFileSystem(bool);
  BLINK_EXPORT static void EnableGamepadExtensions(bool);
  BLINK_EXPORT static void EnableGenericSensor(bool);
  BLINK_EXPORT static void EnableHeapCompaction(bool);
  BLINK_EXPORT static void EnableInputMultipleFieldsUI(bool);
  BLINK_EXPORT static void EnableLazyParseCSS(bool);
  BLINK_EXPORT static void EnableLoadingWithMojo(bool);
  BLINK_EXPORT static void EnableMediaCapture(bool);
  BLINK_EXPORT static void EnableMediaDocumentDownloadButton(bool);
  BLINK_EXPORT static void EnableMediaSession(bool);
  BLINK_EXPORT static void EnableMiddleClickAutoscroll(bool);
  BLINK_EXPORT static void EnableNavigatorContentUtils(bool);
  BLINK_EXPORT static void EnableNetworkInformation(bool);
  BLINK_EXPORT static void EnableNotificationConstructor(bool);
  BLINK_EXPORT static void EnableNotificationContentImage(bool);
  BLINK_EXPORT static void EnableNotifications(bool);
  BLINK_EXPORT static void EnableOnDeviceChange(bool);
  BLINK_EXPORT static void EnableOrientationEvent(bool);
  BLINK_EXPORT static void EnableOverlayScrollbars(bool);
  BLINK_EXPORT static void EnablePagePopup(bool);
  BLINK_EXPORT static void EnablePassiveDocumentEventListeners(bool);
  BLINK_EXPORT static void EnablePaymentRequest(bool);
  BLINK_EXPORT static void EnablePermissionsAPI(bool);
  BLINK_EXPORT static void EnablePointerEvent(bool);
  BLINK_EXPORT static void EnablePreciseMemoryInfo(bool);
  BLINK_EXPORT static void EnablePrintBrowser(bool);
  BLINK_EXPORT static void EnablePresentationAPI(bool);
  BLINK_EXPORT static void EnablePushMessaging(bool);
  BLINK_EXPORT static void EnableReducedReferrerGranularity(bool);
  BLINK_EXPORT static void EnableRenderingPipelineThrottling(bool);
  BLINK_EXPORT static void EnableRemotePlaybackAPI(bool);
  BLINK_EXPORT static void EnableRootLayerScrolling(bool);
  BLINK_EXPORT static void EnableScriptedSpeech(bool);
  BLINK_EXPORT static void EnableScrollAnchoring(bool);
  BLINK_EXPORT static void EnableServiceWorkerNavigationPreload(bool);
  BLINK_EXPORT static void EnableSharedArrayBuffer(bool);
  BLINK_EXPORT static void EnableSharedWorker(bool);
  BLINK_EXPORT static void EnableSlimmingPaintV2(bool);
  BLINK_EXPORT static void EnableSlimmingPaintInvalidation(bool);
  BLINK_EXPORT static void EnableTouchEventFeatureDetection(bool);
  BLINK_EXPORT static void EnableTouchpadAndWheelScrollLatching(bool);
  BLINK_EXPORT static void EnableV8IdleTasks(bool);
  BLINK_EXPORT static void EnableWebAssemblySerialization(bool);
  BLINK_EXPORT static void EnableWebBluetooth(bool);
  BLINK_EXPORT static void EnableWebFontsInterventionV2With2G(bool);
  BLINK_EXPORT static void EnableWebFontsInterventionV2With3G(bool);
  BLINK_EXPORT static void EnableWebFontsInterventionV2WithSlow2G(bool);
  BLINK_EXPORT static void EnableWebFontsInterventionTrigger(bool);
  BLINK_EXPORT static void EnableWebGLDraftExtensions(bool);
  BLINK_EXPORT static void EnableWebGLImageChromium(bool);
  BLINK_EXPORT static void EnableWebUsb(bool);
  BLINK_EXPORT static void EnableWebVR(bool);
  BLINK_EXPORT static void EnableWebVRExperimentalRendering(bool);
  BLINK_EXPORT static void EnableXSLT(bool);
  BLINK_EXPORT static void ForceOverlayFullscreenVideo(bool);
  BLINK_EXPORT static void EnableAutoplayMutedVideos(bool);
  BLINK_EXPORT static void EnableTimerThrottlingForBackgroundTabs(bool);
  BLINK_EXPORT static void EnableTimerThrottlingForHiddenFrames(bool);
  BLINK_EXPORT static void EnableExpensiveBackgroundTimerThrottling(bool);
  BLINK_EXPORT static void EnableCanvas2dDynamicRenderingModeSwitching(bool);
  BLINK_EXPORT static void EnableSendBeaconThrowForBlobWithNonSimpleType(bool);
  BLINK_EXPORT static void EnableBackgroundVideoTrackOptimization(bool);
  BLINK_EXPORT static void EnableVideoFullscreenOrientationLock(bool);
  BLINK_EXPORT static void EnableVideoFullscreenDetection(bool);
  BLINK_EXPORT static void EnableMediaControlsOverlayPlayButton(bool);

 private:
  WebRuntimeFeatures();
};

}  // namespace blink

#endif
