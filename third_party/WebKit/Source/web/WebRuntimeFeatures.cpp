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

#include "config.h"
#include "WebRuntimeFeatures.h"

#include "RuntimeEnabledFeatures.h"
#include "WebMediaPlayerClientImpl.h"

using namespace WebCore;

namespace WebKit {

void WebRuntimeFeatures::enableStableFeatures(bool enable)
{
    RuntimeEnabledFeatures::setStableFeaturesEnabled(enable);
    // FIXME: enableMediaPlayer does not use RuntimeEnabledFeatures
    // and does not belong as part of WebRuntimeFeatures.
    enableMediaPlayer(enable);
}

void WebRuntimeFeatures::enableExperimentalFeatures(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalFeaturesEnabled(enable);
}

void WebRuntimeFeatures::enableTestOnlyFeatures(bool enable)
{
    RuntimeEnabledFeatures::setTestFeaturesEnabled(enable);
}

void WebRuntimeFeatures::enableApplicationCache(bool enable)
{
    RuntimeEnabledFeatures::setApplicationCacheEnabled(enable);
}

bool WebRuntimeFeatures::isApplicationCacheEnabled()
{
    return RuntimeEnabledFeatures::applicationCacheEnabled();
}

void WebRuntimeFeatures::enableDatabase(bool enable)
{
    RuntimeEnabledFeatures::setDatabaseEnabled(enable);
}

bool WebRuntimeFeatures::isDatabaseEnabled()
{
    return RuntimeEnabledFeatures::databaseEnabled();
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

void WebRuntimeFeatures::enableDialogElement(bool enable)
{
    RuntimeEnabledFeatures::setDialogElementEnabled(enable);
}

bool WebRuntimeFeatures::isDialogElementEnabled()
{
    return RuntimeEnabledFeatures::dialogElementEnabled();
}

void WebRuntimeFeatures::enableEncryptedMedia(bool enable)
{
    RuntimeEnabledFeatures::setEncryptedMediaEnabled(enable);
    // FIXME: Hack to allow MediaKeyError to be enabled for either version.
    RuntimeEnabledFeatures::setEncryptedMediaAnyVersionEnabled(
        RuntimeEnabledFeatures::encryptedMediaEnabled()
        || RuntimeEnabledFeatures::legacyEncryptedMediaEnabled());
}

bool WebRuntimeFeatures::isEncryptedMediaEnabled()
{
    return RuntimeEnabledFeatures::encryptedMediaEnabled();
}

void WebRuntimeFeatures::enableLegacyEncryptedMedia(bool enable)
{
    RuntimeEnabledFeatures::setLegacyEncryptedMediaEnabled(enable);
    // FIXME: Hack to allow MediaKeyError to be enabled for either version.
    RuntimeEnabledFeatures::setEncryptedMediaAnyVersionEnabled(
        RuntimeEnabledFeatures::encryptedMediaEnabled()
        || RuntimeEnabledFeatures::legacyEncryptedMediaEnabled());
}

bool WebRuntimeFeatures::isLegacyEncryptedMediaEnabled()
{
    return RuntimeEnabledFeatures::legacyEncryptedMediaEnabled();
}

void WebRuntimeFeatures::enableExperimentalCanvasFeatures(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(enable);
}

bool WebRuntimeFeatures::isExperimentalCanvasFeaturesEnabled()
{
    return RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled();
}

void WebRuntimeFeatures::enableFileSystem(bool enable)
{
    RuntimeEnabledFeatures::setFileSystemEnabled(enable);
}

bool WebRuntimeFeatures::isFileSystemEnabled()
{
    return RuntimeEnabledFeatures::fileSystemEnabled();
}

void WebRuntimeFeatures::enableFullscreen(bool enable)
{
    RuntimeEnabledFeatures::setFullscreenEnabled(enable);
}

bool WebRuntimeFeatures::isFullscreenEnabled()
{
    return RuntimeEnabledFeatures::fullscreenEnabled();
}

void WebRuntimeFeatures::enableGamepad(bool enable)
{
    RuntimeEnabledFeatures::setGamepadEnabled(enable);
}

bool WebRuntimeFeatures::isGamepadEnabled()
{
    return RuntimeEnabledFeatures::gamepadEnabled();
}

void WebRuntimeFeatures::enableGeolocation(bool enable)
{
    RuntimeEnabledFeatures::setGeolocationEnabled(enable);
}

bool WebRuntimeFeatures::isGeolocationEnabled()
{
    return RuntimeEnabledFeatures::geolocationEnabled();
}

void WebRuntimeFeatures::enableLazyLayout(bool enable)
{
    // FIXME: Remove this once Chromium stops calling this.
}

void WebRuntimeFeatures::enableLocalStorage(bool enable)
{
    RuntimeEnabledFeatures::setLocalStorageEnabled(enable);
}

bool WebRuntimeFeatures::isLocalStorageEnabled()
{
    return RuntimeEnabledFeatures::localStorageEnabled();
}

void WebRuntimeFeatures::enableMediaPlayer(bool enable)
{
    RuntimeEnabledFeatures::setMediaEnabled(enable);
}

bool WebRuntimeFeatures::isMediaPlayerEnabled()
{
    return RuntimeEnabledFeatures::mediaEnabled();
}

void WebRuntimeFeatures::enableWebKitMediaSource(bool enable)
{
    RuntimeEnabledFeatures::setWebKitMediaSourceEnabled(enable);
}

bool WebRuntimeFeatures::isWebKitMediaSourceEnabled()
{
    return RuntimeEnabledFeatures::webKitMediaSourceEnabled();
}

void WebRuntimeFeatures::enableMediaStream(bool enable)
{
    RuntimeEnabledFeatures::setMediaStreamEnabled(enable);
}

bool WebRuntimeFeatures::isMediaStreamEnabled()
{
    return RuntimeEnabledFeatures::mediaStreamEnabled();
}

void WebRuntimeFeatures::enableNotifications(bool enable)
{
    RuntimeEnabledFeatures::setNotificationsEnabled(enable);
}

bool WebRuntimeFeatures::isNotificationsEnabled()
{
    return RuntimeEnabledFeatures::notificationsEnabled();
}

void WebRuntimeFeatures::enablePagePopup(bool enable)
{
    RuntimeEnabledFeatures::setPagePopupEnabled(enable);
}

bool WebRuntimeFeatures::isPagePopupEnabled()
{
    return RuntimeEnabledFeatures::pagePopupEnabled();
}

void WebRuntimeFeatures::enablePeerConnection(bool enable)
{
    RuntimeEnabledFeatures::setPeerConnectionEnabled(enable);
}

bool WebRuntimeFeatures::isPeerConnectionEnabled()
{
    return RuntimeEnabledFeatures::peerConnectionEnabled();
}

void WebRuntimeFeatures::enableRequestAutocomplete(bool enable)
{
    RuntimeEnabledFeatures::setRequestAutocompleteEnabled(enable);
}

bool WebRuntimeFeatures::isRequestAutocompleteEnabled()
{
    return RuntimeEnabledFeatures::requestAutocompleteEnabled();
}

void WebRuntimeFeatures::enableScriptedSpeech(bool enable)
{
    RuntimeEnabledFeatures::setScriptedSpeechEnabled(enable);
}

bool WebRuntimeFeatures::isScriptedSpeechEnabled()
{
    return RuntimeEnabledFeatures::scriptedSpeechEnabled();
}

void WebRuntimeFeatures::enableSessionStorage(bool enable)
{
    RuntimeEnabledFeatures::setSessionStorageEnabled(enable);
}

bool WebRuntimeFeatures::isSessionStorageEnabled()
{
    return RuntimeEnabledFeatures::sessionStorageEnabled();
}

void WebRuntimeFeatures::enableSpeechInput(bool enable)
{
    RuntimeEnabledFeatures::setSpeechInputEnabled(enable);
}

bool WebRuntimeFeatures::isSpeechInputEnabled()
{
    return RuntimeEnabledFeatures::speechInputEnabled();
}

void WebRuntimeFeatures::enableSpeechSynthesis(bool enable)
{
    RuntimeEnabledFeatures::setSpeechSynthesisEnabled(enable);
}

bool WebRuntimeFeatures::isSpeechSynthesisEnabled()
{
    return RuntimeEnabledFeatures::speechSynthesisEnabled();
}

void WebRuntimeFeatures::enableTouch(bool enable)
{
    RuntimeEnabledFeatures::setTouchEnabled(enable);
}

bool WebRuntimeFeatures::isTouchEnabled()
{
    return RuntimeEnabledFeatures::touchEnabled();
}

void WebRuntimeFeatures::enableWebAnimationsCSS()
{
    RuntimeEnabledFeatures::setWebAnimationsEnabled(true);
    RuntimeEnabledFeatures::setWebAnimationsCSSEnabled(true);
}

void WebRuntimeFeatures::enableWebAnimationsSVG()
{
    RuntimeEnabledFeatures::setWebAnimationsEnabled(true);
    RuntimeEnabledFeatures::setWebAnimationsSVGEnabled(true);
}

void WebRuntimeFeatures::enableWebAudio(bool enable)
{
    RuntimeEnabledFeatures::setWebAudioEnabled(enable);
}

bool WebRuntimeFeatures::isWebAudioEnabled()
{
    return RuntimeEnabledFeatures::webAudioEnabled();
}

void WebRuntimeFeatures::enableWebGLDraftExtensions(bool enable)
{
    RuntimeEnabledFeatures::setWebGLDraftExtensionsEnabled(enable);
}

bool WebRuntimeFeatures::isWebGLDraftExtensionsEnabled()
{
    return RuntimeEnabledFeatures::webGLDraftExtensionsEnabled();
}

void WebRuntimeFeatures::enableWebMIDI(bool enable)
{
    return RuntimeEnabledFeatures::setWebMIDIEnabled(enable);
}

bool WebRuntimeFeatures::isWebMIDIEnabled()
{
    return RuntimeEnabledFeatures::webMIDIEnabled();
}

void WebRuntimeFeatures::enableDataListElement(bool enable)
{
    RuntimeEnabledFeatures::setDataListElementEnabled(enable);
}

bool WebRuntimeFeatures::isDataListElementEnabled()
{
    return RuntimeEnabledFeatures::dataListElementEnabled();
}

void WebRuntimeFeatures::enableHTMLImports(bool enable)
{
    RuntimeEnabledFeatures::setHTMLImportsEnabled(enable);
}

bool WebRuntimeFeatures::isHTMLImportsEnabled()
{
    return RuntimeEnabledFeatures::htmlImportsEnabled();
}

void WebRuntimeFeatures::enableEmbedderCustomElements(bool enable)
{
    RuntimeEnabledFeatures::setEmbedderCustomElementsEnabled(enable);
}

void WebRuntimeFeatures::enableOverlayScrollbars(bool enable)
{
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(enable);
}

void WebRuntimeFeatures::enableInputModeAttribute(bool enable)
{
    RuntimeEnabledFeatures::setInputModeAttributeEnabled(enable);
}

void WebRuntimeFeatures::enableOverlayFullscreenVideo(bool enable)
{
    RuntimeEnabledFeatures::setOverlayFullscreenVideoEnabled(enable);
}

} // namespace WebKit
