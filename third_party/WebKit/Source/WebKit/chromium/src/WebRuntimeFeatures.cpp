/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "WebRuntimeFeatures.h"

#include "WebMediaPlayerClientImpl.h"
#include "RuntimeEnabledFeatures.h"
#include "modules/websockets/WebSocket.h"

#include <wtf/UnusedParam.h>

using namespace WebCore;

namespace WebKit {

void WebRuntimeFeatures::enableStableFeatures(bool enable)
{
    enableApplicationCache(enable);
    enableDatabase(enable);
    enableDeviceOrientation(enable);
    enableDirectoryUpload(enable);
    enableEncryptedMedia(enable);
    enableFileSystem(enable);
    enableFullscreen(enable);
    enableGamepad(enable);
    enableGeolocation(enable);
    enableIndexedDB(enable);
    enableInputTypeWeek(enable);
    enableJavaScriptI18NAPI(enable);
    enableLocalStorage(enable);
    enableMediaPlayer(enable);
    enableMediaSource(enable);
    enableMediaStream(enable);
    enableNotifications(enable);
    enablePeerConnection(enable);
    enableQuota(enable);
    enableScriptedSpeech(enable);
    enableSessionStorage(enable);
    enableSpeechInput(enable);
    enableTouch(enable);
    enableVideoTrack(enable);
    enableWebAudio(enable);
}

void WebRuntimeFeatures::enableExperimentalFeatures(bool enable)
{
    enableCSSCompositing(enable);
    enableCSSExclusions(enable);
    enableCSSRegions(enable);
    enableCustomDOMElements(enable);
    enableDialogElement(enable);
    enableExperimentalContentSecurityPolicyFeatures(enable);
    enableFontLoadEvents(enable);
    enableSeamlessIFrames(enable);
    enableStyleScoped(enable);
}

void WebRuntimeFeatures::enableTestOnlyFeatures(bool enable)
{
    // This method should be used by ContentShell
    // to enable features which should be enabled for
    // the layout tests but are not yet "experimental".
    enableCanvasPath(enable);
    enableEncryptedMedia(enable);
    enableExperimentalCanvasFeatures(enable);
    enableExperimentalShadowDOM(enable);
    enableFontLoadEvents(enable);
    enableInputTypeDateTime(enable);
    enableRequestAutocomplete(enable);
    enableScriptedSpeech(enable);
    enableWebMIDI(enable);
}

void WebRuntimeFeatures::enableDatabase(bool enable)
{
    RuntimeEnabledFeatures::setDatabaseEnabled(enable);
}

bool WebRuntimeFeatures::isDatabaseEnabled()
{
    return RuntimeEnabledFeatures::databaseEnabled();
}

// FIXME: Remove the ability to enable this feature at runtime.
void WebRuntimeFeatures::enableLocalStorage(bool enable)
{
    RuntimeEnabledFeatures::setLocalStorageEnabled(enable);
}

// FIXME: Remove the ability to enable this feature at runtime.
bool WebRuntimeFeatures::isLocalStorageEnabled()
{
    return RuntimeEnabledFeatures::localStorageEnabled();
}

// FIXME: Remove the ability to enable this feature at runtime.
void WebRuntimeFeatures::enableSessionStorage(bool enable)
{
    RuntimeEnabledFeatures::setSessionStorageEnabled(enable);
}

// FIXME: Remove the ability to enable this feature at runtime.
bool WebRuntimeFeatures::isSessionStorageEnabled()
{
    return RuntimeEnabledFeatures::sessionStorageEnabled();
}

void WebRuntimeFeatures::enableMediaPlayer(bool enable)
{
    WebMediaPlayerClientImpl::setIsEnabled(enable);
}

bool WebRuntimeFeatures::isMediaPlayerEnabled()
{
    return WebMediaPlayerClientImpl::isEnabled();
}

void WebRuntimeFeatures::enableNotifications(bool enable)
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    RuntimeEnabledFeatures::setNotificationsEnabled(enable);
#endif
}

bool WebRuntimeFeatures::isNotificationsEnabled()
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    return RuntimeEnabledFeatures::notificationsEnabled();
#else
    return false;
#endif
}

void WebRuntimeFeatures::enableApplicationCache(bool enable)
{
    RuntimeEnabledFeatures::setApplicationCacheEnabled(enable);
}

bool WebRuntimeFeatures::isApplicationCacheEnabled()
{
    return RuntimeEnabledFeatures::applicationCacheEnabled();
}

void WebRuntimeFeatures::enableGeolocation(bool enable)
{
    RuntimeEnabledFeatures::setGeolocationEnabled(enable);
}

bool WebRuntimeFeatures::isGeolocationEnabled()
{
    return RuntimeEnabledFeatures::geolocationEnabled();
}

void WebRuntimeFeatures::enableIndexedDB(bool enable)
{
    RuntimeEnabledFeatures::setIndexedDBEnabled(enable);
}

bool WebRuntimeFeatures::isIndexedDBEnabled()
{
    return RuntimeEnabledFeatures::indexedDBEnabled();
}

void WebRuntimeFeatures::enableWebAudio(bool enable)
{
    RuntimeEnabledFeatures::setWebAudioEnabled(enable);
}

bool WebRuntimeFeatures::isWebAudioEnabled()
{
    return RuntimeEnabledFeatures::webAudioEnabled();
}

void WebRuntimeFeatures::enableTouch(bool enable)
{
    RuntimeEnabledFeatures::setTouchEnabled(enable);
}

bool WebRuntimeFeatures::isTouchEnabled()
{
    return RuntimeEnabledFeatures::touchEnabled();
}

void WebRuntimeFeatures::enableDeviceMotion(bool enable)
{
    RuntimeEnabledFeatures::setDeviceMotionEnabled(enable);
}

bool WebRuntimeFeatures::isDeviceMotionEnabled()
{
    return RuntimeEnabledFeatures::deviceMotionEnabled();
}

void WebRuntimeFeatures::enableDeviceOrientation(bool enable)
{
    RuntimeEnabledFeatures::setDeviceOrientationEnabled(enable);
}

bool WebRuntimeFeatures::isDeviceOrientationEnabled()
{
    return RuntimeEnabledFeatures::deviceOrientationEnabled();
}

void WebRuntimeFeatures::enableSpeechInput(bool enable)
{
    RuntimeEnabledFeatures::setSpeechInputEnabled(enable);
}

bool WebRuntimeFeatures::isSpeechInputEnabled()
{
    return RuntimeEnabledFeatures::speechInputEnabled();
}

void WebRuntimeFeatures::enableScriptedSpeech(bool enable)
{
    RuntimeEnabledFeatures::setScriptedSpeechEnabled(enable);
}

bool WebRuntimeFeatures::isScriptedSpeechEnabled()
{
    return RuntimeEnabledFeatures::scriptedSpeechEnabled();
}

void WebRuntimeFeatures::enableFileSystem(bool enable)
{
    RuntimeEnabledFeatures::setFileSystemEnabled(enable);
}

bool WebRuntimeFeatures::isFileSystemEnabled()
{
    return RuntimeEnabledFeatures::fileSystemEnabled();
}

void WebRuntimeFeatures::enableJavaScriptI18NAPI(bool enable)
{
    RuntimeEnabledFeatures::setJavaScriptI18NAPIEnabled(enable);
}

bool WebRuntimeFeatures::isJavaScriptI18NAPIEnabled()
{
    return RuntimeEnabledFeatures::javaScriptI18NAPIEnabled();
}

void WebRuntimeFeatures::enableQuota(bool enable)
{
    RuntimeEnabledFeatures::setQuotaEnabled(enable);
}

bool WebRuntimeFeatures::isQuotaEnabled()
{
    return RuntimeEnabledFeatures::quotaEnabled();
}

void WebRuntimeFeatures::enableMediaStream(bool enable)
{
    RuntimeEnabledFeatures::setMediaStreamEnabled(enable);
}

bool WebRuntimeFeatures::isMediaStreamEnabled()
{
    return RuntimeEnabledFeatures::mediaStreamEnabled();
}

void WebRuntimeFeatures::enablePeerConnection(bool enable)
{
    RuntimeEnabledFeatures::setPeerConnectionEnabled(enable);
}

bool WebRuntimeFeatures::isPeerConnectionEnabled()
{
    return RuntimeEnabledFeatures::peerConnectionEnabled();
}

void WebRuntimeFeatures::enableFullscreen(bool enable)
{
    RuntimeEnabledFeatures::setFullscreenEnabled(enable);
}

bool WebRuntimeFeatures::isFullscreenEnabled()
{
    return RuntimeEnabledFeatures::fullscreenEnabled();
}

void WebRuntimeFeatures::enableMediaSource(bool enable)
{
    RuntimeEnabledFeatures::setMediaSourceEnabled(enable);
}

bool WebRuntimeFeatures::isMediaSourceEnabled()
{
    return RuntimeEnabledFeatures::mediaSourceEnabled();
}

void WebRuntimeFeatures::enableEncryptedMedia(bool enable)
{
    RuntimeEnabledFeatures::setEncryptedMediaEnabled(enable);
}

bool WebRuntimeFeatures::isEncryptedMediaEnabled()
{
    return RuntimeEnabledFeatures::encryptedMediaEnabled();
}

void WebRuntimeFeatures::enableVideoTrack(bool enable)
{
    RuntimeEnabledFeatures::setVideoTrackEnabled(enable);
}

bool WebRuntimeFeatures::isVideoTrackEnabled()
{
    return RuntimeEnabledFeatures::videoTrackEnabled();
}

void WebRuntimeFeatures::enableGamepad(bool enable)
{
    RuntimeEnabledFeatures::setGamepadEnabled(enable);
}

bool WebRuntimeFeatures::isGamepadEnabled()
{
    return RuntimeEnabledFeatures::gamepadEnabled();
}

void WebRuntimeFeatures::enableExperimentalShadowDOM(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalShadowDOMEnabled(enable);
}

bool WebRuntimeFeatures::isExperimentalShadowDOMEnabled()
{
    return RuntimeEnabledFeatures::experimentalShadowDOMEnabled();
}

void WebRuntimeFeatures::enableCustomDOMElements(bool enable)
{
    RuntimeEnabledFeatures::setCustomDOMElementsEnabled(enable);
}

bool WebRuntimeFeatures::isCustomDOMElementsEnabled()
{
    return RuntimeEnabledFeatures::customDOMElementsEnabled();
}

void WebRuntimeFeatures::enableStyleScoped(bool enable)
{
    RuntimeEnabledFeatures::setStyleScopedEnabled(enable);
}

bool WebRuntimeFeatures::isStyleScopedEnabled()
{
    return RuntimeEnabledFeatures::styleScopedEnabled();
}

void WebRuntimeFeatures::enableInputTypeDateTime(bool enable)
{
    RuntimeEnabledFeatures::setInputTypeDateTimeEnabled(enable);
}

bool WebRuntimeFeatures::isInputTypeDateTimeEnabled()
{
    return RuntimeEnabledFeatures::inputTypeDateTimeEnabled();
}

void WebRuntimeFeatures::enableInputTypeWeek(bool enable)
{
    RuntimeEnabledFeatures::setInputTypeWeekEnabled(enable);
}

bool WebRuntimeFeatures::isInputTypeWeekEnabled()
{
    return RuntimeEnabledFeatures::inputTypeWeekEnabled();
}

void WebRuntimeFeatures::enableDialogElement(bool enable)
{
    RuntimeEnabledFeatures::setDialogElementEnabled(enable);
}

bool WebRuntimeFeatures::isDialogElementEnabled()
{
    return RuntimeEnabledFeatures::dialogElementEnabled();
}

void WebRuntimeFeatures::enableLazyLayout(bool enable)
{
    RuntimeEnabledFeatures::setLazyLayoutEnabled(enable);
}

bool WebRuntimeFeatures::isLazyLayoutEnabled()
{
    return RuntimeEnabledFeatures::lazyLayoutEnabled();
}

void WebRuntimeFeatures::enableExperimentalContentSecurityPolicyFeatures(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalContentSecurityPolicyFeaturesEnabled(enable);
}

bool WebRuntimeFeatures::isExperimentalContentSecurityPolicyFeaturesEnabled()
{
    return RuntimeEnabledFeatures::experimentalContentSecurityPolicyFeaturesEnabled();
}

void WebRuntimeFeatures::enableSeamlessIFrames(bool enable)
{
    return RuntimeEnabledFeatures::setSeamlessIFramesEnabled(enable);
}

bool WebRuntimeFeatures::isSeamlessIFramesEnabled()
{
    return RuntimeEnabledFeatures::seamlessIFramesEnabled();
}

void WebRuntimeFeatures::enableCanvasPath(bool enable)
{
    RuntimeEnabledFeatures::setCanvasPathEnabled(enable);
}

bool WebRuntimeFeatures::isCanvasPathEnabled()
{
    return RuntimeEnabledFeatures::canvasPathEnabled();
}

void WebRuntimeFeatures::enableCSSExclusions(bool enable)
{
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(enable);
}

bool WebRuntimeFeatures::isCSSExclusionsEnabled()
{
    return RuntimeEnabledFeatures::cssExclusionsEnabled();
}

void WebRuntimeFeatures::enableCSSRegions(bool enable)
{
    RuntimeEnabledFeatures::setCSSRegionsEnabled(enable);
}

bool WebRuntimeFeatures::isCSSRegionsEnabled()
{
    return RuntimeEnabledFeatures::cssRegionsEnabled();
}

void WebRuntimeFeatures::enableCSSCompositing(bool enable)
{
    RuntimeEnabledFeatures::setCSSCompositingEnabled(enable);
}

bool WebRuntimeFeatures::isCSSCompositingEnabled()
{
    return RuntimeEnabledFeatures::cssCompositingEnabled();
}

void WebRuntimeFeatures::enableFontLoadEvents(bool enable)
{
    RuntimeEnabledFeatures::setFontLoadEventsEnabled(enable);
}

bool WebRuntimeFeatures::isFontLoadEventsEnabled()
{
    return RuntimeEnabledFeatures::fontLoadEventsEnabled();
}

void WebRuntimeFeatures::enableRequestAutocomplete(bool enable)
{
    RuntimeEnabledFeatures::setRequestAutocompleteEnabled(enable);
}

bool WebRuntimeFeatures::isRequestAutocompleteEnabled()
{
    return RuntimeEnabledFeatures::requestAutocompleteEnabled();
}

void WebRuntimeFeatures::enableWebPInAcceptHeader(bool enable)
{
    RuntimeEnabledFeatures::setWebPInAcceptHeaderEnabled(enable);
}

bool WebRuntimeFeatures::isWebPInAcceptHeaderEnabled()
{
    return RuntimeEnabledFeatures::webPInAcceptHeaderEnabled();
}

void WebRuntimeFeatures::enableDirectoryUpload(bool enable)
{
    RuntimeEnabledFeatures::setDirectoryUploadEnabled(enable);
}

bool WebRuntimeFeatures::isDirectoryUploadEnabled()
{
    return RuntimeEnabledFeatures::directoryUploadEnabled();
}

void WebRuntimeFeatures::enableExperimentalWebSocket(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalWebSocketEnabled(enable);
}

bool WebRuntimeFeatures::isExperimentalWebSocketEnabled()
{
    return RuntimeEnabledFeatures::experimentalWebSocketEnabled();
}

void WebRuntimeFeatures::enableWebMIDI(bool enable)
{
    return RuntimeEnabledFeatures::setWebMIDIEnabled(enable);
}

bool WebRuntimeFeatures::isWebMIDIEnabled()
{
    return RuntimeEnabledFeatures::webMIDIEnabled();
}

void WebRuntimeFeatures::enableIMEAPI(bool enable)
{
    RuntimeEnabledFeatures::setIMEAPIEnabled(enable);
}

bool WebRuntimeFeatures::isIMEAPIEnabled()
{
    return RuntimeEnabledFeatures::imeAPIEnabled();
}

void WebRuntimeFeatures::enableExperimentalCanvasFeatures(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(enable);
}

bool WebRuntimeFeatures::isExperimentalCanvasFeaturesEnabled()
{
    return RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled();
}

void WebRuntimeFeatures::enableSpeechSynthesis(bool enable)
{
    RuntimeEnabledFeatures::setSpeechSynthesisEnabled(enable);
}

bool WebRuntimeFeatures::isSpeechSynthesisEnabled()
{
    return RuntimeEnabledFeatures::speechSynthesisEnabled();
}

} // namespace WebKit
