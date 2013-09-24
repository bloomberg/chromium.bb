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

    WEBKIT_EXPORT static void enableApplicationCache(bool);
    WEBKIT_EXPORT static bool isApplicationCacheEnabled();

    WEBKIT_EXPORT static void enableDatabase(bool);
    WEBKIT_EXPORT static bool isDatabaseEnabled();

    WEBKIT_EXPORT static void enableDeviceMotion(bool);
    WEBKIT_EXPORT static bool isDeviceMotionEnabled();

    WEBKIT_EXPORT static void enableDeviceOrientation(bool);
    WEBKIT_EXPORT static bool isDeviceOrientationEnabled();

    WEBKIT_EXPORT static void enableDialogElement(bool);
    WEBKIT_EXPORT static bool isDialogElementEnabled();

    WEBKIT_EXPORT static void enableEncryptedMedia(bool);
    WEBKIT_EXPORT static bool isEncryptedMediaEnabled();

    WEBKIT_EXPORT static void enableLegacyEncryptedMedia(bool);
    WEBKIT_EXPORT static bool isLegacyEncryptedMediaEnabled();

    WEBKIT_EXPORT static void enablePrefixedEncryptedMedia(bool);
    WEBKIT_EXPORT static bool isPrefixedEncryptedMediaEnabled();

    WEBKIT_EXPORT static void enableExperimentalCanvasFeatures(bool);
    WEBKIT_EXPORT static bool isExperimentalCanvasFeaturesEnabled();

    WEBKIT_EXPORT static void enableFileSystem(bool);
    WEBKIT_EXPORT static bool isFileSystemEnabled();

    WEBKIT_EXPORT static void enableFullscreen(bool);
    WEBKIT_EXPORT static bool isFullscreenEnabled();

    WEBKIT_EXPORT static void enableGamepad(bool);
    WEBKIT_EXPORT static bool isGamepadEnabled();

    WEBKIT_EXPORT static void enableGeolocation(bool);
    WEBKIT_EXPORT static bool isGeolocationEnabled();

    WEBKIT_EXPORT static void enableLazyLayout(bool);

    WEBKIT_EXPORT static void enableLocalStorage(bool);
    WEBKIT_EXPORT static bool isLocalStorageEnabled();

    WEBKIT_EXPORT static void enableMediaPlayer(bool);
    WEBKIT_EXPORT static bool isMediaPlayerEnabled();

    WEBKIT_EXPORT static void enableWebKitMediaSource(bool);
    WEBKIT_EXPORT static bool isWebKitMediaSourceEnabled();

    WEBKIT_EXPORT static void enableMediaStream(bool);
    WEBKIT_EXPORT static bool isMediaStreamEnabled();

    WEBKIT_EXPORT static void enableNotifications(bool);
    WEBKIT_EXPORT static bool isNotificationsEnabled();

    WEBKIT_EXPORT static void enablePagePopup(bool);
    WEBKIT_EXPORT static bool isPagePopupEnabled();

    WEBKIT_EXPORT static void enablePeerConnection(bool);
    WEBKIT_EXPORT static bool isPeerConnectionEnabled();

    WEBKIT_EXPORT static void enableRequestAutocomplete(bool);
    WEBKIT_EXPORT static bool isRequestAutocompleteEnabled();

    WEBKIT_EXPORT static void enableScriptedSpeech(bool);
    WEBKIT_EXPORT static bool isScriptedSpeechEnabled();

    WEBKIT_EXPORT static void enableSessionStorage(bool);
    WEBKIT_EXPORT static bool isSessionStorageEnabled();

    WEBKIT_EXPORT static void enableSpeechInput(bool);
    WEBKIT_EXPORT static bool isSpeechInputEnabled();

    WEBKIT_EXPORT static void enableSpeechSynthesis(bool);
    WEBKIT_EXPORT static bool isSpeechSynthesisEnabled();

    WEBKIT_EXPORT static void enableTouch(bool);
    WEBKIT_EXPORT static bool isTouchEnabled();

    WEBKIT_EXPORT static void enableWebAnimationsCSS();
    WEBKIT_EXPORT static void enableWebAnimationsSVG();

    WEBKIT_EXPORT static void enableWebAudio(bool);
    WEBKIT_EXPORT static bool isWebAudioEnabled();

    WEBKIT_EXPORT static void enableWebGLDraftExtensions(bool);
    WEBKIT_EXPORT static bool isWebGLDraftExtensionsEnabled();

    WEBKIT_EXPORT static void enableWebMIDI(bool);
    WEBKIT_EXPORT static bool isWebMIDIEnabled();

    WEBKIT_EXPORT static void enableDataListElement(bool);
    WEBKIT_EXPORT static bool isDataListElementEnabled();

    WEBKIT_EXPORT static void enableHTMLImports(bool);
    WEBKIT_EXPORT static bool isHTMLImportsEnabled();

    WEBKIT_EXPORT static void enableEmbedderCustomElements(bool);

    WEBKIT_EXPORT static void enableOverlayScrollbars(bool);

    WEBKIT_EXPORT static void enableInputModeAttribute(bool);

    WEBKIT_EXPORT static void enableOverlayFullscreenVideo(bool);

private:
    WebRuntimeFeatures();
};

} // namespace WebKit

#endif
