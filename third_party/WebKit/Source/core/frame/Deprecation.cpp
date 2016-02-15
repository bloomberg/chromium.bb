// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Deprecation.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerGlobalScope.h"

namespace blink {

Deprecation::Deprecation()
{
    m_cssPropertyDeprecationBits.ensureSize(lastUnresolvedCSSProperty + 1);
}

Deprecation::~Deprecation()
{
}

void Deprecation::clearSuppression()
{
    m_cssPropertyDeprecationBits.clearAll();
}

void Deprecation::suppress(CSSPropertyID unresolvedProperty)
{
    ASSERT(unresolvedProperty >= firstCSSProperty);
    ASSERT(unresolvedProperty <= lastUnresolvedCSSProperty);
    m_cssPropertyDeprecationBits.quickSet(unresolvedProperty);
}
bool Deprecation::isSuppressed(CSSPropertyID unresolvedProperty)
{
    ASSERT(unresolvedProperty >= firstCSSProperty);
    ASSERT(unresolvedProperty <= lastUnresolvedCSSProperty);
    return m_cssPropertyDeprecationBits.quickGet(unresolvedProperty);
}

void Deprecation::warnOnDeprecatedProperties(const LocalFrame* frame, CSSPropertyID unresolvedProperty)
{
    FrameHost* host = frame ? frame->host() : nullptr;
    if (!host || host->deprecation().isSuppressed(unresolvedProperty))
        return;

    String message = deprecationMessage(unresolvedProperty);
    if (!message.isEmpty()) {
        host->deprecation().suppress(unresolvedProperty);
        frame->console().addMessage(ConsoleMessage::create(DeprecationMessageSource, WarningMessageLevel, message));
    }
}

String Deprecation::deprecationMessage(CSSPropertyID unresolvedProperty)
{
    switch (unresolvedProperty) {
    case CSSPropertyWebkitBackgroundComposite:
        return willBeRemoved("'-webkit-background-composite'", 51, "6607299456008192");
    default:
        return emptyString();
    }
}

void Deprecation::countDeprecation(const LocalFrame* frame, UseCounter::Feature feature)
{
    if (!frame)
        return;
    FrameHost* host = frame->host();
    if (!host)
        return;

    if (!host->useCounter().hasRecordedMeasurement(feature)) {
        host->useCounter().recordMeasurement(feature);
        ASSERT(!deprecationMessage(feature).isEmpty());
        frame->console().addMessage(ConsoleMessage::create(DeprecationMessageSource, WarningMessageLevel, deprecationMessage(feature)));
    }
}

void Deprecation::countDeprecation(ExecutionContext* context, UseCounter::Feature feature)
{
    if (!context)
        return;
    if (context->isDocument()) {
        Deprecation::countDeprecation(*toDocument(context), feature);
        return;
    }
    if (context->isWorkerGlobalScope())
        toWorkerGlobalScope(context)->countDeprecation(feature);
}

void Deprecation::countDeprecation(const Document& document, UseCounter::Feature feature)
{
    Deprecation::countDeprecation(document.frame(), feature);
}

void Deprecation::countDeprecationIfNotPrivateScript(v8::Isolate* isolate, ExecutionContext* context, UseCounter::Feature feature)
{
    if (DOMWrapperWorld::current(isolate).isPrivateScriptIsolatedWorld())
        return;
    Deprecation::countDeprecation(context, feature);
}

static const char* milestoneString(int milestone)
{
    switch (milestone) {
    case 50:
        return "M50, around April 2016";
    case 51:
        return "M51, around June 2016";
    case 52:
        return "M52, around August 2016";
    case 53:
        return "M53, around September 2016";
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

static String replacedBy(const char* feature, const char* replacement)
{
    return String::format("%s is deprecated. Please use %s instead.", feature, replacement);
}

String Deprecation::willBeRemoved(const char* feature, int milestone, const char* details)
{
    return String::format("%s is deprecated and will be removed in %s. See https://www.chromestatus.com/features/%s for more details.", feature, milestoneString(milestone), details);
}

static String replacedWillBeRemoved(const char* feature, const char* replacement, int milestone, const char* details)
{
    return String::format("%s is deprecated and will be removed in %s. Please use %s instead. See https://www.chromestatus.com/features/%s for more details.", feature, milestoneString(milestone), replacement, details);
}

String Deprecation::deprecationMessage(UseCounter::Feature feature)
{
    switch (feature) {
    // Quota
    case UseCounter::PrefixedStorageInfo:
        return replacedBy("'window.webkitStorageInfo'", "'navigator.webkitTemporaryStorage' or 'navigator.webkitPersistentStorage'");

    // Keyboard Event (DOM Level 3)
    case UseCounter::KeyboardEventKeyLocation:
        return replacedWillBeRemoved("'KeyboardEvent.keyLocation'", "'KeyboardEvent.location'", 50, "4997403308457984");

    case UseCounter::ConsoleMarkTimeline:
        return replacedBy("'console.markTimeline'", "'console.timeStamp'");

    case UseCounter::FileError:
        return "FileError is deprecated. Please use the 'name' or 'message' attributes of DOMError rather than 'code'.";

    case UseCounter::CSSStyleSheetInsertRuleOptionalArg:
        return "Calling CSSStyleSheet.insertRule() with one argument is deprecated. Please pass the index argument as well: insertRule(x, 0).";

    case UseCounter::PrefixedVideoSupportsFullscreen:
        return replacedBy("'HTMLVideoElement.webkitSupportsFullscreen'", "'Document.fullscreenEnabled'");

    case UseCounter::PrefixedVideoDisplayingFullscreen:
        return replacedBy("'HTMLVideoElement.webkitDisplayingFullscreen'", "'Document.fullscreenElement'");

    case UseCounter::PrefixedVideoEnterFullscreen:
        return replacedBy("'HTMLVideoElement.webkitEnterFullscreen()'", "'Element.requestFullscreen()'");

    case UseCounter::PrefixedVideoExitFullscreen:
        return replacedBy("'HTMLVideoElement.webkitExitFullscreen()'", "'Document.exitFullscreen()'");

    case UseCounter::PrefixedVideoEnterFullScreen:
        return replacedBy("'HTMLVideoElement.webkitEnterFullScreen()'", "'Element.requestFullscreen()'");

    case UseCounter::PrefixedVideoExitFullScreen:
        return replacedBy("'HTMLVideoElement.webkitExitFullScreen()'", "'Document.exitFullscreen()'");

    case UseCounter::PrefixedIndexedDB:
        return replacedBy("'webkitIndexedDB'", "'indexedDB'");

    case UseCounter::PrefixedIDBCursorConstructor:
        return replacedBy("'webkitIDBCursor'", "'IDBCursor'");

    case UseCounter::PrefixedIDBDatabaseConstructor:
        return replacedBy("'webkitIDBDatabase'", "'IDBDatabase'");

    case UseCounter::PrefixedIDBFactoryConstructor:
        return replacedBy("'webkitIDBFactory'", "'IDBFactory'");

    case UseCounter::PrefixedIDBIndexConstructor:
        return replacedBy("'webkitIDBIndex'", "'IDBIndex'");

    case UseCounter::PrefixedIDBKeyRangeConstructor:
        return replacedBy("'webkitIDBKeyRange'", "'IDBKeyRange'");

    case UseCounter::PrefixedIDBObjectStoreConstructor:
        return replacedBy("'webkitIDBObjectStore'", "'IDBObjectStore'");

    case UseCounter::PrefixedIDBRequestConstructor:
        return replacedBy("'webkitIDBRequest'", "'IDBRequest'");

    case UseCounter::PrefixedIDBTransactionConstructor:
        return replacedBy("'webkitIDBTransaction'", "'IDBTransaction'");

    case UseCounter::PrefixedRequestAnimationFrame:
        return "'webkitRequestAnimationFrame' is vendor-specific. Please use the standard 'requestAnimationFrame' instead.";

    case UseCounter::PrefixedCancelAnimationFrame:
        return "'webkitCancelAnimationFrame' is vendor-specific. Please use the standard 'cancelAnimationFrame' instead.";

    case UseCounter::PrefixedCancelRequestAnimationFrame:
        return "'webkitCancelRequestAnimationFrame' is vendor-specific. Please use the standard 'cancelAnimationFrame' instead.";

    case UseCounter::SyncXHRWithCredentials:
        return "Setting 'XMLHttpRequest.withCredentials' for synchronous requests is deprecated.";

    case UseCounter::PictureSourceSrc:
        return "<source src> with a <picture> parent is invalid and therefore ignored. Please use <source srcset> instead.";

    case UseCounter::ConsoleTimeline:
        return replacedBy("'console.timeline'", "'console.time'");

    case UseCounter::ConsoleTimelineEnd:
        return replacedBy("'console.timelineEnd'", "'console.timeEnd'");

    case UseCounter::XMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload:
        return "Synchronous XMLHttpRequest on the main thread is deprecated because of its detrimental effects to the end user's experience. For more help, check https://xhr.spec.whatwg.org/.";

    case UseCounter::GetMatchedCSSRules:
        return "'getMatchedCSSRules()' is deprecated. For more help, check https://code.google.com/p/chromium/issues/detail?id=437569#c2";

    case UseCounter::PrefixedImageSmoothingEnabled:
        return replacedBy("'CanvasRenderingContext2D.webkitImageSmoothingEnabled'", "'CanvasRenderingContext2D.imageSmoothingEnabled'");

    case UseCounter::AudioListenerDopplerFactor:
        return "dopplerFactor is deprecated and will be removed in M45 when all doppler effects are removed";

    case UseCounter::AudioListenerSpeedOfSound:
        return "speedOfSound is deprecated and will be removed in M45 when all doppler effects are removed";

    case UseCounter::AudioListenerSetVelocity:
        return "setVelocity() is deprecated and will be removed in M45 when all doppler effects are removed";

    case UseCounter::PrefixedWindowURL:
        return replacedBy("'webkitURL'", "'URL'");

    case UseCounter::PrefixedAudioContext:
        return replacedBy("'webkitAudioContext'", "'AudioContext'");

    case UseCounter::PrefixedOfflineAudioContext:
        return replacedBy("'webkitOfflineAudioContext'", "'OfflineAudioContext'");

    case UseCounter::RangeExpand:
        return replacedBy("'Range.expand()'", "'Selection.modify()'");

    case UseCounter::PrefixedMediaAddKey:
    case UseCounter::PrefixedMediaGenerateKeyRequest:
    case UseCounter::PrefixedMediaCancelKeyRequest:
        return "The prefixed Encrypted Media Extensions APIs are deprecated. Please use 'navigator.requestMediaKeySystemAccess()' instead.";

    case UseCounter::CanPlayTypeKeySystem:
        return replacedBy("canPlayType()'s 'keySystem' parameter", "'navigator.requestMediaKeySystemAccess()'");

    // Powerful features on insecure origins (https://goo.gl/rStTGz)
    case UseCounter::DeviceMotionInsecureOrigin:
        return "The devicemotion event is deprecated on insecure origins, and support will be removed in the future. You should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::DeviceOrientationInsecureOrigin:
        return "The deviceorientation event is deprecated on insecure origins, and support will be removed in the future. You should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::DeviceOrientationAbsoluteInsecureOrigin:
        return "The deviceorientationabsolute event is deprecated on insecure origins, and support will be removed in the future. You should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::GeolocationInsecureOrigin:
        // TODO(jww): This message should be made less ambigous after WebView
        // is fixed so geolocation can be removed there. After that, this
        // should be updated to read similarly to GetUserMediaInsecureOrigin's
        // message.
        return "getCurrentPosition() and watchPosition() are deprecated on insecure origins. To use this feature, you should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::GetUserMediaInsecureOrigin:
        return "getUserMedia() no longer works on insecure origins. To use this feature, you should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::EncryptedMediaInsecureOrigin:
        return "requestMediaKeySystemAccess() is deprecated on insecure origins in the specification. Support will be removed in the future. You should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::ElementCreateShadowRootMultiple:
        return "Calling Element.createShadowRoot() for an element which already hosts a shadow root is deprecated. See https://www.chromestatus.com/features/4668884095336448 for more details.";

    case UseCounter::ElementCreateShadowRootMultipleWithUserAgentShadowRoot:
        return "Calling Element.createShadowRoot() for an element which already hosts a user-agent shadow root is deprecated. See https://www.chromestatus.com/features/4668884095336448 for more details.";

    case UseCounter::CSSDeepCombinator:
        return "/deep/ combinator is deprecated. See https://www.chromestatus.com/features/6750456638341120 for more details.";

    case UseCounter::CSSSelectorPseudoShadow:
        return "::shadow pseudo-element is deprecated. See https://www.chromestatus.com/features/6750456638341120 for more details.";

    case UseCounter::SVGSMILElementInDocument:
    case UseCounter::SVGSMILAnimationInImageRegardlessOfCache:
        return "SVG's SMIL animations (<animate>, <set>, etc.) are deprecated and will be removed. Please use CSS animations or Web animations instead.";

    case UseCounter::PrefixedPerformanceClearResourceTimings:
        return replacedBy("'Performance.webkitClearResourceTimings'", "'Performance.clearResourceTimings'");

    case UseCounter::PrefixedPerformanceSetResourceTimingBufferSize:
        return replacedBy("'Performance.webkitSetResourceTimingBufferSize'", "'Performance.setResourceTimingBufferSize'");

    case UseCounter::PrefixedPerformanceResourceTimingBufferFull:
        return replacedBy("'Performance.onwebkitresourcetimingbufferfull'", "'Performance.onresourcetimingbufferfull'");

    case UseCounter::BluetoothDeviceInstanceId:
        return replacedBy("'BluetoothDevice.instanceID'", "'BluetoothDevice.id'");

    case UseCounter::BluetoothDeviceConnectGATT:
        return replacedWillBeRemoved("'BluetoothDevice.connectGATT'", "'BluetoothDevice.gatt.connect'", 52, "5264933985976320");

    case UseCounter::V8SVGElement_OffsetParent_AttributeGetter:
        return willBeRemoved("'SVGElement.offsetParent'", 50, "5724912467574784");

    case UseCounter::V8SVGElement_OffsetTop_AttributeGetter:
        return willBeRemoved("'SVGElement.offsetTop'", 50, "5724912467574784");

    case UseCounter::V8SVGElement_OffsetLeft_AttributeGetter:
        return willBeRemoved("'SVGElement.offsetLeft'", 50, "5724912467574784");

    case UseCounter::V8SVGElement_OffsetWidth_AttributeGetter:
        return willBeRemoved("'SVGElement.offsetWidth'", 50, "5724912467574784");

    case UseCounter::V8SVGElement_OffsetHeight_AttributeGetter:
        return willBeRemoved("'SVGElement.offsetHeight'", 50, "5724912467574784");

    case UseCounter::MediaStreamTrackGetSources:
        return "MediaStreamTrack.getSources is deprecated. See https://www.chromestatus.com/feature/4765305641369600 for more details.";

    case UseCounter::DocumentDefaultCharset:
        return willBeRemoved("'Document.defaultCharset'", 50, "6217124578066432");

    case UseCounter::V8TouchEvent_InitTouchEvent_Method:
        return replacedWillBeRemoved("'TouchEvent.initTouchEvent'", "the TouchEvent constructor", 53, "5730982598541312");

    case UseCounter::RTCPeerConnectionCreateAnswerLegacyNoFailureCallback:
        return "RTCPeerConnection.CreateAnswer without a failure callback is deprecated. The failure callback will be a required parameter in M50. See https://www.chromestatus.com/feature/5663288008376320 for more details";

    case UseCounter::RTCPeerConnectionCreateOfferLegacyNoFailureCallback:
        return "RTCPeerConnection.CreateOffer without a failure callback is deprecated. The failure callback will be a required parameter in M50. See https://www.chromestatus.com/feature/5663288008376320 for more details";

    case UseCounter::ObjectObserve:
        return willBeRemoved("'Object.observe'", 50, "6147094632988672");

    // Features that aren't deprecated don't have a deprecation message.
    default:
        return String();
    }
}

} // namespace blink
