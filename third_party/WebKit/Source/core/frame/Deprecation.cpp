// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Deprecation.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/page/Page.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"

namespace {

enum Milestone {
  M56,
  M57,
  M58,
  M59,
  M60,
};

const char* milestoneString(Milestone milestone) {
  // These are the Estimated Stable Dates:
  // https://www.chromium.org/developers/calendar

  switch (milestone) {
    case M56:
      return "M56, around January 2017";
    case M57:
      return "M57, around March 2017";
    case M58:
      return "M58, around April 2017";
    case M59:
      return "M59, around June 2017";
    case M60:
      return "M60, around August 2017";
  }

  ASSERT_NOT_REACHED();
  return nullptr;
}

String replacedBy(const char* feature, const char* replacement) {
  return String::format("%s is deprecated. Please use %s instead.", feature,
                        replacement);
}

String willBeRemoved(const char* feature,
                     Milestone milestone,
                     const char* details) {
  return String::format(
      "%s is deprecated and will be removed in %s. See "
      "https://www.chromestatus.com/features/%s for more details.",
      feature, milestoneString(milestone), details);
}

String replacedWillBeRemoved(const char* feature,
                             const char* replacement,
                             Milestone milestone,
                             const char* details) {
  return String::format(
      "%s is deprecated and will be removed in %s. Please use %s instead. See "
      "https://www.chromestatus.com/features/%s for more details.",
      feature, milestoneString(milestone), replacement, details);
}

}  // anonymous namespace

namespace blink {

Deprecation::Deprecation() : m_muteCount(0) {
  m_cssPropertyDeprecationBits.ensureSize(lastUnresolvedCSSProperty + 1);
}

Deprecation::~Deprecation() {}

void Deprecation::clearSuppression() {
  m_cssPropertyDeprecationBits.clearAll();
}

void Deprecation::muteForInspector() {
  m_muteCount++;
}

void Deprecation::unmuteForInspector() {
  m_muteCount--;
}

void Deprecation::suppress(CSSPropertyID unresolvedProperty) {
  DCHECK(isCSSPropertyIDWithName(unresolvedProperty));
  m_cssPropertyDeprecationBits.quickSet(unresolvedProperty);
}

bool Deprecation::isSuppressed(CSSPropertyID unresolvedProperty) {
  DCHECK(isCSSPropertyIDWithName(unresolvedProperty));
  return m_cssPropertyDeprecationBits.quickGet(unresolvedProperty);
}

void Deprecation::warnOnDeprecatedProperties(const LocalFrame* frame,
                                             CSSPropertyID unresolvedProperty) {
  Page* page = frame ? frame->page() : nullptr;
  if (!page || page->deprecation().m_muteCount ||
      page->deprecation().isSuppressed(unresolvedProperty))
    return;

  String message = deprecationMessage(unresolvedProperty);
  if (!message.isEmpty()) {
    page->deprecation().suppress(unresolvedProperty);
    ConsoleMessage* consoleMessage = ConsoleMessage::create(
        DeprecationMessageSource, WarningMessageLevel, message);
    frame->console().addMessage(consoleMessage);
  }
}

String Deprecation::deprecationMessage(CSSPropertyID unresolvedProperty) {
  switch (unresolvedProperty) {
    case CSSPropertyAliasMotionOffset:
      return replacedWillBeRemoved("motion-offset", "offset-distance", M58,
                                   "6390764217040896");
    case CSSPropertyAliasMotionRotation:
      return replacedWillBeRemoved("motion-rotation", "offset-rotate", M58,
                                   "6390764217040896");
    case CSSPropertyAliasMotionPath:
      return replacedWillBeRemoved("motion-path", "offset-path", M58,
                                   "6390764217040896");
    case CSSPropertyMotion:
      return replacedWillBeRemoved("motion", "offset", M58, "6390764217040896");
    case CSSPropertyOffsetRotation:
      return replacedWillBeRemoved("offset-rotation", "offset-rotate", M58,
                                   "6390764217040896");

    default:
      return emptyString;
  }
}

void Deprecation::countDeprecation(const LocalFrame* frame,
                                   UseCounter::Feature feature) {
  if (!frame)
    return;
  Page* page = frame->page();
  if (!page || page->deprecation().m_muteCount)
    return;

  if (!page->useCounter().hasRecordedMeasurement(feature)) {
    page->useCounter().recordMeasurement(feature);
    ASSERT(!deprecationMessage(feature).isEmpty());
    ConsoleMessage* consoleMessage =
        ConsoleMessage::create(DeprecationMessageSource, WarningMessageLevel,
                               deprecationMessage(feature));
    frame->console().addMessage(consoleMessage);
  }
}

void Deprecation::countDeprecation(ExecutionContext* context,
                                   UseCounter::Feature feature) {
  if (!context)
    return;
  if (context->isDocument()) {
    Deprecation::countDeprecation(*toDocument(context), feature);
    return;
  }
  if (context->isWorkerOrWorkletGlobalScope())
    toWorkerOrWorkletGlobalScope(context)->countDeprecation(feature);
}

void Deprecation::countDeprecation(const Document& document,
                                   UseCounter::Feature feature) {
  Deprecation::countDeprecation(document.frame(), feature);
}

void Deprecation::countDeprecationCrossOriginIframe(
    const LocalFrame* frame,
    UseCounter::Feature feature) {
  // Check to see if the frame can script into the top level document.
  SecurityOrigin* securityOrigin =
      frame->securityContext()->getSecurityOrigin();
  Frame* top = frame->tree().top();
  if (top &&
      !securityOrigin->canAccess(top->securityContext()->getSecurityOrigin()))
    countDeprecation(frame, feature);
}

void Deprecation::countDeprecationCrossOriginIframe(
    const Document& document,
    UseCounter::Feature feature) {
  LocalFrame* frame = document.frame();
  if (!frame)
    return;
  countDeprecationCrossOriginIframe(frame, feature);
}

String Deprecation::deprecationMessage(UseCounter::Feature feature) {
  switch (feature) {
    // Quota
    case UseCounter::PrefixedStorageInfo:
      return replacedBy("'window.webkitStorageInfo'",
                        "'navigator.webkitTemporaryStorage' or "
                        "'navigator.webkitPersistentStorage'");

    case UseCounter::ConsoleMarkTimeline:
      return replacedBy("'console.markTimeline'", "'console.timeStamp'");

    case UseCounter::CSSStyleSheetInsertRuleOptionalArg:
      return "Calling CSSStyleSheet.insertRule() with one argument is "
             "deprecated. Please pass the index argument as well: "
             "insertRule(x, 0).";

    case UseCounter::PrefixedVideoSupportsFullscreen:
      return replacedBy("'HTMLVideoElement.webkitSupportsFullscreen'",
                        "'Document.fullscreenEnabled'");

    case UseCounter::PrefixedVideoDisplayingFullscreen:
      return replacedBy("'HTMLVideoElement.webkitDisplayingFullscreen'",
                        "'Document.fullscreenElement'");

    case UseCounter::PrefixedVideoEnterFullscreen:
      return replacedBy("'HTMLVideoElement.webkitEnterFullscreen()'",
                        "'Element.requestFullscreen()'");

    case UseCounter::PrefixedVideoExitFullscreen:
      return replacedBy("'HTMLVideoElement.webkitExitFullscreen()'",
                        "'Document.exitFullscreen()'");

    case UseCounter::PrefixedVideoEnterFullScreen:
      return replacedBy("'HTMLVideoElement.webkitEnterFullScreen()'",
                        "'Element.requestFullscreen()'");

    case UseCounter::PrefixedVideoExitFullScreen:
      return replacedBy("'HTMLVideoElement.webkitExitFullScreen()'",
                        "'Document.exitFullscreen()'");

    case UseCounter::PrefixedRequestAnimationFrame:
      return "'webkitRequestAnimationFrame' is vendor-specific. Please use the "
             "standard 'requestAnimationFrame' instead.";

    case UseCounter::PrefixedCancelAnimationFrame:
      return "'webkitCancelAnimationFrame' is vendor-specific. Please use the "
             "standard 'cancelAnimationFrame' instead.";

    case UseCounter::PictureSourceSrc:
      return "<source src> with a <picture> parent is invalid and therefore "
             "ignored. Please use <source srcset> instead.";

    case UseCounter::ConsoleTimeline:
      return replacedBy("'console.timeline'", "'console.time'");

    case UseCounter::ConsoleTimelineEnd:
      return replacedBy("'console.timelineEnd'", "'console.timeEnd'");

    case UseCounter::XMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload:
      return "Synchronous XMLHttpRequest on the main thread is deprecated "
             "because of its detrimental effects to the end user's experience. "
             "For more help, check https://xhr.spec.whatwg.org/.";

    case UseCounter::GetMatchedCSSRules:
      return "'getMatchedCSSRules()' is deprecated. For more help, check "
             "https://code.google.com/p/chromium/issues/detail?id=437569#c2";

    case UseCounter::PrefixedWindowURL:
      return replacedBy("'webkitURL'", "'URL'");

    case UseCounter::RangeExpand:
      return replacedBy("'Range.expand()'", "'Selection.modify()'");

    // Blocked subresource requests:
    case UseCounter::LegacyProtocolEmbeddedAsSubresource:
      return String::format(
          "Subresource requests using legacy protocols (like `ftp:`) are "
          "deprecated, and will be blocked in %s. Please deliver "
          "web-accessible resources over modern protocols like HTTPS. "
          "See https://www.chromestatus.com/feature/5709390967472128 for more "
          "details.",
          milestoneString(M59));

    case UseCounter::RequestedSubresourceWithEmbeddedCredentials:
      return String::format(
          "Subresource requests whose URLs contain embedded credentials (e.g. "
          "`https://user:pass@host/`) are deprecated, and will be blocked in "
          "%s. See https://www.chromestatus.com/feature/5669008342777856 for "
          "more details.",
          milestoneString(M59));

    // Powerful features on insecure origins (https://goo.gl/rStTGz)
    case UseCounter::DeviceMotionInsecureOrigin:
      return "The devicemotion event is deprecated on insecure origins, and "
             "support will be removed in the future. You should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case UseCounter::DeviceOrientationInsecureOrigin:
      return "The deviceorientation event is deprecated on insecure origins, "
             "and support will be removed in the future. You should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case UseCounter::DeviceOrientationAbsoluteInsecureOrigin:
      return "The deviceorientationabsolute event is deprecated on insecure "
             "origins, and support will be removed in the future. You should "
             "consider switching your application to a secure origin, such as "
             "HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::GeolocationInsecureOrigin:
    case UseCounter::GeolocationInsecureOriginIframe:
      return "getCurrentPosition() and watchPosition() no longer work on "
             "insecure origins. To use this feature, you should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case UseCounter::GeolocationInsecureOriginDeprecatedNotRemoved:
    case UseCounter::GeolocationInsecureOriginIframeDeprecatedNotRemoved:
      return "getCurrentPosition() and watchPosition() are deprecated on "
             "insecure origins. To use this feature, you should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case UseCounter::GetUserMediaInsecureOrigin:
    case UseCounter::GetUserMediaInsecureOriginIframe:
      return "getUserMedia() no longer works on insecure origins. To use this "
             "feature, you should consider switching your application to a "
             "secure origin, such as HTTPS. See https://goo.gl/rStTGz for more "
             "details.";

    case UseCounter::MediaSourceAbortRemove:
      return "Using SourceBuffer.abort() to abort remove()'s asynchronous "
             "range removal is deprecated due to specification change. Support "
             "will be removed in the future. You should instead await "
             "'updateend'. abort() is intended to only abort an asynchronous "
             "media append or reset parser state. See "
             "https://www.chromestatus.com/features/6107495151960064 for more "
             "details.";
    case UseCounter::MediaSourceDurationTruncatingBuffered:
      return "Setting MediaSource.duration below the highest presentation "
             "timestamp of any buffered coded frames is deprecated due to "
             "specification change. Support for implicit removal of truncated "
             "buffered media will be removed in the future. You should instead "
             "perform explicit remove(newDuration, oldDuration) on all "
             "sourceBuffers, where newDuration < oldDuration. See "
             "https://www.chromestatus.com/features/6107495151960064 for more "
             "details.";

    case UseCounter::ApplicationCacheManifestSelectInsecureOrigin:
    case UseCounter::ApplicationCacheAPIInsecureOrigin:
      return "Use of the Application Cache is deprecated on insecure origins. "
             "Support will be removed in the future. You should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case UseCounter::NotificationInsecureOrigin:
    case UseCounter::NotificationAPIInsecureOriginIframe:
    case UseCounter::NotificationPermissionRequestedInsecureOrigin:
      return String::format(
          "Using the Notification API on insecure origins is "
          "deprecated and will be removed in %s. You should consider "
          "switching your application to a secure origin, such as HTTPS. See "
          "https://goo.gl/rStTGz for more details.",
          milestoneString(M60));

    case UseCounter::ElementCreateShadowRootMultiple:
      return "Calling Element.createShadowRoot() for an element which already "
             "hosts a shadow root is deprecated. See "
             "https://www.chromestatus.com/features/4668884095336448 for more "
             "details.";

    case UseCounter::CSSDeepCombinator:
      return "/deep/ combinator is deprecated. See "
             "https://www.chromestatus.com/features/6750456638341120 for more "
             "details.";

    case UseCounter::CSSSelectorPseudoShadow:
      return "::shadow pseudo-element is deprecated. See "
             "https://www.chromestatus.com/features/6750456638341120 for more "
             "details.";

    case UseCounter::VRDeprecatedFieldOfView:
      return replacedBy("VREyeParameters.fieldOfView",
                        "projection matrices provided by VRFrameData");

    case UseCounter::VRDeprecatedGetPose:
      return replacedBy("VRDisplay.getPose()", "VRDisplay.getFrameData()");

    case UseCounter::
        ServiceWorkerRespondToNavigationRequestWithRedirectedResponse:
      return String::format(
          "The service worker responded to the navigation request with a "
          "redirected response. This will result in an error in %s.",
          milestoneString(M59));

    case UseCounter::CSSSelectorInternalMediaControlsCastButton:
      return willBeRemoved("-internal-media-controls-cast-button selector", M59,
                           "5734009183141888");

    case UseCounter::CSSSelectorInternalMediaControlsOverlayCastButton:
      return willBeRemoved(
          "-internal-media-controls-overlay-cast-button selector", M59,
          "5714245488476160");

    case UseCounter::CSSSelectorInternalMediaControlsTextTrackList:
    case UseCounter::CSSSelectorInternalMediaControlsTextTrackListItem:
    case UseCounter::CSSSelectorInternalMediaControlsTextTrackListItemInput:
    case UseCounter::CSSSelectorInternalMediaControlsTextTrackListKindCaptions:
    case UseCounter::CSSSelectorInternalMediaControlsTextTrackListKindSubtitles:
      return willBeRemoved(
          "-internal-media-controls-text-track-list* selectors", M59,
          "5661431349379072");

    case UseCounter::FileReaderSyncInServiceWorker:
      return willBeRemoved("FileReaderSync in service workers", M59,
                           "5739144722513920");

    case UseCounter::CSSZoomReset:
      return willBeRemoved("\"zoom: reset\"", M59, "4997605029314560");

    case UseCounter::CSSZoomDocument:
      return willBeRemoved("\"zoom: document\"", M59, "4997605029314560");

    case UseCounter::SelectionAddRangeIntersect:
      return "The behavior that Selection.addRange() merges existing Range and "
             "the specified Range was removed. See "
             "https://www.chromestatus.com/features/6680566019653632 for more "
             "details.";

    case UseCounter::SubtleCryptoOnlyStrictSecureContextCheckFailed:
      return String::format(
          "Web Crypto API usage inside secure frames with non-secure ancestors "
          "is deprecated. The API will no longer be exposed in these contexts "
          "as of %s. See https://www.chromestatus.com/features/5030265697075200"
          " for more details.", milestoneString(M59));

    case UseCounter::RtcpMuxPolicyNegotiate:
      return String::format(
          "The rtcpMuxPolicy option is being considered for "
          "removal and may be removed no earlier than %s. If you depend on it, "
          "please see https://www.chromestatus.com/features/5654810086866944 "
          "for more details.",
          milestoneString(M60));

    case UseCounter::V8IDBFactory_WebkitGetDatabaseNames_Method:
      return willBeRemoved("indexedDB.webkitGetDatabaseNames()", M60,
                           "5725741740195840");

    // Features that aren't deprecated don't have a deprecation message.
    default:
      return String();
  }
}

}  // namespace blink
