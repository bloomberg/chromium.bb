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

#include "public/web/WebRuntimeFeatures.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/Assertions.h"

namespace blink {

void WebRuntimeFeatures::EnableExperimentalFeatures(bool enable) {
  RuntimeEnabledFeatures::setExperimentalFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableWebBluetooth(bool enable) {
  RuntimeEnabledFeatures::setWebBluetoothEnabled(enable);
}

void WebRuntimeFeatures::EnableWebAssemblySerialization(bool enable) {
  RuntimeEnabledFeatures::setWebAssemblySerializationEnabled(enable);
}

void WebRuntimeFeatures::EnableWebUsb(bool enable) {
  RuntimeEnabledFeatures::setWebUSBEnabled(enable);
}

void WebRuntimeFeatures::EnableFeatureFromString(const std::string& name,
                                                 bool enable) {
  RuntimeEnabledFeatures::setFeatureEnabledFromString(name, enable);
}

void WebRuntimeFeatures::EnableTestOnlyFeatures(bool enable) {
  RuntimeEnabledFeatures::setTestFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableAccelerated2dCanvas(bool enable) {
  RuntimeEnabledFeatures::setAccelerated2dCanvasEnabled(enable);
}

void WebRuntimeFeatures::EnableAccessibilityObjectModel(bool enable) {
  RuntimeEnabledFeatures::setAccessibilityObjectModelEnabled(enable);
}

void WebRuntimeFeatures::EnableAudioOutputDevices(bool enable) {
  RuntimeEnabledFeatures::setAudioOutputDevicesEnabled(enable);
}

void WebRuntimeFeatures::EnableCanvas2dImageChromium(bool enable) {
  RuntimeEnabledFeatures::setCanvas2dImageChromiumEnabled(enable);
}

void WebRuntimeFeatures::EnableColorCorrectRendering(bool enable) {
  RuntimeEnabledFeatures::setColorCorrectRenderingEnabled(enable);
}

void WebRuntimeFeatures::EnableColorCorrectRenderingDefaultMode(bool enable) {
  RuntimeEnabledFeatures::setColorCorrectRenderingDefaultModeEnabled(enable);
}

void WebRuntimeFeatures::EnableCompositedSelectionUpdate(bool enable) {
  RuntimeEnabledFeatures::setCompositedSelectionUpdateEnabled(enable);
}

bool WebRuntimeFeatures::IsCompositedSelectionUpdateEnabled() {
  return RuntimeEnabledFeatures::compositedSelectionUpdateEnabled();
}

void WebRuntimeFeatures::EnableDatabase(bool enable) {
  RuntimeEnabledFeatures::setDatabaseEnabled(enable);
}

void WebRuntimeFeatures::EnableDecodeToYUV(bool enable) {
  RuntimeEnabledFeatures::setDecodeToYUVEnabled(enable);
}

void WebRuntimeFeatures::ForceDisplayList2dCanvas(bool enable) {
  RuntimeEnabledFeatures::setForceDisplayList2dCanvasEnabled(enable);
}

void WebRuntimeFeatures::ForceDisable2dCanvasCopyOnWrite(bool enable) {
  RuntimeEnabledFeatures::setForceDisable2dCanvasCopyOnWriteEnabled(enable);
}

void WebRuntimeFeatures::EnableDisplayList2dCanvas(bool enable) {
  RuntimeEnabledFeatures::setDisplayList2dCanvasEnabled(enable);
}

void WebRuntimeFeatures::EnableCanvas2dDynamicRenderingModeSwitching(
    bool enable) {
  RuntimeEnabledFeatures::setEnableCanvas2dDynamicRenderingModeSwitchingEnabled(
      enable);
}

void WebRuntimeFeatures::EnableDocumentWriteEvaluator(bool enable) {
  RuntimeEnabledFeatures::setDocumentWriteEvaluatorEnabled(enable);
}

void WebRuntimeFeatures::EnableExperimentalCanvasFeatures(bool enable) {
  RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableFastMobileScrolling(bool enable) {
  RuntimeEnabledFeatures::setFastMobileScrollingEnabled(enable);
}

void WebRuntimeFeatures::EnableFeaturePolicy(bool enable) {
  RuntimeEnabledFeatures::setFeaturePolicyEnabled(enable);
}

void WebRuntimeFeatures::EnableFileSystem(bool enable) {
  RuntimeEnabledFeatures::setFileSystemEnabled(enable);
}

void WebRuntimeFeatures::EnableGamepadExtensions(bool enable) {
  RuntimeEnabledFeatures::setGamepadExtensionsEnabled(enable);
}

void WebRuntimeFeatures::EnableGenericSensor(bool enable) {
  RuntimeEnabledFeatures::setSensorEnabled(enable);
}

void WebRuntimeFeatures::EnableHeapCompaction(bool enable) {
  RuntimeEnabledFeatures::setHeapCompactionEnabled(enable);
}

void WebRuntimeFeatures::EnableInputMultipleFieldsUI(bool enable) {
  RuntimeEnabledFeatures::setInputMultipleFieldsUIEnabled(enable);
}

void WebRuntimeFeatures::EnableLazyParseCSS(bool enable) {
  RuntimeEnabledFeatures::setLazyParseCSSEnabled(enable);
}

void WebRuntimeFeatures::EnableLoadingWithMojo(bool enable) {
  RuntimeEnabledFeatures::setLoadingWithMojoEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaCapture(bool enable) {
  RuntimeEnabledFeatures::setMediaCaptureEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaDocumentDownloadButton(bool enable) {
  RuntimeEnabledFeatures::setMediaDocumentDownloadButtonEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaSession(bool enable) {
  RuntimeEnabledFeatures::setMediaSessionEnabled(enable);
}

void WebRuntimeFeatures::EnableNotificationConstructor(bool enable) {
  RuntimeEnabledFeatures::setNotificationConstructorEnabled(enable);
}

void WebRuntimeFeatures::EnableNotificationContentImage(bool enable) {
  RuntimeEnabledFeatures::setNotificationContentImageEnabled(enable);
}

void WebRuntimeFeatures::EnableNotifications(bool enable) {
  RuntimeEnabledFeatures::setNotificationsEnabled(enable);
}

void WebRuntimeFeatures::EnableNavigatorContentUtils(bool enable) {
  RuntimeEnabledFeatures::setNavigatorContentUtilsEnabled(enable);
}

void WebRuntimeFeatures::EnableNetworkInformation(bool enable) {
  RuntimeEnabledFeatures::setNetworkInformationEnabled(enable);
}

void WebRuntimeFeatures::EnableOnDeviceChange(bool enable) {
  RuntimeEnabledFeatures::setOnDeviceChangeEnabled(enable);
}

void WebRuntimeFeatures::EnableOrientationEvent(bool enable) {
  RuntimeEnabledFeatures::setOrientationEventEnabled(enable);
}

void WebRuntimeFeatures::EnableOriginTrials(bool enable) {
  RuntimeEnabledFeatures::setOriginTrialsEnabled(enable);
}

bool WebRuntimeFeatures::IsOriginTrialsEnabled() {
  return RuntimeEnabledFeatures::originTrialsEnabled();
}

void WebRuntimeFeatures::EnablePagePopup(bool enable) {
  RuntimeEnabledFeatures::setPagePopupEnabled(enable);
}

void WebRuntimeFeatures::EnableMiddleClickAutoscroll(bool enable) {
  RuntimeEnabledFeatures::setMiddleClickAutoscrollEnabled(enable);
}

void WebRuntimeFeatures::EnablePassiveDocumentEventListeners(bool enable) {
  RuntimeEnabledFeatures::setPassiveDocumentEventListenersEnabled(enable);
}

void WebRuntimeFeatures::EnablePaymentRequest(bool enable) {
  RuntimeEnabledFeatures::setPaymentRequestEnabled(enable);
}

void WebRuntimeFeatures::EnablePermissionsAPI(bool enable) {
  RuntimeEnabledFeatures::setPermissionsEnabled(enable);
}

void WebRuntimeFeatures::EnablePointerEvent(bool enable) {
  RuntimeEnabledFeatures::setPointerEventEnabled(enable);
}

void WebRuntimeFeatures::EnableScriptedSpeech(bool enable) {
  RuntimeEnabledFeatures::setScriptedSpeechEnabled(enable);
}

void WebRuntimeFeatures::EnableSlimmingPaintV2(bool enable) {
  RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(enable);
}

void WebRuntimeFeatures::EnableSlimmingPaintInvalidation(bool enable) {
  RuntimeEnabledFeatures::setSlimmingPaintInvalidationEnabled(enable);
}

void WebRuntimeFeatures::EnableTouchEventFeatureDetection(bool enable) {
  RuntimeEnabledFeatures::setTouchEventFeatureDetectionEnabled(enable);
}

void WebRuntimeFeatures::EnableTouchpadAndWheelScrollLatching(bool enable) {
  RuntimeEnabledFeatures::setTouchpadAndWheelScrollLatchingEnabled(enable);
}

void WebRuntimeFeatures::EnableWebGLDraftExtensions(bool enable) {
  RuntimeEnabledFeatures::setWebGLDraftExtensionsEnabled(enable);
}

void WebRuntimeFeatures::EnableWebGLImageChromium(bool enable) {
  RuntimeEnabledFeatures::setWebGLImageChromiumEnabled(enable);
}

void WebRuntimeFeatures::EnableXSLT(bool enable) {
  RuntimeEnabledFeatures::setXSLTEnabled(enable);
}

void WebRuntimeFeatures::EnableOverlayScrollbars(bool enable) {
  RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(enable);
}

void WebRuntimeFeatures::ForceOverlayFullscreenVideo(bool enable) {
  RuntimeEnabledFeatures::setForceOverlayFullscreenVideoEnabled(enable);
}

void WebRuntimeFeatures::EnableSharedArrayBuffer(bool enable) {
  RuntimeEnabledFeatures::setSharedArrayBufferEnabled(enable);
}

void WebRuntimeFeatures::EnableSharedWorker(bool enable) {
  RuntimeEnabledFeatures::setSharedWorkerEnabled(enable);
}

void WebRuntimeFeatures::EnablePreciseMemoryInfo(bool enable) {
  RuntimeEnabledFeatures::setPreciseMemoryInfoEnabled(enable);
}

void WebRuntimeFeatures::EnablePrintBrowser(bool enable) {
  RuntimeEnabledFeatures::setPrintBrowserEnabled(enable);
}

void WebRuntimeFeatures::EnableCredentialManagerAPI(bool enable) {
  RuntimeEnabledFeatures::setCredentialManagerEnabled(enable);
}

void WebRuntimeFeatures::EnableV8IdleTasks(bool enable) {
  RuntimeEnabledFeatures::setV8IdleTasksEnabled(enable);
}

void WebRuntimeFeatures::EnableReducedReferrerGranularity(bool enable) {
  RuntimeEnabledFeatures::setReducedReferrerGranularityEnabled(enable);
}

void WebRuntimeFeatures::EnablePushMessaging(bool enable) {
  RuntimeEnabledFeatures::setPushMessagingEnabled(enable);
}

void WebRuntimeFeatures::EnableWebVR(bool enable) {
  RuntimeEnabledFeatures::setWebVREnabled(enable);
}

void WebRuntimeFeatures::EnableWebVRExperimentalRendering(bool enable) {
  RuntimeEnabledFeatures::setWebVRExperimentalRenderingEnabled(enable);
}

void WebRuntimeFeatures::EnablePresentationAPI(bool enable) {
  RuntimeEnabledFeatures::setPresentationEnabled(enable);
}

void WebRuntimeFeatures::EnableWebFontsInterventionV2With2G(bool enable) {
  RuntimeEnabledFeatures::setWebFontsInterventionV2With2GEnabled(enable);
}

void WebRuntimeFeatures::EnableWebFontsInterventionV2With3G(bool enable) {
  RuntimeEnabledFeatures::setWebFontsInterventionV2With3GEnabled(enable);
}

void WebRuntimeFeatures::EnableWebFontsInterventionV2WithSlow2G(bool enable) {
  RuntimeEnabledFeatures::setWebFontsInterventionV2WithSlow2GEnabled(enable);
}

void WebRuntimeFeatures::EnableWebFontsInterventionTrigger(bool enable) {
  RuntimeEnabledFeatures::setWebFontsInterventionTriggerEnabled(enable);
}

void WebRuntimeFeatures::EnableRenderingPipelineThrottling(bool enable) {
  RuntimeEnabledFeatures::setRenderingPipelineThrottlingEnabled(enable);
}

void WebRuntimeFeatures::EnableExpensiveBackgroundTimerThrottling(bool enable) {
  RuntimeEnabledFeatures::setExpensiveBackgroundTimerThrottlingEnabled(enable);
}

void WebRuntimeFeatures::EnableRootLayerScrolling(bool enable) {
  RuntimeEnabledFeatures::setRootLayerScrollingEnabled(enable);
}

void WebRuntimeFeatures::EnableScrollAnchoring(bool enable) {
  RuntimeEnabledFeatures::setScrollAnchoringEnabled(enable);
}

void WebRuntimeFeatures::EnableServiceWorkerNavigationPreload(bool enable) {
  RuntimeEnabledFeatures::setServiceWorkerNavigationPreloadEnabled(enable);
}

void WebRuntimeFeatures::EnableAutoplayMutedVideos(bool enable) {
  RuntimeEnabledFeatures::setAutoplayMutedVideosEnabled(enable);
}

void WebRuntimeFeatures::EnableTimerThrottlingForBackgroundTabs(bool enable) {
  RuntimeEnabledFeatures::setTimerThrottlingForBackgroundTabsEnabled(enable);
}

void WebRuntimeFeatures::EnableTimerThrottlingForHiddenFrames(bool enable) {
  RuntimeEnabledFeatures::setTimerThrottlingForHiddenFramesEnabled(enable);
}

void WebRuntimeFeatures::EnableSendBeaconThrowForBlobWithNonSimpleType(
    bool enable) {
  RuntimeEnabledFeatures::setSendBeaconThrowForBlobWithNonSimpleTypeEnabled(
      enable);
}

void WebRuntimeFeatures::EnableBackgroundVideoTrackOptimization(bool enable) {
  RuntimeEnabledFeatures::setBackgroundVideoTrackOptimizationEnabled(enable);
}

void WebRuntimeFeatures::EnableRemotePlaybackAPI(bool enable) {
  RuntimeEnabledFeatures::setRemotePlaybackEnabled(enable);
}

void WebRuntimeFeatures::EnableVideoFullscreenOrientationLock(bool enable) {
  RuntimeEnabledFeatures::setVideoFullscreenOrientationLockEnabled(enable);
}

void WebRuntimeFeatures::EnableVideoFullscreenDetection(bool enable) {
  RuntimeEnabledFeatures::setVideoFullscreenDetectionEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaControlsOverlayPlayButton(bool enable) {
  RuntimeEnabledFeatures::setMediaControlsOverlayPlayButtonEnabled(enable);
}

}  // namespace blink
