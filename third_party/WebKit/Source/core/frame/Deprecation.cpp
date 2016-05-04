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

namespace {

const char* milestoneString(int milestone)
{
    // These are the Estimated Stable Dates:
    // https://www.chromium.org/developers/calendar

    switch (milestone) {
    case 50:
        return "M50, around April 2016";
    case 51:
        return "M51, around May 2016";
    case 52:
        return "M52, around July 2016";
    case 53:
        return "M53, around September 2016";
    case 54:
        return "M54, around October 2016";
    case 55:
        return "M55, around November 2016";
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

String replacedBy(const char* feature, const char* replacement)
{
    return String::format("%s is deprecated. Please use %s instead.", feature, replacement);
}

String willBeRemoved(const char* feature, int milestone, const char* details)
{
    return String::format("%s is deprecated and will be removed in %s. See https://www.chromestatus.com/features/%s for more details.", feature, milestoneString(milestone), details);
}

String dopplerWillBeRemoved(const char* feature, int milestone, const char* details)
{
    return String::format("%s is deprecated and will be removed in %s. It has no effect as the Web Audio doppler effects have already been removed internally. See https://www.chromestatus.com/features/%s for more details.", feature, milestoneString(milestone), details);
}

String replacedWillBeRemoved(const char* feature, const char* replacement, int milestone, const char* details)
{
    return String::format("%s is deprecated and will be removed in %s. Please use %s instead. See https://www.chromestatus.com/features/%s for more details.", feature, milestoneString(milestone), replacement, details);
}

} // anonymous namespace

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
    // TODO: Add a switch here when there are properties that we intend to deprecate.
    // Returning an empty string for now.
    return emptyString();
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

void Deprecation::countDeprecationCrossOriginIframe(const LocalFrame* frame, UseCounter::Feature feature)
{
    // Check to see if the frame can script into the top level document.
    SecurityOrigin* securityOrigin = frame->securityContext()->getSecurityOrigin();
    Frame* top = frame->tree().top();
    if (top && !securityOrigin->canAccess(top->securityContext()->getSecurityOrigin()))
        countDeprecation(frame, feature);
}

void Deprecation::countDeprecationCrossOriginIframe(const Document& document, UseCounter::Feature feature)
{
    LocalFrame* frame = document.frame();
    if (!frame)
        return;
    countDeprecationCrossOriginIframe(frame, feature);
}

String Deprecation::deprecationMessage(UseCounter::Feature feature)
{
    switch (feature) {
    // Quota
    case UseCounter::PrefixedStorageInfo:
        return replacedBy("'window.webkitStorageInfo'", "'navigator.webkitTemporaryStorage' or 'navigator.webkitPersistentStorage'");

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
        return dopplerWillBeRemoved("'AudioListener.dopplerFactor'", 55, "5238926818148352");

    case UseCounter::AudioListenerSpeedOfSound:
        return dopplerWillBeRemoved("'AudioListener.speedOfSound'", 55, "5238926818148352");

    case UseCounter::AudioListenerSetVelocity:
        return dopplerWillBeRemoved("'AudioListener.setVelocity()'", 55, "5238926818148352");

    case UseCounter::PannerNodeSetVelocity:
        return dopplerWillBeRemoved("'PannerNode.setVelocity()'", 55, "5238926818148352");

    case UseCounter::PrefixedWindowURL:
        return replacedBy("'webkitURL'", "'URL'");

    case UseCounter::PrefixedAudioContext:
        return replacedBy("'webkitAudioContext'", "'AudioContext'");

    case UseCounter::PrefixedOfflineAudioContext:
        return replacedBy("'webkitOfflineAudioContext'", "'OfflineAudioContext'");

    case UseCounter::RangeExpand:
        return replacedBy("'Range.expand()'", "'Selection.modify()'");

    // Powerful features on insecure origins (https://goo.gl/rStTGz)
    case UseCounter::DeviceMotionInsecureOrigin:
        return "The devicemotion event is deprecated on insecure origins, and support will be removed in the future. You should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::DeviceOrientationInsecureOrigin:
        return "The deviceorientation event is deprecated on insecure origins, and support will be removed in the future. You should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::DeviceOrientationAbsoluteInsecureOrigin:
        return "The deviceorientationabsolute event is deprecated on insecure origins, and support will be removed in the future. You should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::GeolocationInsecureOrigin:
    case UseCounter::GeolocationInsecureOriginIframe:
        // TODO(jww): This message should be made less ambigous after WebView
        // is fixed so geolocation can be removed there. After that, this
        // should be updated to read similarly to GetUserMediaInsecureOrigin's
        // message.
        return "getCurrentPosition() and watchPosition() are deprecated on insecure origins. To use this feature, you should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::GetUserMediaInsecureOrigin:
    case UseCounter::GetUserMediaInsecureOriginIframe:
        return "getUserMedia() no longer works on insecure origins. To use this feature, you should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::EncryptedMediaInsecureOrigin:
        return "requestMediaKeySystemAccess() is deprecated on insecure origins in the specification. Support will be removed in the future. You should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::ApplicationCacheManifestSelectInsecureOrigin:
    case UseCounter::ApplicationCacheAPIInsecureOrigin:
        return "Use of the Application Cache is deprecated on insecure origins. Support will be removed in the future. You should consider switching your application to a secure origin, such as HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::ElementCreateShadowRootMultiple:
        return "Calling Element.createShadowRoot() for an element which already hosts a shadow root is deprecated. See https://www.chromestatus.com/features/4668884095336448 for more details.";

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

    case UseCounter::MediaStreamTrackGetSources:
        return "MediaStreamTrack.getSources is deprecated. See https://www.chromestatus.com/feature/4765305641369600 for more details.";

    case UseCounter::V8TouchEvent_InitTouchEvent_Method:
        return replacedWillBeRemoved("'TouchEvent.initTouchEvent'", "the TouchEvent constructor", 53, "5730982598541312");

    case UseCounter::ObjectObserve:
        return willBeRemoved("'Object.observe'", 50, "6147094632988672");

    case UseCounter::SVGZoomEvent:
        return willBeRemoved("'SVGZoomEvent'", 52, "5760883808534528");

    case UseCounter::WebAnimationHyphenatedProperty:
        return "Hyphenated property names in Web Animations keyframes are invalid and therefore ignored. Please use camelCase instead.";

    case UseCounter::HTMLKeygenElement:
        return willBeRemoved("The <keygen> element", 54, "5716060992962560");

    case UseCounter::ResultsAttribute:
        return willBeRemoved("'results' attribute", 53, "5738199536107520");

    case UseCounter::TouchDragUserGestureUsedCrossOrigin:
        return willBeRemoved("Performing sensitive operations in iframes on touch events which don't represent a tap gesture", 52, "5649871251963904");

    case UseCounter::WebAnimationsEasingAsFunctionLinear:
        return String::format("Specifying animation easing as a function is deprecated and all support will be removed in %s, at which point this will throw a TypeError. This warning may have been triggered by the Web Animations or Polymer polyfills. See http://crbug.com/601672 for details.", milestoneString(54));

    case UseCounter::WindowPostMessageWithLegacyTargetOriginArgument:
        return replacedWillBeRemoved("'window.postMessage(message, transferables, targetOrigin)'", "'window.postMessage(message, targetOrigin, transferables)'", 54, "5719033043222528");

    case UseCounter::EncryptedMediaAllSelectedContentTypesMissingCodecs:
        return "contentType strings without codecs will not be supported by requestMediaKeySystemAccess() in the future. Please specify the desired codec(s) as part of the contentType.";

    case UseCounter::V8KeyboardEvent_KeyIdentifier_AttributeGetter:
        return willBeRemoved("'KeyboardEvent.keyIdentifier'", 53, "5316065118650368");

    case UseCounter::During_Microtask_Alert:
        return willBeRemoved("Invoking 'alert()' during microtask execution", 53, "5647113010544640");

    case UseCounter::During_Microtask_Confirm:
        return willBeRemoved("Invoking 'confirm()' during microtask execution", 53, "5647113010544640");

    case UseCounter::During_Microtask_Print:
        return willBeRemoved("Invoking 'print()' during microtask execution", 53, "5647113010544640");

    case UseCounter::During_Microtask_Prompt:
        return willBeRemoved("Invoking 'prompt()' during microtask execution", 53, "5647113010544640");

    case UseCounter::During_Microtask_SyncXHR:
        return willBeRemoved("Invoking 'send()' on a sync XHR during microtask execution", 53, "5647113010544640");

    // Features that aren't deprecated don't have a deprecation message.
    default:
        return String();
    }
}

} // namespace blink
