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

#include "../../../Platform/chromium/public/WebCommon.h"

namespace WebKit {

// This class is used to enable runtime features of Blink.
// All features are disabled by default.
// Most clients should call enableStableFeatures() to enable
// features Blink has made API commitments to.
class WebRuntimeFeatures {
public:
    WEBKIT_EXPORT static void enableStableFeatures(bool);
    WEBKIT_EXPORT static void enableExperimentalFeatures(bool);
    WEBKIT_EXPORT static void enableTestOnlyFeatures(bool);

    // FIXME: Delete after removing all callers in Content.
    static void enableFullScreenAPI(bool enable) { enableFullscreen(enable); }
    static void enableIndexedDatabase(bool enable) { enableIndexedDB(enable); }

    WEBKIT_EXPORT static void enableApplicationCache(bool);
    WEBKIT_EXPORT static bool isApplicationCacheEnabled();

    WEBKIT_EXPORT static void enableCanvasPath(bool);
    WEBKIT_EXPORT static bool isCanvasPathEnabled();

    WEBKIT_EXPORT static void enableCSSCompositing(bool);
    WEBKIT_EXPORT static bool isCSSCompositingEnabled();

    WEBKIT_EXPORT static void enableCSSExclusions(bool);
    WEBKIT_EXPORT static bool isCSSExclusionsEnabled();

    WEBKIT_EXPORT static void enableCSSRegions(bool);
    WEBKIT_EXPORT static bool isCSSRegionsEnabled();

    WEBKIT_EXPORT static void enableCustomDOMElements(bool);
    WEBKIT_EXPORT static bool isCustomDOMElementsEnabled();

    WEBKIT_EXPORT static void enableDatabase(bool);
    WEBKIT_EXPORT static bool isDatabaseEnabled();

    WEBKIT_EXPORT static void enableDeviceMotion(bool);
    WEBKIT_EXPORT static bool isDeviceMotionEnabled();

    WEBKIT_EXPORT static void enableDeviceOrientation(bool);
    WEBKIT_EXPORT static bool isDeviceOrientationEnabled();

    WEBKIT_EXPORT static void enableDialogElement(bool);
    WEBKIT_EXPORT static bool isDialogElementEnabled();

    WEBKIT_EXPORT static void enableDirectoryUpload(bool);
    WEBKIT_EXPORT static bool isDirectoryUploadEnabled();

    WEBKIT_EXPORT static void enableEncryptedMedia(bool);
    WEBKIT_EXPORT static bool isEncryptedMediaEnabled();

    WEBKIT_EXPORT static void enableExperimentalCanvasFeatures(bool);
    WEBKIT_EXPORT static bool isExperimentalCanvasFeaturesEnabled();

    WEBKIT_EXPORT static void enableExperimentalContentSecurityPolicyFeatures(bool);
    WEBKIT_EXPORT static bool isExperimentalContentSecurityPolicyFeaturesEnabled();

    WEBKIT_EXPORT static void enableExperimentalShadowDOM(bool);
    WEBKIT_EXPORT static bool isExperimentalShadowDOMEnabled();

    WEBKIT_EXPORT static void enableExperimentalWebSocket(bool);
    WEBKIT_EXPORT static bool isExperimentalWebSocketEnabled();

    WEBKIT_EXPORT static void enableFileSystem(bool);
    WEBKIT_EXPORT static bool isFileSystemEnabled();

    WEBKIT_EXPORT static void enableFontLoadEvents(bool);
    WEBKIT_EXPORT static bool isFontLoadEventsEnabled();

    WEBKIT_EXPORT static void enableFullscreen(bool);
    WEBKIT_EXPORT static bool isFullscreenEnabled();

    WEBKIT_EXPORT static void enableGamepad(bool);
    WEBKIT_EXPORT static bool isGamepadEnabled();

    WEBKIT_EXPORT static void enableGeolocation(bool);
    WEBKIT_EXPORT static bool isGeolocationEnabled();

    WEBKIT_EXPORT static void enableIMEAPI(bool);
    WEBKIT_EXPORT static bool isIMEAPIEnabled();

    WEBKIT_EXPORT static void enableIndexedDB(bool);
    WEBKIT_EXPORT static bool isIndexedDBEnabled();

    WEBKIT_EXPORT static void enableInputTypeDateTime(bool);
    WEBKIT_EXPORT static bool isInputTypeDateTimeEnabled();

    WEBKIT_EXPORT static void enableInputTypeWeek(bool);
    WEBKIT_EXPORT static bool isInputTypeWeekEnabled();

    WEBKIT_EXPORT static void enableJavaScriptI18NAPI(bool);
    WEBKIT_EXPORT static bool isJavaScriptI18NAPIEnabled();

    WEBKIT_EXPORT static void enableLazyLayout(bool);
    WEBKIT_EXPORT static bool isLazyLayoutEnabled();

    WEBKIT_EXPORT static void enableLocalStorage(bool);
    WEBKIT_EXPORT static bool isLocalStorageEnabled();

    WEBKIT_EXPORT static void enableMediaPlayer(bool);
    WEBKIT_EXPORT static bool isMediaPlayerEnabled();

    WEBKIT_EXPORT static void enableMediaSource(bool);
    WEBKIT_EXPORT static bool isMediaSourceEnabled();

    WEBKIT_EXPORT static void enableMediaStream(bool);
    WEBKIT_EXPORT static bool isMediaStreamEnabled();

    WEBKIT_EXPORT static void enableNotifications(bool);
    WEBKIT_EXPORT static bool isNotificationsEnabled();

    WEBKIT_EXPORT static void enablePeerConnection(bool);
    WEBKIT_EXPORT static bool isPeerConnectionEnabled();

    WEBKIT_EXPORT static void enableQuota(bool);
    WEBKIT_EXPORT static bool isQuotaEnabled();

    WEBKIT_EXPORT static void enableRequestAutocomplete(bool);
    WEBKIT_EXPORT static bool isRequestAutocompleteEnabled();

    WEBKIT_EXPORT static void enableScriptedSpeech(bool);
    WEBKIT_EXPORT static bool isScriptedSpeechEnabled();

    WEBKIT_EXPORT static void enableSeamlessIFrames(bool);
    WEBKIT_EXPORT static bool isSeamlessIFramesEnabled();

    WEBKIT_EXPORT static void enableSessionStorage(bool);
    WEBKIT_EXPORT static bool isSessionStorageEnabled();

    WEBKIT_EXPORT static void enableSpeechInput(bool);
    WEBKIT_EXPORT static bool isSpeechInputEnabled();

    WEBKIT_EXPORT static void enableSpeechSynthesis(bool);
    WEBKIT_EXPORT static bool isSpeechSynthesisEnabled();

    WEBKIT_EXPORT static void enableStyleScoped(bool);
    WEBKIT_EXPORT static bool isStyleScopedEnabled();

    WEBKIT_EXPORT static void enableTouch(bool);
    WEBKIT_EXPORT static bool isTouchEnabled();

    WEBKIT_EXPORT static void enableVideoTrack(bool);
    WEBKIT_EXPORT static bool isVideoTrackEnabled();

    WEBKIT_EXPORT static void enableWebAudio(bool);
    WEBKIT_EXPORT static bool isWebAudioEnabled();

    WEBKIT_EXPORT static void enableWebMIDI(bool);
    WEBKIT_EXPORT static bool isWebMIDIEnabled();

    WEBKIT_EXPORT static void enableWebPInAcceptHeader(bool);
    WEBKIT_EXPORT static bool isWebPInAcceptHeaderEnabled();

private:
    WebRuntimeFeatures();
};

} // namespace WebKit

#endif
