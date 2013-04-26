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
#include "core/page/RuntimeEnabledFeatures.h"

#include "core/platform/graphics/MediaPlayer.h"
#include "core/workers/SharedWorkerRepository.h"

namespace WebCore {

bool RuntimeEnabledFeatures::isLocalStorageEnabled = true;
bool RuntimeEnabledFeatures::isSessionStorageEnabled = true;
bool RuntimeEnabledFeatures::isWebkitNotificationsEnabled = false;
bool RuntimeEnabledFeatures::isApplicationCacheEnabled = true;
bool RuntimeEnabledFeatures::isGeolocationEnabled = true;
bool RuntimeEnabledFeatures::isIndexedDBEnabled = false;
bool RuntimeEnabledFeatures::isCanvasPathEnabled = false;
#if ENABLE(CSS_EXCLUSIONS)
bool RuntimeEnabledFeatures::isCSSExclusionsEnabled = false;
#endif
#if ENABLE(CSS_REGIONS)
bool RuntimeEnabledFeatures::isCSSRegionsEnabled = false;
#endif
#if ENABLE(CSS_COMPOSITING)
bool RuntimeEnabledFeatures::isCSSCompositingEnabled = false;
#endif
bool RuntimeEnabledFeatures::isFontLoadEventsEnabled = false;
bool RuntimeEnabledFeatures::isFullscreenEnabled = true;
bool RuntimeEnabledFeatures::isDatabaseEnabled = true;
#if ENABLE(WEB_AUDIO)
bool RuntimeEnabledFeatures::isAudioContextEnabled = false;
#endif
bool RuntimeEnabledFeatures::isWebMIDIEnabled = false;
bool RuntimeEnabledFeatures::isTouchEnabled = true;
bool RuntimeEnabledFeatures::isDeviceMotionEnabled = true;
bool RuntimeEnabledFeatures::isDeviceOrientationEnabled = true;
bool RuntimeEnabledFeatures::isSpeechInputEnabled = true;
bool RuntimeEnabledFeatures::isScriptedSpeechEnabled = false;
bool RuntimeEnabledFeatures::isFileSystemEnabled = false;
#if ENABLE(JAVASCRIPT_I18N_API)
bool RuntimeEnabledFeatures::isJavaScriptI18NAPIEnabled = false;
#endif
#if ENABLE(MEDIA_STREAM)
bool RuntimeEnabledFeatures::isMediaStreamEnabled = true;
#endif
#if ENABLE(MEDIA_STREAM)
bool RuntimeEnabledFeatures::isPeerConnectionEnabled = true;
#endif
bool RuntimeEnabledFeatures::isWebkitGetGamepadsEnabled = false;
bool RuntimeEnabledFeatures::isQuotaEnabled = false;
bool RuntimeEnabledFeatures::isMediaSourceEnabled = false;
#if ENABLE(ENCRYPTED_MEDIA)
bool RuntimeEnabledFeatures::isEncryptedMediaEnabled = false;
#endif
bool RuntimeEnabledFeatures::isWebkitVideoTrackEnabled = true;
bool RuntimeEnabledFeatures::isExperimentalShadowDOMEnabled = false;
bool RuntimeEnabledFeatures::isAuthorShadowDOMForAnyElementEnabled = false;
bool RuntimeEnabledFeatures::isCustomDOMElementsEnabled = false;
bool RuntimeEnabledFeatures::isStyleScopedEnabled = false;
#if ENABLE(INPUT_TYPE_DATETIME_INCOMPLETE)
bool RuntimeEnabledFeatures::isInputTypeDateTimeEnabled = false;
#endif
bool RuntimeEnabledFeatures::isInputTypeWeekEnabled = true;
bool RuntimeEnabledFeatures::isDialogElementEnabled = false;
bool RuntimeEnabledFeatures::isLazyLayoutEnabled = false;
bool RuntimeEnabledFeatures::isExperimentalContentSecurityPolicyFeaturesEnabled = false;
bool RuntimeEnabledFeatures::isSeamlessIFramesEnabled = false;
bool RuntimeEnabledFeatures::isLangAttributeAwareFormControlUIEnabled = false;
bool RuntimeEnabledFeatures::isRequestAutocompleteEnabled = false;
bool RuntimeEnabledFeatures::isWebPInAcceptHeaderEnabled = false;
bool RuntimeEnabledFeatures::isDirectoryUploadEnabled = true;
bool RuntimeEnabledFeatures::isExperimentalWebSocketEnabled = false;
bool RuntimeEnabledFeatures::isIMEAPIEnabled = false;

bool RuntimeEnabledFeatures::mediaEnabled()
{
    return MediaPlayer::isAvailable();
}

#if ENABLE(SHARED_WORKERS)
bool RuntimeEnabledFeatures::sharedWorkerEnabled()
{
    return SharedWorkerRepository::isAvailable();
}
#endif

} // namespace WebCore
