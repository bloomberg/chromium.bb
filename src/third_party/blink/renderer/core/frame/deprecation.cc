// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/deprecation.h"

#include <bitset>
#include "build/build_config.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation_report_body.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

using blink::WebFeature;

namespace {

const char kChromeLoadTimesNavigationTiming[] =
    "chrome.loadTimes() is deprecated, instead use standardized API: "
    "Navigation Timing 2. "
    "https://www.chromestatus.com/features/5637885046816768.";
const char kChromeLoadTimesNextHopProtocol[] =
    "chrome.loadTimes() is deprecated, instead use standardized API: "
    "nextHopProtocol in Navigation Timing 2. "
    "https://www.chromestatus.com/features/5637885046816768.";

const char kChromeLoadTimesPaintTiming[] =
    "chrome.loadTimes() is deprecated, instead use standardized API: "
    "Paint Timing. "
    "https://www.chromestatus.com/features/5637885046816768.";

enum Milestone {
  kUnknown,
  kM61,
  kM62,
  kM64,
  kM65,
  kM70,
  kM71,
  kM72,
  kM75,
  kM76,
  kM77,
  kM78,
  kM79,
  kM80,
};

// Returns estimated milestone dates as human-readable strings.
const char* MilestoneString(Milestone milestone) {
  // These are the Estimated Stable Dates:
  // https://www.chromium.org/developers/calendar

  switch (milestone) {
    case kUnknown:
      return "";
    case kM61:
      return "M61, around September 2017";
    case kM62:
      return "M62, around October 2017";
    case kM64:
      return "M64, around January 2018";
    case kM65:
      return "M65, around March 2018";
    case kM70:
      return "M70, around October 2018";
    case kM71:
      return "M71, around December 2018";
    case kM72:
      return "M72, around January 2019";
    case kM75:
      return "M75, around June 2019";
    case kM76:
      return "M76, around July 2019";
    case kM77:
      return "M77, around September 2019";
    case kM78:
      return "M78, around October 2019";
    case kM79:
      return "M79, around December 2019";
    case kM80:
      return "M80, around February 2020";
  }

  NOTREACHED();
  return nullptr;
}

// Returns estimated milestone dates as milliseconds since January 1, 1970.
double MilestoneDate(Milestone milestone) {
  // These are the Estimated Stable Dates:
  // https://www.chromium.org/developers/calendar
  // All are at 04:00:00 GMT.
  // TODO(yoichio): We should have something like "Time(March, 6, 2018)".
  switch (milestone) {
    case kUnknown:
      return 0;
    case kM61:
      return 1504584000000;  // September 5, 2017.
    case kM62:
      return 1508212800000;  // October 17, 2017.
    case kM64:
      return 1516683600000;  // January 23, 2018.
    case kM65:
      return 1520312400000;  // March 6, 2018.
    case kM70:
      return 1539662400000;  // October 16, 2018.
    case kM71:
      return 1543899600000;  // December 4, 2018.
    case kM72:
      return 1548734400000;  // January 29, 2019.
    case kM75:
      return 1559620800000;  // June 4, 2019.
    case kM76:
      return 1564459200000;  // Jul 30, 2019.
    case kM77:
      return 1568088000000;  // September 10, 2019.
    case kM78:
      return 1571716800000;  // October 22, 2019.
    case kM79:
      return 1575950400000;  // December 10, 2019.
    case kM80:
      return 1580529600000;  // February 1, 2020. (This is a guess!)
  }

  NOTREACHED();
  return 0;
}

struct DeprecationInfo {
  String id;
  Milestone anticipated_removal;
  String message;
};

String ReplacedBy(const char* feature, const char* replacement) {
  return String::Format("%s is deprecated. Please use %s instead.", feature,
                        replacement);
}

String WillBeRemoved(const char* feature,
                     Milestone milestone,
                     const char* details) {
  return String::Format(
      "%s is deprecated and will be removed in %s. See "
      "https://www.chromestatus.com/features/%s for more details.",
      feature, MilestoneString(milestone), details);
}

String ReplacedWillBeRemoved(const char* feature,
                             const char* replacement,
                             Milestone milestone,
                             const char* details) {
  return String::Format(
      "%s is deprecated and will be removed in %s. Please use %s instead. See "
      "https://www.chromestatus.com/features/%s for more details.",
      feature, MilestoneString(milestone), replacement, details);
}

DeprecationInfo GetDeprecationInfo(WebFeature feature) {
  switch (feature) {
    // Quota
    case WebFeature::kPrefixedStorageInfo:
      return {"PrefixedStorageInfo", kUnknown,
              ReplacedBy("'window.webkitStorageInfo'",
                         "'navigator.webkitTemporaryStorage' or "
                         "'navigator.webkitPersistentStorage'")};

    case WebFeature::kPrefixedVideoSupportsFullscreen:
      return {"PrefixedVideoSupportsFullscreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitSupportsFullscreen'",
                         "'Document.fullscreenEnabled'")};

    case WebFeature::kPrefixedVideoDisplayingFullscreen:
      return {"PrefixedVideoDisplayingFullscreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitDisplayingFullscreen'",
                         "'Document.fullscreenElement'")};

    case WebFeature::kPrefixedVideoEnterFullscreen:
      return {"PrefixedVideoEnterFullscreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitEnterFullscreen()'",
                         "'Element.requestFullscreen()'")};

    case WebFeature::kPrefixedVideoExitFullscreen:
      return {"PrefixedVideoExitFullscreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitExitFullscreen()'",
                         "'Document.exitFullscreen()'")};

    case WebFeature::kPrefixedVideoEnterFullScreen:
      return {"PrefixedVideoEnterFullScreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitEnterFullScreen()'",
                         "'Element.requestFullscreen()'")};

    case WebFeature::kPrefixedVideoExitFullScreen:
      return {"PrefixedVideoExitFullScreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitExitFullScreen()'",
                         "'Document.exitFullscreen()'")};

    case WebFeature::kPrefixedRequestAnimationFrame:
      return {"PrefixedRequestAnimationFrame", kUnknown,
              "'webkitRequestAnimationFrame' is vendor-specific. Please use "
              "the standard 'requestAnimationFrame' instead."};

    case WebFeature::kPrefixedCancelAnimationFrame:
      return {"PrefixedCancelAnimationFrame", kUnknown,
              "'webkitCancelAnimationFrame' is vendor-specific. Please use the "
              "standard 'cancelAnimationFrame' instead."};

    case WebFeature::kPictureSourceSrc:
      return {"PictureSourceSrc", kUnknown,
              "<source src> with a <picture> parent is invalid and therefore "
              "ignored. Please use <source srcset> instead."};

    case WebFeature::kXMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload:
      return {"XMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload",
              kUnknown,
              "Synchronous XMLHttpRequest on the main thread is deprecated "
              "because of its detrimental effects to the end user's "
              "experience. For more help, check https://xhr.spec.whatwg.org/."};

    case WebFeature::kPrefixedWindowURL:
      return {"PrefixedWindowURL", kUnknown,
              ReplacedBy("'webkitURL'", "'URL'")};

    case WebFeature::kRangeExpand:
      return {"RangeExpand", kUnknown,
              ReplacedBy("'Range.expand()'", "'Selection.modify()'")};

    // Blocked subresource requests:
    case WebFeature::kLegacyProtocolEmbeddedAsSubresource:
      return {"LegacyProtocolEmbeddedAsSubresource", kUnknown,
              String::Format(
                  "Subresource requests using legacy protocols (like `ftp:`) "
                  "are blocked. Please deliver web-accessible resources over "
                  "modern protocols like HTTPS. See "
                  "https://www.chromestatus.com/feature/5709390967472128 for "
                  "details.")};

    case WebFeature::kRequestedSubresourceWithEmbeddedCredentials:
      return {"RequestedSubresourceWithEmbeddedCredentials", kUnknown,
              "Subresource requests whose URLs contain embedded credentials "
              "(e.g. `https://user:pass@host/`) are blocked. See "
              "https://www.chromestatus.com/feature/5669008342777856 for more "
              "details."};

    // Powerful features on insecure origins (https://goo.gl/rStTGz)
    case WebFeature::kGeolocationInsecureOrigin:
    case WebFeature::kGeolocationInsecureOriginIframe:
      return {"GeolocationInsecureOrigin", kUnknown,
              "getCurrentPosition() and watchPosition() no longer work on "
              "insecure origins. To use this feature, you should consider "
              "switching your application to a secure origin, such as HTTPS. "
              "See https://goo.gl/rStTGz for more details."};

    case WebFeature::kGeolocationInsecureOriginDeprecatedNotRemoved:
    case WebFeature::kGeolocationInsecureOriginIframeDeprecatedNotRemoved:
      return {"GeolocationInsecureOriginDeprecatedNotRemoved", kUnknown,
              "getCurrentPosition() and watchPosition() are deprecated on "
              "insecure origins. To use this feature, you should consider "
              "switching your application to a secure origin, such as HTTPS. "
              "See https://goo.gl/rStTGz for more details."};

    case WebFeature::kGetUserMediaInsecureOrigin:
    case WebFeature::kGetUserMediaInsecureOriginIframe:
      return {
          "GetUserMediaInsecureOrigin", kUnknown,
          "getUserMedia() no longer works on insecure origins. To use this "
          "feature, you should consider switching your application to a "
          "secure origin, such as HTTPS. See https://goo.gl/rStTGz for more "
          "details."};

    case WebFeature::kMediaSourceAbortRemove:
      return {
          "MediaSourceAbortRemove", kUnknown,
          "Using SourceBuffer.abort() to abort remove()'s asynchronous "
          "range removal is deprecated due to specification change. Support "
          "will be removed in the future. You should instead await "
          "'updateend'. abort() is intended to only abort an asynchronous "
          "media append or reset parser state. See "
          "https://www.chromestatus.com/features/6107495151960064 for more "
          "details."};

    case WebFeature::kMediaSourceDurationTruncatingBuffered:
      return {
          "MediaSourceDurationTruncatingBuffered", kUnknown,
          "Setting MediaSource.duration below the highest presentation "
          "timestamp of any buffered coded frames is deprecated due to "
          "specification change. Support for implicit removal of truncated "
          "buffered media will be removed in the future. You should instead "
          "perform explicit remove(newDuration, oldDuration) on all "
          "sourceBuffers, where newDuration < oldDuration. See "
          "https://www.chromestatus.com/features/6107495151960064 for more "
          "details."};

    case WebFeature::kApplicationCacheManifestSelectInsecureOrigin:
    case WebFeature::kApplicationCacheAPIInsecureOrigin:
      return {"ApplicationCacheAPIInsecureOrigin", kM70,
              "Application Cache is restricted to secure contexts. Please "
              "consider migrating your application to HTTPS, and eventually "
              "shifting over to Service Workers. See https://goo.gl/rStTGz for "
              "more details."};

    case WebFeature::kNotificationInsecureOrigin:
    case WebFeature::kNotificationAPIInsecureOriginIframe:
    case WebFeature::kNotificationPermissionRequestedInsecureOrigin:
      return {
          "NotificationInsecureOrigin", kUnknown,
          String::Format(
              "The Notification API may no longer be used from insecure "
              "origins. "
              "You should consider switching your application to a secure "
              "origin, "
              "such as HTTPS. See https://goo.gl/rStTGz for more details.")};

    case WebFeature::kNotificationPermissionRequestedIframe:
      return {
          "NotificationPermissionRequestedIframe", kUnknown,
          String::Format(
              "Permission for the Notification API may no longer be requested "
              "from "
              "a cross-origin iframe. You should consider requesting "
              "permission "
              "from a top-level frame or opening a new window instead. See "
              "https://www.chromestatus.com/feature/6451284559265792 for more "
              "details.")};

    case WebFeature::kCSSDeepCombinator:
      return {"CSSDeepCombinator", kM65,
              "/deep/ combinator is no longer supported in CSS dynamic profile."
              "It is now effectively no-op, acting as if it were a descendant "
              "combinator. /deep/ combinator will be removed, and will be "
              "invalid at M65. You should remove it. See "
              "https://www.chromestatus.com/features/4964279606312960 for more "
              "details."};

    case WebFeature::kVREyeParametersOffset:
      return {"VREyeParametersOffset", kUnknown,
              ReplacedBy("VREyeParameters.offset",
                         "view matrices provided by VRFrameData")};

    case WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton:
      return {
          "CSSSelectorInternalMediaControlsOverlayCastButton", kM61,
          WillBeRemoved("-internal-media-controls-overlay-cast-button selector",
                        kM61, "5714245488476160")};

    case WebFeature::kSelectionAddRangeIntersect:
      return {
          "SelectionAddRangeIntersect", kUnknown,
          "The behavior that Selection.addRange() merges existing Range and "
          "the specified Range was removed. See "
          "https://www.chromestatus.com/features/6680566019653632 for more "
          "details."};

    case WebFeature::kRtcpMuxPolicyNegotiate:
      return {"RtcpMuxPolicyNegotiate", kM62,
              String::Format("The rtcpMuxPolicy option is being considered for "
                             "removal and may be removed no earlier than %s. "
                             "If you depend on it, "
                             "please see "
                             "https://www.chromestatus.com/features/"
                             "5654810086866944 "
                             "for more details.",
                             MilestoneString(kM62))};

    case WebFeature::kCanRequestURLHTTPContainingNewline:
      return {
          "CanRequestURLHTTPContainingNewline", kUnknown,
          "Resource requests whose URLs contained both removed whitespace "
          "(`\\n`, `\\r`, `\\t`) characters and less-than characters (`<`) "
          "are blocked. Please remove newlines and encode less-than "
          "characters from places like element attribute values in order to "
          "load these resources. See "
          "https://www.chromestatus.com/feature/5735596811091968 for more "
          "details."};

#define kWebComponentsV0DeprecationPost                \
  "https://developers.google.com/web/updates/2019/07/" \
  "web-components-time-to-upgrade"

    case WebFeature::kHTMLImports:
      return {"HTMLImports", kM80,
              ReplacedWillBeRemoved(
                  "HTML Imports", "ES modules", kM80,
                  "5144752345317376 and " kWebComponentsV0DeprecationPost)};

    case WebFeature::kElementCreateShadowRoot:
      return {"ElementCreateShadowRoot", kM80,
              ReplacedWillBeRemoved(
                  "Element.createShadowRoot", "Element.attachShadow", kM80,
                  "4507242028072960 and " kWebComponentsV0DeprecationPost)};

    case WebFeature::kDocumentRegisterElement:
      return {
          "DocumentRegisterElement", kM80,
          ReplacedWillBeRemoved(
              "document.registerElement", "window.customElements.define", kM80,
              "4642138092470272 and " kWebComponentsV0DeprecationPost)};
    case WebFeature::kCSSSelectorPseudoUnresolved:
      return {"CSSSelectorPseudoUnresolved", kM80,
              ReplacedWillBeRemoved(
                  ":unresolved pseudo selector", ":not(:defined)", kM80,
                  "4642138092470272 and " kWebComponentsV0DeprecationPost)};

#undef kWebComponentsV0DeprecationPost

    case WebFeature::kPresentationRequestStartInsecureOrigin:
    case WebFeature::kPresentationReceiverInsecureOrigin:
      return {
          "PresentationInsecureOrigin", kM72,
          String("Using the Presentation API on insecure origins is "
                 "deprecated and will be removed in M72. You should consider "
                 "switching your application to a secure origin, such as "
                 "HTTPS. See "
                 "https://goo.gl/rStTGz for more details.")};

    case WebFeature::kLocalCSSFileExtensionRejected:
      return {"LocalCSSFileExtensionRejected", kM64,
              String("CSS cannot be loaded from `file:` URLs unless they end "
                     "in a `.css` file extension.")};

    case WebFeature::kChromeLoadTimesRequestTime:
    case WebFeature::kChromeLoadTimesStartLoadTime:
    case WebFeature::kChromeLoadTimesCommitLoadTime:
    case WebFeature::kChromeLoadTimesFinishDocumentLoadTime:
    case WebFeature::kChromeLoadTimesFinishLoadTime:
    case WebFeature::kChromeLoadTimesNavigationType:
    case WebFeature::kChromeLoadTimesConnectionInfo:
      return {"ChromeLoadTimesConnectionInfo", kUnknown,
              kChromeLoadTimesNavigationTiming};

    case WebFeature::kChromeLoadTimesFirstPaintTime:
    case WebFeature::kChromeLoadTimesFirstPaintAfterLoadTime:
      return {"ChromeLoadTimesFirstPaintAfterLoadTime", kUnknown,
              kChromeLoadTimesPaintTiming};

    case WebFeature::kChromeLoadTimesWasFetchedViaSpdy:
    case WebFeature::kChromeLoadTimesWasNpnNegotiated:
    case WebFeature::kChromeLoadTimesNpnNegotiatedProtocol:
    case WebFeature::kChromeLoadTimesWasAlternateProtocolAvailable:
      return {"ChromeLoadTimesWasAlternateProtocolAvailable", kUnknown,
              kChromeLoadTimesNextHopProtocol};

    case WebFeature::kMediaElementSourceOnOfflineContext:
      return {"MediaElementAudioSourceNode", kM71,
              WillBeRemoved("Creating a MediaElementAudioSourceNode on an "
                            "OfflineAudioContext",
                            kM71, "5258622686724096")};

    case WebFeature::kMediaStreamDestinationOnOfflineContext:
      return {"MediaStreamAudioDestinationNode", kM71,
              WillBeRemoved("Creating a MediaStreamAudioDestinationNode on an "
                            "OfflineAudioContext",
                            kM71, "5258622686724096")};

    case WebFeature::kMediaStreamSourceOnOfflineContext:
      return {
          "MediaStreamAudioSourceNode", kM71,
          WillBeRemoved(
              "Creating a MediaStreamAudioSourceNode on an OfflineAudioContext",
              kM71, "5258622686724096")};

    case WebFeature::kGridRowTrackPercentIndefiniteHeight:
      return {"GridRowTrackPercentIndefiniteHeight", kM70,
              String::Format("Percentages row tracks and gutters for "
                             "indefinite height grid containers will be "
                             "resolved against the intrinsic height instead of "
                             "being treated as auto and zero respectively. "
                             "This change will happen in %s. See "
                             "https://www.chromestatus.com/feature/"
                             "6708326821789696 for more details.",
                             MilestoneString(kM70))};
    case WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay:
      return {
          "TextToSpeech_DisallowedByAutoplay", kM71,
          String::Format("speechSynthesis.speak() without user activation is "
                         "no longer allowed since %s. See "
                         "https://www.chromestatus.com/feature/"
                         "5687444770914304 for more details",
                         MilestoneString(kM71))};

    case WebFeature::kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics:
      return {"RTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics", kM72,
              String::Format(
                  "\"Complex\" Plan B SDP detected! Chrome will switch the "
                  "default sdpSemantics in %s from 'plan-b' to the "
                  "standardized 'unified-plan' format and this peer connection "
                  "is relying on the default sdpSemantics. This SDP is not "
                  "compatible with Unified Plan and will be rejected by "
                  "clients expecting Unified Plan. For more information about "
                  "how to prepare for the switch, see "
                  "https://webrtc.org/web-apis/chrome/unified-plan/.",
                  MilestoneString(kM72))};

    case WebFeature::kNoSysexWebMIDIWithoutPermission:
      return {"NoSysexWebMIDIWithoutPermission", kM77,
              String::Format(
                  "Web MIDI will ask a permission to use even if the sysex is "
                  "not specified in the MIDIOptions since %s. See "
                  "https://www.chromestatus.com/feature/5138066234671104 for "
                  "more details.",
                  MilestoneString(kM77))};

    case WebFeature::kCustomCursorIntersectsViewport:
      return {
          "CustomCursorIntersectsViewport", kM75,
          WillBeRemoved(
              "Custom cursors with size greater than 32x32 DIP intersecting "
              "native UI",
              kM75, "5825971391299584")};

    case WebFeature::kV8AtomicsWake:
      return {"V8AtomicsWake", kM76,
              ReplacedWillBeRemoved("Atomics.wake", "Atomics.notify", kM76,
                                    "6228189936353280")};

    case WebFeature::kVRGetDisplays:
      return {"WebVR", kM79,
              String::Format("WebVR is deprecated and will be removed as soon "
                             "as %s. Please use WebXR instead.  See "
                             "https://www.chromestatus.com/feature/"
                             "4532810371039232 for details.",
                             MilestoneString(kM79))};

    case WebFeature::kCSSValueAppearanceCheckboxForOthersRendered:
      return {"CSSValueAppearanceCheckboxForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: checkbox' for elements other "
                            "than input[type=checkbox]",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceRadioForOthersRendered:
      return {"CSSValueAppearanceRadioForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: radio' for elements other "
                            "than input[type=radio]",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearancePushButtonForOthersRendered:
      return {
          "CSSValueAppearancePushButtonForOthersRendered", kM79,
          WillBeRemoved(
              "'-webkit-appearance: push-button' for elements other than "
              "input[type=button], input[type=reset], and input[type=submit]",
              kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceSquareButtonForOthersRendered:
      return {"CSSValueAppearanceSquareButtonForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: square-button' for elements "
                            "other than input[type=color]",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceButtonForAnchor:
      return {"CSSValueAppearanceSquareButtonForAnchor", kM79,
              WillBeRemoved("'-webkit-appearance: button' for a", kM79,
                            "5070237827334144")};
    case WebFeature::kCSSValueAppearanceButtonForSelectRendered:
      return {
          "CSSValueAppearanceButtonForSelectRendered", kM79,
          WillBeRemoved("'-webkit-appearance: button' for drop-down box select",
                        kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceInnerSpinButtonForOthersRendered:
      return {"CSSValueAppearanceInnerSpinButtonForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: inner-spin-button' for "
                            "elements other than :-webkit-inner-spin-button",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceListboxForOthersRendered:
      return {"CSSValueAppearanceListboxForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: listbox' for "
                            "elements other than listbox select",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceMenuListForOthersRendered:
      return {
        "CSSValueAppearanceMenuListForOthersRendered", kM79,
            WillBeRemoved(
#if defined(OS_ANDROID)
                "'-webkit-appearance: menulist' for elements other "
                "than drop-down box select, input[type=color][list], "
                "input[type=date], "
                "input[type=datetime-local], input[type=month], "
                "input[type=time], and input[type=week]",
#else
                "'-webkit-appearance: menulist' for elements other "
                "than select and input[type=color][list]",
#endif
                kM79, "5070237827334144")
      };
    case WebFeature::kCSSValueAppearanceMenuListButtonForOthersRendered:
      return {
        "CSSValueAppearanceMenuListButtonForOthersRendered", kM79,
            WillBeRemoved(
#if defined(OS_ANDROID)
                "'-webkit-appearance: menulist-button' for elements other "
                "than drop-down box select, input[type=color][list], "
                "input[type=date], "
                "input[type=datetime-local], input[type=month], "
                "input[type=time], and input[type=week]",
#else
                "'-webkit-appearance: menulist-button' for elements other "
                "than select and input[type=color][list]",
#endif
                kM79, "5070237827334144")
      };
    case WebFeature::kCSSValueAppearanceMeterForOthersRendered:
      return {"CSSValueAppearanceMeterForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: meter' for "
                            "elements other than meter",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceProgressBarForOthersRendered:
      return {"CSSValueAppearanceProgressBarForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: progress-bar' for "
                            "elements other than progress",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceSliderHorizontalForOthersRendered:
      return {"CSSValueAppearanceSliderHorizontalForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: slider-horizontal' for "
                            "elements other than input[type=range]",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceSliderVerticalForOthersRendered:
      return {"CSSValueAppearanceSliderVerticalForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: slider-vertical' for "
                            "elements other than input[type=range]",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceSliderThumbHorizontalForOthersRendered:
      return {"CSSValueAppearanceSliderThumbHorizontalForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: sliderthumb-horizontal' for "
                            "elements other than ::-webkit-slider-thumb and "
                            "::-webkit-media-slider-thumb",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceSliderThumbVerticalForOthersRendered:
      return {"CSSValueAppearanceSliderThumbVerticalForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: sliderthumb-vertical' for "
                            "elements other than ::-webkit-slider-thumb and "
                            "::-webkit-media-slider-thumb",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceSearchFieldForOthersRendered:
      return {"CSSValueAppearanceSearchFieldForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: searchfield' for "
                            "elements other than input[type=search]",
                            kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceSearchCancelForOthers2Rendered:
      return {
          "CSSValueAppearanceSearchCancelForOthers2Rendered", kM79,
          WillBeRemoved("'-webkit-appearance: searchfield-cancel-button' for "
                        "elements other than ::-webkit-clear-button and "
                        "::-webkit-search-cancel-button",
                        kM79, "5070237827334144")};
    case WebFeature::kCSSValueAppearanceTextFieldForOthersRendered:
      return {
        "CSSValueAppearanceTextFieldForOthersRendered", kM79,
            WillBeRemoved(
                "'-webkit-appearance: textfield' for "
#if defined(OS_ANDROID)
                "elements other than input[type=email], input[type=number], "
                "input[type=password], input[type=search], input[type=tel], "
                "input[type=text], input[type=url]",
#else
                "elements other than input[type=email], input[type=number], "
                "input[type=password], input[type=search], input[type=tel], "
                "input[type=text], input[type=url], input[type=date], "
                "input[type=datetime-local], input[type=month], "
                "input[type=time], and input[type=week]",
#endif
                kM79, "5070237827334144")
      };
    case WebFeature::kCSSValueAppearanceTextAreaForOthersRendered:
      return {"CSSValueAppearanceTextAreaForOthersRendered", kM79,
              WillBeRemoved("'-webkit-appearance: textarea' for "
                            "elements other than textarea",
                            kM79, "5070237827334144")};

    // Features that aren't deprecated don't have a deprecation message.
    default:
      return {"NotDeprecated", kUnknown, ""};
  }
}

}  // anonymous namespace

namespace blink {

Deprecation::Deprecation() : mute_count_(0) {
}

Deprecation::~Deprecation() = default;

void Deprecation::ClearSuppression() {
  css_property_deprecation_bits_.reset();
  features_deprecation_bits_.reset();
}

void Deprecation::MuteForInspector() {
  mute_count_++;
}

void Deprecation::UnmuteForInspector() {
  mute_count_--;
}

void Deprecation::Suppress(CSSPropertyID unresolved_property) {
  DCHECK(isCSSPropertyIDWithName(unresolved_property));
  css_property_deprecation_bits_.set(static_cast<size_t>(unresolved_property));
}

bool Deprecation::IsSuppressed(CSSPropertyID unresolved_property) {
  DCHECK(isCSSPropertyIDWithName(unresolved_property));
  return css_property_deprecation_bits_[static_cast<size_t>(
      unresolved_property)];
}

void Deprecation::SetReported(WebFeature feature) {
  features_deprecation_bits_.set(static_cast<size_t>(feature));
}

bool Deprecation::GetReported(WebFeature feature) const {
  return features_deprecation_bits_[static_cast<size_t>(feature)];
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
    ConsoleMessage* console_message =
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kDeprecation,
                               mojom::ConsoleMessageLevel::kWarning, message);
    frame->Console().AddMessage(console_message);
  }
}

String Deprecation::DeprecationMessage(CSSPropertyID unresolved_property) {
  // TODO: Add a switch here when there are properties that we intend to
  // deprecate.
  // Returning an empty string for now.
  return g_empty_string;
}

void Deprecation::CountDeprecation(ExecutionContext* context,
                                   WebFeature feature) {
  if (!context)
    return;

  context->CountDeprecation(feature);
}

void Deprecation::CountDeprecation(const Document& document,
                                   WebFeature feature) {
  Deprecation::CountDeprecation(document.Loader(), feature);
}

void Deprecation::CountDeprecation(DocumentLoader* loader, WebFeature feature) {
  if (!loader)
    return;
  LocalFrame* frame = loader->GetFrame();
  if (!frame)
    return;
  Page* page = frame->GetPage();
  if (!loader || !page || page->GetDeprecation().mute_count_ ||
      page->GetDeprecation().GetReported(feature))
    return;

  page->GetDeprecation().SetReported(feature);
  UseCounter::Count(loader, feature);
  GenerateReport(frame, feature);
}

void Deprecation::CountDeprecationCrossOriginIframe(const Document& document,
                                                    WebFeature feature) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return;

  // Check to see if the frame can script into the top level document.
  const SecurityOrigin* security_origin =
      frame->GetSecurityContext()->GetSecurityOrigin();
  Frame& top = frame->Tree().Top();
  if (!security_origin->CanAccess(
          top.GetSecurityContext()->GetSecurityOrigin()))
    CountDeprecation(document, feature);
}

void Deprecation::GenerateReport(const LocalFrame* frame, WebFeature feature) {
  DeprecationInfo info = GetDeprecationInfo(feature);

  // Send the deprecation message to the console as a warning.
  DCHECK(!info.message.IsEmpty());
  ConsoleMessage* console_message = ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kDeprecation,
      mojom::ConsoleMessageLevel::kWarning, info.message);
  frame->Console().AddMessage(console_message);

  if (!frame || !frame->Client())
    return;

  Document* document = frame->GetDocument();

  // Construct the deprecation report.
  double removal_date = MilestoneDate(info.anticipated_removal);
  DeprecationReportBody* body = MakeGarbageCollected<DeprecationReportBody>(
      info.id, removal_date, info.message);
  Report* report = MakeGarbageCollected<Report>(
      ReportType::kDeprecation, document->Url().GetString(), body);

  // Send the deprecation report to the Reporting API and any
  // ReportingObservers.
  ReportingContext::From(document)->QueueReport(report);
}

// static
String Deprecation::DeprecationMessage(WebFeature feature) {
  return GetDeprecationInfo(feature).message;
}

}  // namespace blink
