/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

// Use this file to assert that various public API enum values continue
// matching blink defined enum values.

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "build/build_config.h"
#include "core/dom/IconURL.h"
#include "core/editing/SelectionType.h"
#if defined(OS_MACOSX)
#include "core/events/WheelEvent.h"
#endif
#include "core/fileapi/FileError.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/CSPSource.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/forms/TextControlInnerElements.h"
#include "core/html/media/AutoplayPolicy.h"
#include "core/layout/compositing/CompositedSelectionBound.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/NavigationPolicy.h"
#include "core/page/PageVisibilityState.h"
#include "core/style/ComputedStyleConstants.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IndexedDB.h"
#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "modules/quota/DeprecatedStorageQuota.h"
#include "modules/speech/SpeechRecognitionError.h"
#include "platform/FileMetadata.h"
#include "platform/FileSystemType.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSmoothingMode.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/text/TextChecking.h"
#include "platform/text/TextDecoration.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/StringImpl.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebFileInfo.h"
#include "public/platform/WebFileSystem.h"
#include "public/platform/WebHistoryScrollRestorationType.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebMediaPlayerClient.h"
#include "public/platform/WebMediaSource.h"
#include "public/platform/WebMouseWheelEvent.h"
#include "public/platform/WebScrollBoundaryBehavior.h"
#include "public/platform/WebScrollbar.h"
#include "public/platform/WebScrollbarBehavior.h"
#include "public/platform/WebSelectionBound.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBDatabaseException.h"
#include "public/platform/modules/indexeddb/WebIDBFactory.h"
#include "public/platform/modules/indexeddb/WebIDBKey.h"
#include "public/platform/modules/indexeddb/WebIDBKeyPath.h"
#include "public/platform/modules/indexeddb/WebIDBMetadata.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebFrameLoadType.h"
#include "public/web/WebHistoryCommitType.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebInputElement.h"
#include "public/web/WebNavigatorContentUtilsClient.h"
#include "public/web/WebRemoteFrameClient.h"
#include "public/web/WebSandboxFlags.h"
#include "public/web/WebSecurityPolicy.h"
#include "public/web/WebSelection.h"
#include "public/web/WebSerializedScriptValueVersion.h"
#include "public/web/WebSettings.h"
#include "public/web/WebTextCheckingResult.h"
#include "public/web/WebTextDecorationType.h"
#include "public/web/WebView.h"

namespace blink {

STATIC_ASSERT_ENUM(kWebIDBDatabaseExceptionUnknownError, kUnknownError);
STATIC_ASSERT_ENUM(kWebIDBDatabaseExceptionConstraintError, kConstraintError);
STATIC_ASSERT_ENUM(kWebIDBDatabaseExceptionDataError, kDataError);
STATIC_ASSERT_ENUM(kWebIDBDatabaseExceptionVersionError, kVersionError);
STATIC_ASSERT_ENUM(kWebIDBDatabaseExceptionAbortError, kAbortError);
STATIC_ASSERT_ENUM(kWebIDBDatabaseExceptionQuotaError, kQuotaExceededError);
STATIC_ASSERT_ENUM(kWebIDBDatabaseExceptionTimeoutError, kTimeoutError);

STATIC_ASSERT_ENUM(kWebIDBKeyTypeInvalid, IDBKey::kInvalidType);
STATIC_ASSERT_ENUM(kWebIDBKeyTypeArray, IDBKey::kArrayType);
STATIC_ASSERT_ENUM(kWebIDBKeyTypeBinary, IDBKey::kBinaryType);
STATIC_ASSERT_ENUM(kWebIDBKeyTypeString, IDBKey::kStringType);
STATIC_ASSERT_ENUM(kWebIDBKeyTypeDate, IDBKey::kDateType);
STATIC_ASSERT_ENUM(kWebIDBKeyTypeNumber, IDBKey::kNumberType);

STATIC_ASSERT_ENUM(kWebIDBKeyPathTypeNull, IDBKeyPath::kNullType);
STATIC_ASSERT_ENUM(kWebIDBKeyPathTypeString, IDBKeyPath::kStringType);
STATIC_ASSERT_ENUM(kWebIDBKeyPathTypeArray, IDBKeyPath::kArrayType);

STATIC_ASSERT_ENUM(WebIDBMetadata::kNoVersion, IDBDatabaseMetadata::kNoVersion);


STATIC_ASSERT_ENUM(kWebTextDecorationTypeSpelling, kTextDecorationTypeSpelling);
STATIC_ASSERT_ENUM(kWebTextDecorationTypeGrammar, kTextDecorationTypeGrammar);

STATIC_ASSERT_ENUM(kWebStandardCommit, kStandardCommit);
STATIC_ASSERT_ENUM(kWebBackForwardCommit, kBackForwardCommit);
STATIC_ASSERT_ENUM(kWebInitialCommitInChildFrame, kInitialCommitInChildFrame);
STATIC_ASSERT_ENUM(kWebHistoryInertCommit, kHistoryInertCommit);

STATIC_ASSERT_ENUM(kWebHistorySameDocumentLoad, kHistorySameDocumentLoad);
STATIC_ASSERT_ENUM(kWebHistoryDifferentDocumentLoad,
                   kHistoryDifferentDocumentLoad);

STATIC_ASSERT_ENUM(kWebHistoryScrollRestorationManual,
                   kScrollRestorationManual);
STATIC_ASSERT_ENUM(kWebHistoryScrollRestorationAuto, kScrollRestorationAuto);

STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelVerbose, kVerboseMessageLevel);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelInfo, kInfoMessageLevel);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelWarning, kWarningMessageLevel);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelError, kErrorMessageLevel);

STATIC_ASSERT_ENUM(kWebCustomHandlersNew,
                   NavigatorContentUtilsClient::kCustomHandlersNew);
STATIC_ASSERT_ENUM(kWebCustomHandlersRegistered,
                   NavigatorContentUtilsClient::kCustomHandlersRegistered);
STATIC_ASSERT_ENUM(kWebCustomHandlersDeclined,
                   NavigatorContentUtilsClient::kCustomHandlersDeclined);

STATIC_ASSERT_ENUM(WebSelection::kNoSelection, kNoSelection);
STATIC_ASSERT_ENUM(WebSelection::kCaretSelection, kCaretSelection);
STATIC_ASSERT_ENUM(WebSelection::kRangeSelection, kRangeSelection);

STATIC_ASSERT_ENUM(WebSettings::kImageAnimationPolicyAllowed,
                   kImageAnimationPolicyAllowed);
STATIC_ASSERT_ENUM(WebSettings::kImageAnimationPolicyAnimateOnce,
                   kImageAnimationPolicyAnimateOnce);
STATIC_ASSERT_ENUM(WebSettings::kImageAnimationPolicyNoAnimation,
                   kImageAnimationPolicyNoAnimation);

STATIC_ASSERT_ENUM(WebSettings::kV8CacheOptionsDefault, kV8CacheOptionsDefault);
STATIC_ASSERT_ENUM(WebSettings::kV8CacheOptionsNone, kV8CacheOptionsNone);
STATIC_ASSERT_ENUM(WebSettings::kV8CacheOptionsParse, kV8CacheOptionsParse);
STATIC_ASSERT_ENUM(WebSettings::kV8CacheOptionsCode, kV8CacheOptionsCode);

STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::kDefault,
                   V8CacheStrategiesForCacheStorage::kDefault);
STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::kNone,
                   V8CacheStrategiesForCacheStorage::kNone);
STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::kNormal,
                   V8CacheStrategiesForCacheStorage::kNormal);
STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::kAggressive,
                   V8CacheStrategiesForCacheStorage::kAggressive);

STATIC_ASSERT_ENUM(WebSandboxFlags::kNone, kSandboxNone);
STATIC_ASSERT_ENUM(WebSandboxFlags::kNavigation, kSandboxNavigation);
STATIC_ASSERT_ENUM(WebSandboxFlags::kPlugins, kSandboxPlugins);
STATIC_ASSERT_ENUM(WebSandboxFlags::kOrigin, kSandboxOrigin);
STATIC_ASSERT_ENUM(WebSandboxFlags::kForms, kSandboxForms);
STATIC_ASSERT_ENUM(WebSandboxFlags::kScripts, kSandboxScripts);
STATIC_ASSERT_ENUM(WebSandboxFlags::kTopNavigation, kSandboxTopNavigation);
STATIC_ASSERT_ENUM(WebSandboxFlags::kPopups, kSandboxPopups);
STATIC_ASSERT_ENUM(WebSandboxFlags::kAutomaticFeatures,
                   kSandboxAutomaticFeatures);
STATIC_ASSERT_ENUM(WebSandboxFlags::kPointerLock, kSandboxPointerLock);
STATIC_ASSERT_ENUM(WebSandboxFlags::kDocumentDomain, kSandboxDocumentDomain);
STATIC_ASSERT_ENUM(WebSandboxFlags::kOrientationLock, kSandboxOrientationLock);
STATIC_ASSERT_ENUM(WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts,
                   kSandboxPropagatesToAuxiliaryBrowsingContexts);
STATIC_ASSERT_ENUM(WebSandboxFlags::kModals, kSandboxModals);

STATIC_ASSERT_ENUM(WebFrameLoadType::kStandard, kFrameLoadTypeStandard);
STATIC_ASSERT_ENUM(WebFrameLoadType::kBackForward, kFrameLoadTypeBackForward);
STATIC_ASSERT_ENUM(WebFrameLoadType::kReload, kFrameLoadTypeReload);
STATIC_ASSERT_ENUM(WebFrameLoadType::kReplaceCurrentItem,
                   kFrameLoadTypeReplaceCurrentItem);
STATIC_ASSERT_ENUM(WebFrameLoadType::kInitialInChildFrame,
                   kFrameLoadTypeInitialInChildFrame);
STATIC_ASSERT_ENUM(WebFrameLoadType::kInitialHistoryLoad,
                   kFrameLoadTypeInitialHistoryLoad);
STATIC_ASSERT_ENUM(WebFrameLoadType::kReloadBypassingCache,
                   kFrameLoadTypeReloadBypassingCache);

STATIC_ASSERT_ENUM(FrameDetachType::kRemove,
                   WebFrameClient::DetachType::kRemove);
STATIC_ASSERT_ENUM(FrameDetachType::kSwap, WebFrameClient::DetachType::kSwap);
STATIC_ASSERT_ENUM(FrameDetachType::kRemove,
                   WebRemoteFrameClient::DetachType::kRemove);
STATIC_ASSERT_ENUM(FrameDetachType::kSwap,
                   WebRemoteFrameClient::DetachType::kSwap);

STATIC_ASSERT_ENUM(WebSettings::ProgressBarCompletion::kLoadEvent,
                   ProgressBarCompletion::kLoadEvent);
STATIC_ASSERT_ENUM(WebSettings::ProgressBarCompletion::kResourcesBeforeDCL,
                   ProgressBarCompletion::kResourcesBeforeDCL);
STATIC_ASSERT_ENUM(WebSettings::ProgressBarCompletion::kDOMContentLoaded,
                   ProgressBarCompletion::kDOMContentLoaded);
STATIC_ASSERT_ENUM(
    WebSettings::ProgressBarCompletion::kResourcesBeforeDCLAndSameOriginIFrames,
    ProgressBarCompletion::kResourcesBeforeDCLAndSameOriginIFrames);

STATIC_ASSERT_ENUM(WebSettings::AutoplayPolicy::kNoUserGestureRequired,
                   AutoplayPolicy::Type::kNoUserGestureRequired);
STATIC_ASSERT_ENUM(WebSettings::AutoplayPolicy::kUserGestureRequired,
                   AutoplayPolicy::Type::kUserGestureRequired);
STATIC_ASSERT_ENUM(
    WebSettings::AutoplayPolicy::kUserGestureRequiredForCrossOrigin,
    AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin);
STATIC_ASSERT_ENUM(WebSettings::AutoplayPolicy::kDocumentUserActivationRequired,
                   AutoplayPolicy::Type::kDocumentUserActivationRequired);

STATIC_ASSERT_ENUM(WebScrollBoundaryBehavior::kScrollBoundaryBehaviorTypeAuto,
                   EScrollBoundaryBehavior::kAuto);
STATIC_ASSERT_ENUM(
    WebScrollBoundaryBehavior::kScrollBoundaryBehaviorTypeContain,
    EScrollBoundaryBehavior::kContain);
STATIC_ASSERT_ENUM(WebScrollBoundaryBehavior::kScrollBoundaryBehaviorTypeNone,
                   EScrollBoundaryBehavior::kNone);

// This ensures that the version number published in
// WebSerializedScriptValueVersion.h matches the serializer's understanding.
// TODO(jbroman): Fix this to also account for the V8-side version. See
// https://crbug.com/704293.
static_assert(kSerializedScriptValueVersion ==
                  SerializedScriptValue::kWireFormatVersion,
              "Update WebSerializedScriptValueVersion.h.");

}  // namespace blink
