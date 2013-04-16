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
#include "RuntimeEnabledFeatures.h"

#include "AsyncFileSystem.h"
#include "DatabaseManager.h"
#include "MediaPlayer.h"
#include "SharedWorkerRepository.h"
#include "WebSocket.h"

namespace WebCore {

bool RuntimeEnabledFeatures::isLocalStorageEnabled = true;
bool RuntimeEnabledFeatures::isSessionStorageEnabled = true;
bool RuntimeEnabledFeatures::isWebkitNotificationsEnabled = false;
bool RuntimeEnabledFeatures::isApplicationCacheEnabled = true;
bool RuntimeEnabledFeatures::isDataTransferItemsEnabled = true;
bool RuntimeEnabledFeatures::isGeolocationEnabled = true;
bool RuntimeEnabledFeatures::isIndexedDBEnabled = false;
bool RuntimeEnabledFeatures::isWebAudioEnabled = false;
bool RuntimeEnabledFeatures::isTouchEnabled = true;
bool RuntimeEnabledFeatures::isDeviceMotionEnabled = true;
bool RuntimeEnabledFeatures::isDeviceOrientationEnabled = true;
bool RuntimeEnabledFeatures::isSpeechInputEnabled = true;
bool RuntimeEnabledFeatures::isCanvasPathEnabled = false;
bool RuntimeEnabledFeatures::isCSSExclusionsEnabled = false;
bool RuntimeEnabledFeatures::isCSSRegionsEnabled = false;
bool RuntimeEnabledFeatures::isCSSCompositingEnabled = false;
bool RuntimeEnabledFeatures::isLangAttributeAwareFormControlUIEnabled = false;
bool RuntimeEnabledFeatures::isDirectoryUploadEnabled = true;
bool RuntimeEnabledFeatures::isScriptedSpeechEnabled = false;

#if ENABLE(MEDIA_STREAM)
bool RuntimeEnabledFeatures::isMediaStreamEnabled = true;
bool RuntimeEnabledFeatures::isPeerConnectionEnabled = true;
#endif

#if ENABLE(GAMEPAD)
bool RuntimeEnabledFeatures::isGamepadEnabled = false;
#endif

bool RuntimeEnabledFeatures::isFileSystemEnabled = false;

bool RuntimeEnabledFeatures::fileSystemEnabled()
{
    return isFileSystemEnabled && AsyncFileSystem::isAvailable();
}

#if ENABLE(JAVASCRIPT_I18N_API)
bool RuntimeEnabledFeatures::isJavaScriptI18NAPIEnabled = false;

bool RuntimeEnabledFeatures::javaScriptI18NAPIEnabled()
{
    return isJavaScriptI18NAPIEnabled;
}
#endif

#if ENABLE(VIDEO)

bool RuntimeEnabledFeatures::audioEnabled()
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::htmlMediaElementEnabled()
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::htmlAudioElementEnabled()
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::htmlVideoElementEnabled()
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::htmlSourceElementEnabled()
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::mediaControllerEnabled()
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::mediaErrorEnabled()
{
    return MediaPlayer::isAvailable();
}

bool RuntimeEnabledFeatures::timeRangesEnabled()
{
    return MediaPlayer::isAvailable();
}

#endif

#if ENABLE(SHARED_WORKERS)
bool RuntimeEnabledFeatures::sharedWorkerEnabled()
{
    return SharedWorkerRepository::isAvailable();
}
#endif

bool RuntimeEnabledFeatures::openDatabaseEnabled()
{
    return DatabaseManager::manager().isAvailable();
}

bool RuntimeEnabledFeatures::openDatabaseSyncEnabled()
{
    return DatabaseManager::manager().isAvailable();
}

bool RuntimeEnabledFeatures::isQuotaEnabled = false;

bool RuntimeEnabledFeatures::isFullScreenAPIEnabled = true;

bool RuntimeEnabledFeatures::isMediaSourceEnabled = false;

bool RuntimeEnabledFeatures::isVideoTrackEnabled = true;

#if ENABLE(ENCRYPTED_MEDIA)
bool RuntimeEnabledFeatures::isEncryptedMediaEnabled = false;
#endif

bool RuntimeEnabledFeatures::isShadowDOMEnabled = false;

bool RuntimeEnabledFeatures::isExperimentalShadowDOMEnabled = false;

bool RuntimeEnabledFeatures::isAuthorShadowDOMForAnyElementEnabled = false;

#if ENABLE(CUSTOM_ELEMENTS)
bool RuntimeEnabledFeatures::isCustomDOMElementsEnabled = false;
#endif

bool RuntimeEnabledFeatures::isStyleScopedEnabled = false;

#if ENABLE(INPUT_TYPE_DATETIME_INCOMPLETE)
bool RuntimeEnabledFeatures::isInputTypeDateTimeEnabled = false;
#endif

bool RuntimeEnabledFeatures::isInputTypeWeekEnabled = true;

#if ENABLE(DIALOG_ELEMENT)
bool RuntimeEnabledFeatures::isDialogElementEnabled = false;
#endif

bool RuntimeEnabledFeatures::isRequestAutocompleteEnabled = false;

bool RuntimeEnabledFeatures::areExperimentalContentSecurityPolicyFeaturesEnabled = false;

bool RuntimeEnabledFeatures::areSeamlessIFramesEnabled = false;

#if ENABLE(FONT_LOAD_EVENTS)
bool RuntimeEnabledFeatures::isFontLoadEventsEnabled = false;
#endif

#if USE(WEBP)
bool RuntimeEnabledFeatures::isWebPInAcceptHeaderEnabled = false;
#endif

} // namespace WebCore
