// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Deprecation.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/DeprecationReport.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Report.h"
#include "core/frame/ReportingContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/page/Page.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebFeaturePolicyFeature.h"

namespace {

enum Milestone {
  M60,
  M61,
  M62,
  M63,
  M64,
  M65,
};

const char* milestoneString(Milestone milestone) {
  // These are the Estimated Stable Dates:
  // https://www.chromium.org/developers/calendar

  switch (milestone) {
    case M60:
      return "M60, around August 2017";
    case M61:
      return "M61, around September 2017";
    case M62:
      return "M62, around October 2017";
    case M63:
      return "M63, around December 2017";
    case M64:
      return "M64, around January 2018";
    case M65:
      return "M65, around March 2018";
  }

  NOTREACHED();
  return nullptr;
}

String replacedBy(const char* feature, const char* replacement) {
  return String::Format("%s is deprecated. Please use %s instead.", feature,
                        replacement);
}

String willBeRemoved(const char* feature,
                     Milestone milestone,
                     const char* details) {
  return String::Format(
      "%s is deprecated and will be removed in %s. See "
      "https://www.chromestatus.com/features/%s for more details.",
      feature, milestoneString(milestone), details);
}

String replacedWillBeRemoved(const char* feature,
                             const char* replacement,
                             Milestone milestone,
                             const char* details) {
  return String::Format(
      "%s is deprecated and will be removed in %s. Please use %s instead. See "
      "https://www.chromestatus.com/features/%s for more details.",
      feature, milestoneString(milestone), replacement, details);
}

String DeprecatedWillBeDisabledByFeaturePolicyInCrossOriginIframe(
    const char* function,
    const char* allow_string,
    Milestone milestone) {
  return String::Format(
      "%s usage in cross-origin iframes is deprecated and will be disabled in "
      "%s. To continue to use this feature, it must be enabled by the "
      "embedding document using Feature Policy, e.g. "
      "<iframe allow=\"%s\" ...>. See https://goo.gl/EuHzyv for more details.",
      function, milestoneString(milestone), allow_string);
}

}  // anonymous namespace

namespace blink {

Deprecation::Deprecation() : mute_count_(0) {
  css_property_deprecation_bits_.EnsureSize(numCSSPropertyIDs);
}

Deprecation::~Deprecation() {}

void Deprecation::ClearSuppression() {
  css_property_deprecation_bits_.ClearAll();
}

void Deprecation::MuteForInspector() {
  mute_count_++;
}

void Deprecation::UnmuteForInspector() {
  mute_count_--;
}

void Deprecation::Suppress(CSSPropertyID unresolved_property) {
  DCHECK(isCSSPropertyIDWithName(unresolved_property));
  css_property_deprecation_bits_.QuickSet(unresolved_property);
}

bool Deprecation::IsSuppressed(CSSPropertyID unresolved_property) {
  DCHECK(isCSSPropertyIDWithName(unresolved_property));
  return css_property_deprecation_bits_.QuickGet(unresolved_property);
}

void Deprecation::WarnOnDeprecatedProperties(
    const LocalFrame* frame,
    CSSPropertyID unresolved_property) {
  Page* page = frame ? frame->GetPage() : nullptr;
  if (!page || page->GetDeprecation().mute_count_ ||
      page->GetDeprecation().IsSuppressed(unresolved_property))
    return;

  String message = DeprecationMessage(unresolved_property);
  if (!message.IsEmpty()) {
    page->GetDeprecation().Suppress(unresolved_property);
    ConsoleMessage* console_message = ConsoleMessage::Create(
        kDeprecationMessageSource, kWarningMessageLevel, message);
    frame->Console().AddMessage(console_message);
  }
}

String Deprecation::DeprecationMessage(CSSPropertyID unresolved_property) {
  // TODO: Add a switch here when there are properties that we intend to
  // deprecate.
  // Returning an empty string for now.
  return g_empty_string;
}

void Deprecation::CountDeprecation(const LocalFrame* frame,
                                   WebFeature feature) {
  if (!frame)
    return;
  Page* page = frame->GetPage();
  if (!page || page->GetDeprecation().mute_count_)
    return;

  if (!page->GetUseCounter().HasRecordedMeasurement(feature)) {
    page->GetUseCounter().RecordMeasurement(feature, *frame);
    String message = DeprecationMessage(feature);

    DCHECK(!message.IsEmpty());
    ConsoleMessage* console_message = ConsoleMessage::Create(
        kDeprecationMessageSource, kWarningMessageLevel, message);
    frame->Console().AddMessage(console_message);

    GenerateReport(frame, message);
  }
}

void Deprecation::CountDeprecation(ExecutionContext* context,
                                   WebFeature feature) {
  if (!context)
    return;
  if (context->IsDocument()) {
    Deprecation::CountDeprecation(*ToDocument(context), feature);
    return;
  }
  if (context->IsWorkerOrWorkletGlobalScope())
    ToWorkerOrWorkletGlobalScope(context)->CountDeprecation(feature);
}

void Deprecation::CountDeprecation(const Document& document,
                                   WebFeature feature) {
  Deprecation::CountDeprecation(document.GetFrame(), feature);
}

void Deprecation::CountDeprecationCrossOriginIframe(const LocalFrame* frame,
                                                    WebFeature feature) {
  // Check to see if the frame can script into the top level document.
  SecurityOrigin* security_origin =
      frame->GetSecurityContext()->GetSecurityOrigin();
  Frame& top = frame->Tree().Top();
  if (!security_origin->CanAccess(
          top.GetSecurityContext()->GetSecurityOrigin()))
    CountDeprecation(frame, feature);
}

void Deprecation::CountDeprecationCrossOriginIframe(const Document& document,
                                                    WebFeature feature) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return;
  CountDeprecationCrossOriginIframe(frame, feature);
}

void Deprecation::CountDeprecationFeaturePolicy(
    const Document& document,
    WebFeaturePolicyFeature feature) {
  // If feature policy is not enabled, don't do anything.
  if (!RuntimeEnabledFeatures::FeaturePolicyEnabled())
    return;

  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return;

  // If the feature is allowed, don't log a warning.
  if (frame->IsFeatureEnabled(feature))
    return;

  // If the feature is disabled, log a warning but only if the request is from a
  // cross-origin iframe. Ideally we would check here if the feature is actually
  // disabled due to the parent frame's policy (as opposed to the current frame
  // disabling the feature on itself) but that can't happen right now anyway
  // (until the general syntax is shipped) and this is also a good enough
  // approximation for deprecation messages.
  switch (feature) {
    case WebFeaturePolicyFeature::kEme:
      CountDeprecationCrossOriginIframe(
          frame,
          WebFeature::
              kEncryptedMediaDisallowedByFeaturePolicyInCrossOriginIframe);
      break;
    case WebFeaturePolicyFeature::kGeolocation:
      CountDeprecationCrossOriginIframe(
          frame,
          WebFeature::kGeolocationDisallowedByFeaturePolicyInCrossOriginIframe);
      break;
    case WebFeaturePolicyFeature::kMicrophone:
      CountDeprecationCrossOriginIframe(
          frame,
          WebFeature::
              kGetUserMediaMicDisallowedByFeaturePolicyInCrossOriginIframe);
      break;
    case WebFeaturePolicyFeature::kCamera:
      CountDeprecationCrossOriginIframe(
          frame,
          WebFeature::
              kGetUserMediaCameraDisallowedByFeaturePolicyInCrossOriginIframe);
      break;
    case WebFeaturePolicyFeature::kMidiFeature:
      CountDeprecationCrossOriginIframe(
          frame,
          WebFeature::
              kRequestMIDIAccessDisallowedByFeaturePolicyInCrossOriginIframe);
      break;
    default:
      NOTREACHED();
  }
}

void Deprecation::GenerateReport(const LocalFrame* frame,
                                 const String& message) {
  if (!frame || !frame->Client())
    return;

  Document* document = frame->GetDocument();
  ReportingContext* reporting_context = ReportingContext::From(document);
  if (!reporting_context->ObserverExists())
    return;

  // Send a deprecation report to any ReportingObservers.
  ReportBody* body = new DeprecationReport(message, SourceLocation::Capture());
  Report* report = new Report("deprecation", document->Url().GetString(), body);
  reporting_context->QueueReport(report);
}

String Deprecation::DeprecationMessage(WebFeature feature) {
  switch (feature) {
    // Quota
    case WebFeature::kPrefixedStorageInfo:
      return replacedBy("'window.webkitStorageInfo'",
                        "'navigator.webkitTemporaryStorage' or "
                        "'navigator.webkitPersistentStorage'");

    case WebFeature::kConsoleMarkTimeline:
      return replacedBy("'console.markTimeline'", "'console.timeStamp'");

    case WebFeature::kPrefixedVideoSupportsFullscreen:
      return replacedBy("'HTMLVideoElement.webkitSupportsFullscreen'",
                        "'Document.fullscreenEnabled'");

    case WebFeature::kPrefixedVideoDisplayingFullscreen:
      return replacedBy("'HTMLVideoElement.webkitDisplayingFullscreen'",
                        "'Document.fullscreenElement'");

    case WebFeature::kPrefixedVideoEnterFullscreen:
      return replacedBy("'HTMLVideoElement.webkitEnterFullscreen()'",
                        "'Element.requestFullscreen()'");

    case WebFeature::kPrefixedVideoExitFullscreen:
      return replacedBy("'HTMLVideoElement.webkitExitFullscreen()'",
                        "'Document.exitFullscreen()'");

    case WebFeature::kPrefixedVideoEnterFullScreen:
      return replacedBy("'HTMLVideoElement.webkitEnterFullScreen()'",
                        "'Element.requestFullscreen()'");

    case WebFeature::kPrefixedVideoExitFullScreen:
      return replacedBy("'HTMLVideoElement.webkitExitFullScreen()'",
                        "'Document.exitFullscreen()'");

    case WebFeature::kPrefixedRequestAnimationFrame:
      return "'webkitRequestAnimationFrame' is vendor-specific. Please use the "
             "standard 'requestAnimationFrame' instead.";

    case WebFeature::kPrefixedCancelAnimationFrame:
      return "'webkitCancelAnimationFrame' is vendor-specific. Please use the "
             "standard 'cancelAnimationFrame' instead.";

    case WebFeature::kPictureSourceSrc:
      return "<source src> with a <picture> parent is invalid and therefore "
             "ignored. Please use <source srcset> instead.";

    case WebFeature::kConsoleTimeline:
      return replacedBy("'console.timeline'", "'console.time'");

    case WebFeature::kConsoleTimelineEnd:
      return replacedBy("'console.timelineEnd'", "'console.timeEnd'");

    case WebFeature::kXMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload:
      return "Synchronous XMLHttpRequest on the main thread is deprecated "
             "because of its detrimental effects to the end user's experience. "
             "For more help, check https://xhr.spec.whatwg.org/.";

    case WebFeature::kGetMatchedCSSRules:
      return willBeRemoved("document.getMatchedCSSRules()", M63,
                           "4606972603138048");

    case WebFeature::kPrefixedWindowURL:
      return replacedBy("'webkitURL'", "'URL'");

    case WebFeature::kRangeExpand:
      return replacedBy("'Range.expand()'", "'Selection.modify()'");

    // Blocked subresource requests:
    case WebFeature::kLegacyProtocolEmbeddedAsSubresource:
      return String::Format(
          "Subresource requests using legacy protocols (like `ftp:`) are "
          "blocked. Please deliver web-accessible resources over modern "
          "protocols like HTTPS. See "
          "https://www.chromestatus.com/feature/5709390967472128 for details.");

    case WebFeature::kRequestedSubresourceWithEmbeddedCredentials:
      return "Subresource requests whose URLs contain embedded credentials "
             "(e.g. `https://user:pass@host/`) are blocked. See "
             "https://www.chromestatus.com/feature/5669008342777856 for more "
             "details.";

    // Powerful features on insecure origins (https://goo.gl/rStTGz)
    case WebFeature::kDeviceMotionInsecureOrigin:
      return "The devicemotion event is deprecated on insecure origins, and "
             "support will be removed in the future. You should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case WebFeature::kDeviceOrientationInsecureOrigin:
      return "The deviceorientation event is deprecated on insecure origins, "
             "and support will be removed in the future. You should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case WebFeature::kDeviceOrientationAbsoluteInsecureOrigin:
      return "The deviceorientationabsolute event is deprecated on insecure "
             "origins, and support will be removed in the future. You should "
             "consider switching your application to a secure origin, such as "
             "HTTPS. See https://goo.gl/rStTGz for more details.";

    case WebFeature::kGeolocationInsecureOrigin:
    case WebFeature::kGeolocationInsecureOriginIframe:
      return "getCurrentPosition() and watchPosition() no longer work on "
             "insecure origins. To use this feature, you should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case WebFeature::kGeolocationInsecureOriginDeprecatedNotRemoved:
    case WebFeature::kGeolocationInsecureOriginIframeDeprecatedNotRemoved:
      return "getCurrentPosition() and watchPosition() are deprecated on "
             "insecure origins. To use this feature, you should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case WebFeature::kGetUserMediaInsecureOrigin:
    case WebFeature::kGetUserMediaInsecureOriginIframe:
      return "getUserMedia() no longer works on insecure origins. To use this "
             "feature, you should consider switching your application to a "
             "secure origin, such as HTTPS. See https://goo.gl/rStTGz for more "
             "details.";

    case WebFeature::kMediaSourceAbortRemove:
      return "Using SourceBuffer.abort() to abort remove()'s asynchronous "
             "range removal is deprecated due to specification change. Support "
             "will be removed in the future. You should instead await "
             "'updateend'. abort() is intended to only abort an asynchronous "
             "media append or reset parser state. See "
             "https://www.chromestatus.com/features/6107495151960064 for more "
             "details.";
    case WebFeature::kMediaSourceDurationTruncatingBuffered:
      return "Setting MediaSource.duration below the highest presentation "
             "timestamp of any buffered coded frames is deprecated due to "
             "specification change. Support for implicit removal of truncated "
             "buffered media will be removed in the future. You should instead "
             "perform explicit remove(newDuration, oldDuration) on all "
             "sourceBuffers, where newDuration < oldDuration. See "
             "https://www.chromestatus.com/features/6107495151960064 for more "
             "details.";

    case WebFeature::kApplicationCacheManifestSelectInsecureOrigin:
    case WebFeature::kApplicationCacheAPIInsecureOrigin:
      return "Use of the Application Cache is deprecated on insecure origins. "
             "Support will be removed in the future. You should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case WebFeature::kNotificationInsecureOrigin:
    case WebFeature::kNotificationAPIInsecureOriginIframe:
    case WebFeature::kNotificationPermissionRequestedInsecureOrigin:
      return String::Format(
          "The Notification API may no longer be used from insecure origins. "
          "You should consider switching your application to a secure origin, "
          "such as HTTPS. See https://goo.gl/rStTGz for more details.");

    case WebFeature::kNotificationPermissionRequestedIframe:
      return String::Format(
          "Permission for the Notification API may no longer be requested from "
          "a cross-origin iframe. You should consider requesting permission "
          "from a top-level frame or opening a new window instead. See "
          "https://www.chromestatus.com/feature/6451284559265792 for more "
          "details.");

    case WebFeature::kElementCreateShadowRootMultiple:
      return willBeRemoved(
          "Calling Element.createShadowRoot() for an element "
          "which already hosts a shadow root",
          M63, "4668884095336448");

    case WebFeature::kCSSDeepCombinator:
      return "/deep/ combinator is no longer supported in CSS dynamic profile."
             "It is now effectively no-op, acting as if it were a descendant "
             "combinator. /deep/ combinator will be removed, and will be "
             "invalid at M65. You should remove it. See "
             "https://www.chromestatus.com/features/4964279606312960 for more "
             "details.";

    case WebFeature::kVREyeParametersOffset:
      return replacedBy("VREyeParameters.offset",
                        "view matrices provided by VRFrameData");

    case WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton:
      return willBeRemoved(
          "-internal-media-controls-overlay-cast-button selector", M61,
          "5714245488476160");

    case WebFeature::kSelectionAddRangeIntersect:
      return "The behavior that Selection.addRange() merges existing Range and "
             "the specified Range was removed. See "
             "https://www.chromestatus.com/features/6680566019653632 for more "
             "details.";

    case WebFeature::kRtcpMuxPolicyNegotiate:
      return String::Format(
          "The rtcpMuxPolicy option is being considered for "
          "removal and may be removed no earlier than %s. If you depend on it, "
          "please see https://www.chromestatus.com/features/5654810086866944 "
          "for more details.",
          milestoneString(M62));

    case WebFeature::kVibrateWithoutUserGesture:
      return willBeRemoved(
          "A call to navigator.vibrate without user tap on the frame or any "
          "embedded frame",
          M60, "5644273861001216");

    case WebFeature::kChildSrcAllowedWorkerThatScriptSrcBlocked:
      return replacedWillBeRemoved("The 'child-src' directive",
                                   "the 'script-src' directive for Workers",
                                   M60, "5922594955984896");

    case WebFeature::kCanRequestURLHTTPContainingNewline:
      return "Resource requests whose URLs contained both removed whitespace "
             "(`\\n`, `\\r`, `\\t`) characters and less-than characters (`<`) "
             "are blocked. Please remove newlines and encode less-than "
             "characters from places like element attribute values in order to "
             "load these resources. See "
             "https://www.chromestatus.com/feature/5735596811091968 for more "
             "details.";

    case WebFeature::kCredentialManagerCredentialRequestOptionsUnmediated:
      return replacedWillBeRemoved(
          "The boolean flag CredentialRequestOptions.unmediated",
          "the CredentialRequestOptions.mediation enum", M62,
          "6076479909658624");

    case WebFeature::kCredentialManagerIdName:
    case WebFeature::kCredentialManagerPasswordName:
    case WebFeature::kCredentialManagerAdditionalData:
    case WebFeature::kCredentialManagerCustomFetch:
      return String::Format(
          "Passing 'PasswordCredential' objects into 'fetch(..., { "
          "credentials: ... })' is deprecated, and will be removed in %s. See "
          "https://www.chromestatus.com/features/5689327799500800 for more "
          "details and https://developers.google.com/web/updates/2017/06/"
          "credential-management-updates for migration suggestions.",
          milestoneString(M62));
    case WebFeature::kPaymentRequestNetworkNameInSupportedMethods:
      return replacedWillBeRemoved(
          "Card issuer network (\"amex\", \"diners\", \"discover\", \"jcb\", "
          "\"mastercard\", \"mir\", \"unionpay\", \"visa\") as payment method",
          "payment method name \"basic-card\" with issuer network in the "
          "\"supportedNetworks\" field",
          M64, "5725727580225536");
    case WebFeature::kCredentialManagerRequireUserMediation:
      return replacedWillBeRemoved(
          "The CredentialsContainer.requireUserMediation method",
          "the CredentialsContainer.preventSilentAccess method", M62,
          "4781762488041472");
    case WebFeature::kDeprecatedTimingFunctionStepMiddle:
      return replacedWillBeRemoved(
          "The step timing function with step position 'middle'",
          "the frames timing function", M62, "5189363944128512");
    case WebFeature::kHTMLImportsHasStyleSheets:
      return String::Format(
          "Styling master document from stylesheets defined in HTML Imports "
          "is deprecated, and is planned to be removed in %s. Please refer to "
          "https://goo.gl/EGXzpw for possible migration paths.",
          milestoneString(M65));
    case WebFeature::
        kEncryptedMediaDisallowedByFeaturePolicyInCrossOriginIframe:
      return DeprecatedWillBeDisabledByFeaturePolicyInCrossOriginIframe(
          "requestMediaKeySystemAccess", "encrypted-media", M63);
    case WebFeature::kGeolocationDisallowedByFeaturePolicyInCrossOriginIframe:
      return DeprecatedWillBeDisabledByFeaturePolicyInCrossOriginIframe(
          "getCurrentPosition and watchPosition", "geolocation", M63);
    case WebFeature::
        kGetUserMediaMicDisallowedByFeaturePolicyInCrossOriginIframe:
      return DeprecatedWillBeDisabledByFeaturePolicyInCrossOriginIframe(
          "getUserMedia (microphone)", "microphone", M63);
    case WebFeature::
        kGetUserMediaCameraDisallowedByFeaturePolicyInCrossOriginIframe:
      return DeprecatedWillBeDisabledByFeaturePolicyInCrossOriginIframe(
          "getUserMedia (camera)", "camera", M63);
    case WebFeature::
        kRequestMIDIAccessDisallowedByFeaturePolicyInCrossOriginIframe:
      return DeprecatedWillBeDisabledByFeaturePolicyInCrossOriginIframe(
          "requestMIDIAccess", "midi", M63);

    case WebFeature::kPresentationRequestStartInsecureOrigin:
    case WebFeature::kPresentationReceiverInsecureOrigin:
      return String(
          "Using the Presentation API on insecure origins is "
          "deprecated and will be removed in M68. You should consider "
          "switching your application to a secure origin, such as HTTPS. See "
          "https://goo.gl/rStTGz for more details.");

    case WebFeature::kPaymentRequestSupportedMethodsArray:
      return replacedWillBeRemoved(
          "PaymentRequest's supportedMethods taking an array",
          "a single string", M64, "5177301645918208");

    // Features that aren't deprecated don't have a deprecation message.
    default:
      return String();
  }
}

}  // namespace blink
