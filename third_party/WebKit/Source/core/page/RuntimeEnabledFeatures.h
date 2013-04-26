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

#ifndef RuntimeEnabledFeatures_h
#define RuntimeEnabledFeatures_h

namespace WebCore {

// A class that stores static enablers for all experimental features. Note that
// the method names must line up with the JavaScript method they enable for code
// generation to work properly.

class RuntimeEnabledFeatures {
public:
    static void setLocalStorageEnabled(bool isEnabled) { isLocalStorageEnabled = isEnabled; }
    static bool localStorageEnabled() { return isLocalStorageEnabled; }

    static void setSessionStorageEnabled(bool isEnabled) { isSessionStorageEnabled = isEnabled; }
    static bool sessionStorageEnabled() { return isSessionStorageEnabled; }

    static void setWebkitNotificationsEnabled(bool isEnabled) { isWebkitNotificationsEnabled = isEnabled; }
    static bool webkitNotificationsEnabled() { return isWebkitNotificationsEnabled; }

    static void setApplicationCacheEnabled(bool isEnabled) { isApplicationCacheEnabled = isEnabled; }
    static bool applicationCacheEnabled() { return isApplicationCacheEnabled; }

    static void setGeolocationEnabled(bool isEnabled) { isGeolocationEnabled = isEnabled; }
    static bool geolocationEnabled() { return isGeolocationEnabled; }

    static void setIndexedDBEnabled(bool isEnabled) { isIndexedDBEnabled = isEnabled; }
    static bool indexedDBEnabled() { return isIndexedDBEnabled; }

    static void setCanvasPathEnabled(bool isEnabled) { isCanvasPathEnabled = isEnabled; }
    static bool canvasPathEnabled() { return isCanvasPathEnabled; }

#if ENABLE(CSS_EXCLUSIONS)
    static void setCSSExclusionsEnabled(bool isEnabled) { isCSSExclusionsEnabled = isEnabled; }
    static bool cssExclusionsEnabled() { return isCSSExclusionsEnabled; }
#else
    static void setCSSExclusionsEnabled(bool) { }
    static bool cssExclusionsEnabled() { return false; }
#endif

#if ENABLE(CSS_REGIONS)
    static void setCSSRegionsEnabled(bool isEnabled) { isCSSRegionsEnabled = isEnabled; }
    static bool cssRegionsEnabled() { return isCSSRegionsEnabled; }
#else
    static void setCSSRegionsEnabled(bool) { }
    static bool cssRegionsEnabled() { return false; }
#endif

#if ENABLE(CSS_COMPOSITING)
    static void setCSSCompositingEnabled(bool isEnabled) { isCSSCompositingEnabled = isEnabled; }
    static bool cssCompositingEnabled() { return isCSSCompositingEnabled; }
#else
    static void setCSSCompositingEnabled(bool) { }
    static bool cssCompositingEnabled() { return false; }
#endif

    static void setFontLoadEventsEnabled(bool isEnabled) { isFontLoadEventsEnabled = isEnabled; }
    static bool fontLoadEventsEnabled() { return isFontLoadEventsEnabled; }

    static void setFullscreenEnabled(bool isEnabled) { isFullscreenEnabled = isEnabled; }
    static bool fullscreenEnabled() { return isFullscreenEnabled; }

    static bool mediaEnabled();

    static bool sharedWorkerEnabled();

    static void setDatabaseEnabled(bool isEnabled) { isDatabaseEnabled = isEnabled; }
    static bool databaseEnabled() { return isDatabaseEnabled; }

#if ENABLE(WEB_AUDIO)
    static void setAudioContextEnabled(bool isEnabled) { isAudioContextEnabled = isEnabled; }
    static bool audioContextEnabled() { return isAudioContextEnabled; }
#else
    static void setAudioContextEnabled(bool) { }
    static bool audioContextEnabled() { return false; }
#endif

    static void setWebMIDIEnabled(bool isEnabled) { isWebMIDIEnabled = isEnabled; }
    static bool webMIDIEnabled() { return isWebMIDIEnabled; }

    static void setTouchEnabled(bool isEnabled) { isTouchEnabled = isEnabled; }
    static bool touchEnabled() { return isTouchEnabled; }

    static void setDeviceMotionEnabled(bool isEnabled) { isDeviceMotionEnabled = isEnabled; }
    static bool deviceMotionEnabled() { return isDeviceMotionEnabled; }

    static void setDeviceOrientationEnabled(bool isEnabled) { isDeviceOrientationEnabled = isEnabled; }
    static bool deviceOrientationEnabled() { return isDeviceOrientationEnabled; }

    static void setSpeechInputEnabled(bool isEnabled) { isSpeechInputEnabled = isEnabled; }
    static bool speechInputEnabled() { return isSpeechInputEnabled; }

    static void setScriptedSpeechEnabled(bool isEnabled) { isScriptedSpeechEnabled = isEnabled; }
    static bool scriptedSpeechEnabled() { return isScriptedSpeechEnabled; }

    static void setFileSystemEnabled(bool isEnabled) { isFileSystemEnabled = isEnabled; }
    static bool fileSystemEnabled() { return isFileSystemEnabled; }

#if ENABLE(JAVASCRIPT_I18N_API)
    static void setJavaScriptI18NAPIEnabled(bool isEnabled) { isJavaScriptI18NAPIEnabled = isEnabled; }
    static bool javaScriptI18NAPIEnabled() { return isJavaScriptI18NAPIEnabled; }
#else
    static void setJavaScriptI18NAPIEnabled(bool) { }
    static bool javaScriptI18NAPIEnabled() { return false; }
#endif

#if ENABLE(MEDIA_STREAM)
    static void setMediaStreamEnabled(bool isEnabled) { isMediaStreamEnabled = isEnabled; }
    static bool mediaStreamEnabled() { return isMediaStreamEnabled; }
#else
    static void setMediaStreamEnabled(bool) { }
    static bool mediaStreamEnabled() { return false; }
#endif

#if ENABLE(MEDIA_STREAM)
    static void setPeerConnectionEnabled(bool isEnabled) { isPeerConnectionEnabled = isEnabled; }
    static bool peerConnectionEnabled() { return isMediaStreamEnabled && isPeerConnectionEnabled; }
#else
    static void setPeerConnectionEnabled(bool) { }
    static bool peerConnectionEnabled() { return false; }
#endif

    static void setWebkitGetGamepadsEnabled(bool isEnabled) { isWebkitGetGamepadsEnabled = isEnabled; }
    static bool webkitGetGamepadsEnabled() { return isWebkitGetGamepadsEnabled; }

    static void setQuotaEnabled(bool isEnabled) { isQuotaEnabled = isEnabled; }
    static bool quotaEnabled() { return isQuotaEnabled; }

    static void setMediaSourceEnabled(bool isEnabled) { isMediaSourceEnabled = isEnabled; }
    static bool mediaSourceEnabled() { return isMediaSourceEnabled; }

#if ENABLE(ENCRYPTED_MEDIA)
    static void setEncryptedMediaEnabled(bool isEnabled) { isEncryptedMediaEnabled = isEnabled; }
    static bool encryptedMediaEnabled() { return isEncryptedMediaEnabled; }
#else
    static void setEncryptedMediaEnabled(bool) { }
    static bool encryptedMediaEnabled() { return false; }
#endif

    static void setWebkitVideoTrackEnabled(bool isEnabled) { isWebkitVideoTrackEnabled = isEnabled; }
    static bool webkitVideoTrackEnabled() { return isWebkitVideoTrackEnabled; }

    static void setExperimentalShadowDOMEnabled(bool isEnabled) { isExperimentalShadowDOMEnabled = isEnabled; }
    static bool experimentalShadowDOMEnabled() { return isExperimentalShadowDOMEnabled; }

    static void setAuthorShadowDOMForAnyElementEnabled(bool isEnabled) { isAuthorShadowDOMForAnyElementEnabled = isEnabled; }
    static bool authorShadowDOMForAnyElementEnabled() { return isAuthorShadowDOMForAnyElementEnabled; }

    static void setCustomDOMElementsEnabled(bool isEnabled) { isCustomDOMElementsEnabled = isEnabled; }
    static bool customDOMElementsEnabled() { return isCustomDOMElementsEnabled; }

    static void setStyleScopedEnabled(bool isEnabled) { isStyleScopedEnabled = isEnabled; }
    static bool styleScopedEnabled() { return isStyleScopedEnabled; }

#if ENABLE(INPUT_TYPE_DATETIME_INCOMPLETE)
    static void setInputTypeDateTimeEnabled(bool isEnabled) { isInputTypeDateTimeEnabled = isEnabled; }
    static bool inputTypeDateTimeEnabled() { return isInputTypeDateTimeEnabled; }
#else
    static void setInputTypeDateTimeEnabled(bool) { }
    static bool inputTypeDateTimeEnabled() { return false; }
#endif

    static void setInputTypeWeekEnabled(bool isEnabled) { isInputTypeWeekEnabled = isEnabled; }
    static bool inputTypeWeekEnabled() { return isInputTypeWeekEnabled; }

    static void setDialogElementEnabled(bool isEnabled) { isDialogElementEnabled = isEnabled; }
    static bool dialogElementEnabled() { return isDialogElementEnabled; }

    static void setLazyLayoutEnabled(bool isEnabled) { isLazyLayoutEnabled = isEnabled; }
    static bool lazyLayoutEnabled() { return isLazyLayoutEnabled; }

    static void setExperimentalContentSecurityPolicyFeaturesEnabled(bool isEnabled) { isExperimentalContentSecurityPolicyFeaturesEnabled = isEnabled; }
    static bool experimentalContentSecurityPolicyFeaturesEnabled() { return isExperimentalContentSecurityPolicyFeaturesEnabled; }

    static void setSeamlessIFramesEnabled(bool isEnabled) { isSeamlessIFramesEnabled = isEnabled; }
    static bool seamlessIFramesEnabled() { return isSeamlessIFramesEnabled; }

    // The lang attribute support is incomplete and should only be turned on for tests.
    static void setLangAttributeAwareFormControlUIEnabled(bool isEnabled) { isLangAttributeAwareFormControlUIEnabled = isEnabled; }
    static bool langAttributeAwareFormControlUIEnabled() { return isLangAttributeAwareFormControlUIEnabled; }

    static void setRequestAutocompleteEnabled(bool isEnabled) { isRequestAutocompleteEnabled = isEnabled; }
    static bool requestAutocompleteEnabled() { return isRequestAutocompleteEnabled; }

    static void setWebPInAcceptHeaderEnabled(bool isEnabled) { isWebPInAcceptHeaderEnabled = isEnabled; }
    static bool webPInAcceptHeaderEnabled() { return isWebPInAcceptHeaderEnabled; }

    static void setDirectoryUploadEnabled(bool isEnabled) { isDirectoryUploadEnabled = isEnabled; }
    static bool directoryUploadEnabled() { return isDirectoryUploadEnabled; }

    static void setExperimentalWebSocketEnabled(bool isEnabled) { isExperimentalWebSocketEnabled = isEnabled; }
    static bool experimentalWebSocketEnabled() { return isExperimentalWebSocketEnabled; }

    static void setIMEAPIEnabled(bool isEnabled) { isIMEAPIEnabled = isEnabled; }
    static bool imeAPIEnabled() { return isIMEAPIEnabled; }

private:
    // Never instantiate.
    RuntimeEnabledFeatures() { }

    static bool isLocalStorageEnabled;
    static bool isSessionStorageEnabled;
    static bool isWebkitNotificationsEnabled;
    static bool isApplicationCacheEnabled;
    static bool isGeolocationEnabled;
    static bool isIndexedDBEnabled;
    static bool isCanvasPathEnabled;
#if ENABLE(CSS_EXCLUSIONS)
    static bool isCSSExclusionsEnabled;
#endif
#if ENABLE(CSS_REGIONS)
    static bool isCSSRegionsEnabled;
#endif
#if ENABLE(CSS_COMPOSITING)
    static bool isCSSCompositingEnabled;
#endif
    static bool isFontLoadEventsEnabled;
    static bool isFullscreenEnabled;
    static bool isDatabaseEnabled;
#if ENABLE(WEB_AUDIO)
    static bool isAudioContextEnabled;
#endif
    static bool isWebMIDIEnabled;
    static bool isTouchEnabled;
    static bool isDeviceMotionEnabled;
    static bool isDeviceOrientationEnabled;
    static bool isSpeechInputEnabled;
    static bool isScriptedSpeechEnabled;
    static bool isFileSystemEnabled;
#if ENABLE(JAVASCRIPT_I18N_API)
    static bool isJavaScriptI18NAPIEnabled;
#endif
#if ENABLE(MEDIA_STREAM)
    static bool isMediaStreamEnabled;
#endif
#if ENABLE(MEDIA_STREAM)
    static bool isPeerConnectionEnabled;
#endif
    static bool isWebkitGetGamepadsEnabled;
    static bool isQuotaEnabled;
    static bool isMediaSourceEnabled;
#if ENABLE(ENCRYPTED_MEDIA)
    static bool isEncryptedMediaEnabled;
#endif
    static bool isWebkitVideoTrackEnabled;
    static bool isExperimentalShadowDOMEnabled;
    static bool isAuthorShadowDOMForAnyElementEnabled;
    static bool isCustomDOMElementsEnabled;
    static bool isStyleScopedEnabled;
#if ENABLE(INPUT_TYPE_DATETIME_INCOMPLETE)
    static bool isInputTypeDateTimeEnabled;
#endif
    static bool isInputTypeWeekEnabled;
    static bool isDialogElementEnabled;
    static bool isLazyLayoutEnabled;
    static bool isExperimentalContentSecurityPolicyFeaturesEnabled;
    static bool isSeamlessIFramesEnabled;
    static bool isLangAttributeAwareFormControlUIEnabled;
    static bool isRequestAutocompleteEnabled;
    static bool isWebPInAcceptHeaderEnabled;
    static bool isDirectoryUploadEnabled;
    static bool isExperimentalWebSocketEnabled;
    static bool isIMEAPIEnabled;
};

} // namespace WebCore

#endif // RuntimeEnabledFeatures_h
