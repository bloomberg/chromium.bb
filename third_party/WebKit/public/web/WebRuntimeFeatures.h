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
    BLINK_EXPORT static void enableStableFeatures(bool);
    BLINK_EXPORT static void enableExperimentalFeatures(bool);
    BLINK_EXPORT static void enableTestOnlyFeatures(bool);

    BLINK_EXPORT static void enableApplicationCache(bool);
    BLINK_EXPORT static bool isApplicationCacheEnabled();

    BLINK_EXPORT static void enableDatabase(bool);
    BLINK_EXPORT static bool isDatabaseEnabled();

    BLINK_EXPORT static void enableDeviceMotion(bool);
    BLINK_EXPORT static bool isDeviceMotionEnabled();

    BLINK_EXPORT static void enableDeviceOrientation(bool);
    BLINK_EXPORT static bool isDeviceOrientationEnabled();

    BLINK_EXPORT static void enableDialogElement(bool);
    BLINK_EXPORT static bool isDialogElementEnabled();

    BLINK_EXPORT static void enableEncryptedMedia(bool);
    BLINK_EXPORT static bool isEncryptedMediaEnabled();

    BLINK_EXPORT static void enablePrefixedEncryptedMedia(bool);
    BLINK_EXPORT static bool isPrefixedEncryptedMediaEnabled();

    BLINK_EXPORT static void enableExperimentalCanvasFeatures(bool);
    BLINK_EXPORT static bool isExperimentalCanvasFeaturesEnabled();

    BLINK_EXPORT static void enableFileSystem(bool);
    BLINK_EXPORT static bool isFileSystemEnabled();

    BLINK_EXPORT static void enableFullscreen(bool);
    BLINK_EXPORT static bool isFullscreenEnabled();

    BLINK_EXPORT static void enableGamepad(bool);
    BLINK_EXPORT static bool isGamepadEnabled();

    BLINK_EXPORT static void enableGeolocation(bool);
    BLINK_EXPORT static bool isGeolocationEnabled();

    BLINK_EXPORT static void enableLazyLayout(bool);

    BLINK_EXPORT static void enableLocalStorage(bool);
    BLINK_EXPORT static bool isLocalStorageEnabled();

    BLINK_EXPORT static void enableMediaPlayer(bool);
    BLINK_EXPORT static bool isMediaPlayerEnabled();

    BLINK_EXPORT static void enableWebKitMediaSource(bool);
    BLINK_EXPORT static bool isWebKitMediaSourceEnabled();

    BLINK_EXPORT static void enableMediaStream(bool);
    BLINK_EXPORT static bool isMediaStreamEnabled();

    BLINK_EXPORT static void enableNotifications(bool);
    BLINK_EXPORT static bool isNotificationsEnabled();

    BLINK_EXPORT static void enablePagePopup(bool);
    BLINK_EXPORT static bool isPagePopupEnabled();

    BLINK_EXPORT static void enablePeerConnection(bool);
    BLINK_EXPORT static bool isPeerConnectionEnabled();

    BLINK_EXPORT static void enableRequestAutocomplete(bool);
    BLINK_EXPORT static bool isRequestAutocompleteEnabled();

    BLINK_EXPORT static void enableScriptedSpeech(bool);
    BLINK_EXPORT static bool isScriptedSpeechEnabled();

    BLINK_EXPORT static void enableSessionStorage(bool);
    BLINK_EXPORT static bool isSessionStorageEnabled();

    BLINK_EXPORT static void enableSpeechInput(bool);
    BLINK_EXPORT static bool isSpeechInputEnabled();

    BLINK_EXPORT static void enableSpeechSynthesis(bool);
    BLINK_EXPORT static bool isSpeechSynthesisEnabled();

    BLINK_EXPORT static void enableTouch(bool);
    BLINK_EXPORT static bool isTouchEnabled();

    BLINK_EXPORT static void enableWebAnimationsCSS();
    BLINK_EXPORT static void enableWebAnimationsSVG();

    BLINK_EXPORT static void enableWebAudio(bool);
    BLINK_EXPORT static bool isWebAudioEnabled();

    BLINK_EXPORT static void enableWebGLDraftExtensions(bool);
    BLINK_EXPORT static bool isWebGLDraftExtensionsEnabled();

    BLINK_EXPORT static void enableWebMIDI(bool);
    BLINK_EXPORT static bool isWebMIDIEnabled();

    BLINK_EXPORT static void enableDataListElement(bool);
    BLINK_EXPORT static bool isDataListElementEnabled();

    BLINK_EXPORT static void enableHTMLImports(bool);
    BLINK_EXPORT static bool isHTMLImportsEnabled();

    BLINK_EXPORT static void enableXSLT(bool);

    BLINK_EXPORT static void enableEmbedderCustomElements(bool);

    BLINK_EXPORT static void enableOverlayScrollbars(bool);

    BLINK_EXPORT static void enableInputModeAttribute(bool);

    BLINK_EXPORT static void enableOverlayFullscreenVideo(bool);

private:
    WebRuntimeFeatures();
};

} // namespace WebKit

#endif
