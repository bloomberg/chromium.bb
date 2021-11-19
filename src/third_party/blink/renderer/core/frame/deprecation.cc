// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/deprecation.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation_report_body.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"

namespace blink {

namespace {

enum Milestone {
  kUnknown,
  kM61 = 61,
  kM62 = 62,
  kM64 = 64,
  kM65 = 65,
  kM70 = 70,
  kM71 = 71,
  kM72 = 72,
  kM75 = 75,
  kM76 = 76,
  kM77 = 77,
  kM78 = 78,
  kM79 = 79,
  kM80 = 80,
  kM81 = 81,
  kM82 = 82,
  kM83 = 83,
  kM84 = 84,
  kM85 = 85,
  kM86 = 86,
  kM87 = 87,
  kM88 = 88,
  kM89 = 89,
  kM90 = 90,
  kM91 = 91,
  kM92 = 92,
  kM93 = 93,
  kM94 = 94,
  kM95 = 95,
  kM96 = 96,
  kM97 = 97,
  kM98 = 98,
  kM99 = 99,
  kM100 = 100,
};

// Returns estimated milestone dates as milliseconds since January 1, 1970.
base::Time::Exploded MilestoneDate(Milestone milestone) {
  // These are the Estimated Stable Dates:
  // https://www.chromium.org/developers/calendar
  // All dates except for kUnknown are at 04:00:00 GMT.
  switch (milestone) {
    case kUnknown:
      break;
    case kM61:
      return {2017, 9, 0, 5, 4};
    case kM62:
      return {2017, 10, 0, 17, 4};
    case kM64:
      return {2018, 1, 0, 23, 4};
    case kM65:
      return {2018, 3, 0, 6, 4};
    case kM70:
      return {2018, 10, 0, 16, 4};
    case kM71:
      return {2018, 12, 0, 4, 4};
    case kM72:
      return {2019, 1, 0, 29, 4};
    case kM75:
      return {2019, 6, 0, 4, 4};
    case kM76:
      return {2019, 7, 0, 30, 4};
    case kM77:
      return {2019, 9, 0, 10, 4};
    case kM78:
      return {2019, 10, 0, 22, 4};
    case kM79:
      return {2019, 12, 0, 10, 4};
    case kM80:
      return {2020, 2, 0, 4, 4};
    case kM81:
      return {2020, 4, 0, 7, 4};
    case kM82:
      // This release was cancelled, so this is the (new) M83 date.
      // https://groups.google.com/a/chromium.org/d/msg/chromium-dev/N1NxbSVOZas/ySlEKDKkBgAJ
      return {2020, 5, 0, 18, 4};
    case kM83:
      return {2020, 5, 0, 18, 4};
    case kM84:
      return {2020, 7, 0, 14, 4};
    case kM85:
      return {2020, 8, 0, 25, 4};
    case kM86:
      return {2020, 10, 0, 6, 4};
    case kM87:
      return {2020, 11, 0, 17, 4};
    case kM88:
      return {2021, 1, 0, 19, 4};
    case kM89:
      return {2021, 3, 0, 2, 4};
    case kM90:
      return {2021, 4, 0, 13, 4};
    case kM91:
      return {2021, 5, 0, 25, 4};
    case kM92:
      return {2021, 7, 0, 20, 4};
    case kM93:
      return {2021, 8, 0, 31, 4};
    case kM94:
      return {2021, 9, 0, 21, 4};
    case kM95:
      return {2021, 10, 0, 19, 4};
    case kM96:
      return {2022, 11, 0, 16, 4};
    case kM97:
      return {2022, 1, 0, 4, 4};
    case kM98:
      return {2022, 2, 0, 1, 4};
    case kM99:
      return {2022, 3, 0, 1, 4};
    case kM100:
      return {2022, 3, 0, 29, 4};
  }

  NOTREACHED();
  return {1970, 1, 0, 1, 0};
}

// Returns estimated milestone dates as human-readable strings.
const String MilestoneString(const Milestone milestone) {
  if (milestone == kUnknown)
    return String();
  base::Time::Exploded date = MilestoneDate(milestone);
  return String::Format("M%d, around %s %d", milestone,
                        WTF::kMonthFullName[date.month - 1], date.year);
}

class DeprecationInfo final {
 public:
  // Use this to inform developers of any `details` for the deprecation. Use
  // this format only if none of the ones below make sense.
  static const DeprecationInfo WithDetails(const String& id,
                                           const Milestone milestone,
                                           const String& details) {
    return DeprecationInfo(id, milestone, String(), String(), details, String(),
                           details);
  }

  // Use this to inform developers of any `details` for the deprecation with
  // info at `chrome_status_id`. Use this format only if none of the ones below
  // make sense.
  static const DeprecationInfo WithDetailsAndChromeStatusID(
      const String& id,
      const Milestone milestone,
      const String& details,
      const String& chrome_status_id) {
    return DeprecationInfo(
        id, milestone, String(), String(), details, chrome_status_id,
        String::Format(
            "%s See https://www.chromestatus.com/feature/%s for more details.",
            details.Ascii().c_str(), chrome_status_id.Ascii().c_str()));
  }

  // Use this to inform developers a deprecated `feature` has a `replacement`.
  static const DeprecationInfo WithFeatureAndReplacement(
      const String& id,
      const Milestone milestone,
      const String& feature,
      const String& replacement) {
    return DeprecationInfo(
        id, milestone, feature, replacement, String(), String(),
        String::Format("%s is deprecated. Please use %s instead.",
                       feature.Ascii().c_str(), replacement.Ascii().c_str()));
  }

  // Use this to inform developers a deprecated `feature` has info at
  // `chrome_status_id`.
  static const DeprecationInfo WithFeatureAndChromeStatusID(
      const String& id,
      const Milestone milestone,
      const String& feature,
      const String& chrome_status_id) {
    return DeprecationInfo(
        id, milestone, feature, String(), String(), chrome_status_id,
        String::Format(
            "%s is deprecated and will be removed in %s. See "
            "https://www.chromestatus.com/feature/%s for more details.",
            feature.Ascii().c_str(), MilestoneString(milestone).Ascii().c_str(),
            chrome_status_id.Ascii().c_str()));
  }

  // Use this to inform developers a deprecated `feature` has a `replacement`
  // and info at `chrome_status_id`.
  static const DeprecationInfo WithFeatureAndReplacementAndChromeStatusID(
      const String& id,
      const Milestone milestone,
      const String& feature,
      const String& replacement,
      const String& chrome_status_id) {
    return DeprecationInfo(
        id, milestone, feature, replacement, String(), chrome_status_id,
        String::Format(
            "%s is deprecated and will be removed in %s. Please use %s "
            "instead. See https://www.chromestatus.com/feature/%s for more "
            "details.",
            feature.Ascii().c_str(), MilestoneString(milestone).Ascii().c_str(),
            replacement.Ascii().c_str(), chrome_status_id.Ascii().c_str()));
  }

  const String id_;
  const Milestone milestone_;
  const String feature_;
  const String replacement_;
  const String details_;
  const String chrome_status_id_;
  const String message_;

 private:
  DeprecationInfo(const String& id,
                  const Milestone milestone,
                  const String& feature,
                  const String& replacement,
                  const String& details,
                  const String& chrome_status_id,
                  const String& message)
      : id_(id),
        milestone_(milestone),
        feature_(feature),
        replacement_(replacement),
        details_(details),
        chrome_status_id_(chrome_status_id),
        message_(message) {
    for (wtf_size_t i = 0; i < chrome_status_id_.length(); ++i) {
      DCHECK(IsASCIIDigit(chrome_status_id_[i]));
    }
  }
};

const DeprecationInfo GetDeprecationInfo(const WebFeature feature) {
  switch (feature) {
    // Quota
    case WebFeature::kPrefixedStorageInfo:
      return DeprecationInfo::WithFeatureAndReplacement(
          "PrefixedStorageInfo", kUnknown, "'window.webkitStorageInfo'",
          "'navigator.webkitTemporaryStorage' or "
          "'navigator.webkitPersistentStorage'");

    case WebFeature::kPrefixedVideoSupportsFullscreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          "PrefixedVideoSupportsFullscreen", kUnknown,
          "'HTMLVideoElement.webkitSupportsFullscreen'",
          "'Document.fullscreenEnabled'");

    case WebFeature::kPrefixedVideoDisplayingFullscreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          "PrefixedVideoDisplayingFullscreen", kUnknown,
          "'HTMLVideoElement.webkitDisplayingFullscreen'",
          "'Document.fullscreenElement'");

    case WebFeature::kPrefixedVideoEnterFullscreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          "PrefixedVideoEnterFullscreen", kUnknown,
          "'HTMLVideoElement.webkitEnterFullscreen()'",
          "'Element.requestFullscreen()'");

    case WebFeature::kPrefixedVideoExitFullscreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          "PrefixedVideoExitFullscreen", kUnknown,
          "'HTMLVideoElement.webkitExitFullscreen()'",
          "'Document.exitFullscreen()'");

    case WebFeature::kPrefixedVideoEnterFullScreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          "PrefixedVideoEnterFullScreen", kUnknown,
          "'HTMLVideoElement.webkitEnterFullScreen()'",
          "'Element.requestFullscreen()'");

    case WebFeature::kPrefixedVideoExitFullScreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          "PrefixedVideoExitFullScreen", kUnknown,
          "'HTMLVideoElement.webkitExitFullScreen()'",
          "'Document.exitFullscreen()'");

    case WebFeature::kPrefixedRequestAnimationFrame:
      return DeprecationInfo::WithDetails(
          "PrefixedRequestAnimationFrame", kUnknown,
          "'webkitRequestAnimationFrame' is vendor-specific. Please use "
          "the standard 'requestAnimationFrame' instead.");

    case WebFeature::kPrefixedCancelAnimationFrame:
      return DeprecationInfo::WithDetails(
          "PrefixedCancelAnimationFrame", kUnknown,
          "'webkitCancelAnimationFrame' is vendor-specific. Please use the "
          "standard 'cancelAnimationFrame' instead.");

    case WebFeature::kPictureSourceSrc:
      return DeprecationInfo::WithDetails(
          "PictureSourceSrc", kUnknown,
          "<source src> with a <picture> parent is invalid and therefore "
          "ignored. Please use <source srcset> instead.");

    case WebFeature::kXMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload:
      return DeprecationInfo::WithDetails(
          "XMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload", kUnknown,
          "Synchronous XMLHttpRequest on the main thread is deprecated because "
          "of its detrimental effects to the end user's experience. For more "
          "help, check https://xhr.spec.whatwg.org/.");

    case WebFeature::kPrefixedWindowURL:
      return DeprecationInfo::WithFeatureAndReplacement(
          "PrefixedWindowURL", kUnknown, "'webkitURL'", "'URL'");

    case WebFeature::kRangeExpand:
      return DeprecationInfo::WithFeatureAndReplacement(
          "RangeExpand", kUnknown, "'Range.expand()'", "'Selection.modify()'");

    // Blocked subresource requests:
    case WebFeature::kLegacyProtocolEmbeddedAsSubresource:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "LegacyProtocolEmbeddedAsSubresource", kUnknown,
          "Subresource requests using legacy protocols (like `ftp:`) are "
          "blocked. Please deliver web-accessible resources over modern "
          "protocols like HTTPS.",
          "5709390967472128");

    case WebFeature::kRequestedSubresourceWithEmbeddedCredentials:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "RequestedSubresourceWithEmbeddedCredentials", kUnknown,
          "Subresource requests whose URLs contain embedded credentials (e.g. "
          "`https://user:pass@host/`) are blocked.",
          "5669008342777856");

    // Powerful features on insecure origins (https://goo.gl/rStTGz)
    case WebFeature::kGeolocationInsecureOrigin:
    case WebFeature::kGeolocationInsecureOriginIframe:
      return DeprecationInfo::WithDetails(
          "GeolocationInsecureOrigin", kUnknown,
          "getCurrentPosition() and watchPosition() no longer work on insecure "
          "origins. To use this feature, you should consider switching your "
          "application to a secure origin, such as HTTPS. See "
          "https://goo.gl/rStTGz for more details.");

    case WebFeature::kGeolocationInsecureOriginDeprecatedNotRemoved:
    case WebFeature::kGeolocationInsecureOriginIframeDeprecatedNotRemoved:
      return DeprecationInfo::WithDetails(
          "GeolocationInsecureOriginDeprecatedNotRemoved", kUnknown,
          "getCurrentPosition() and watchPosition() are deprecated on insecure "
          "origins. To use this feature, you should consider switching your "
          "application to a secure origin, such as HTTPS. See "
          "https://goo.gl/rStTGz for more details.");

    case WebFeature::kGetUserMediaInsecureOrigin:
    case WebFeature::kGetUserMediaInsecureOriginIframe:
      return DeprecationInfo::WithDetails(
          "GetUserMediaInsecureOrigin", kUnknown,
          "getUserMedia() no longer works on insecure origins. To use this "
          "feature, you should consider switching your application to a secure "
          "origin, such as HTTPS. See https://goo.gl/rStTGz for more details.");

    case WebFeature::kMediaSourceAbortRemove:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "MediaSourceAbortRemove", kUnknown,
          "Using SourceBuffer.abort() to abort remove()'s asynchronous range "
          "removal is deprecated due to specification change. Support will be "
          "removed in the future. You should instead await 'updateend'. "
          "abort() is intended to only abort an asynchronous media append or "
          "reset parser state.",
          "6107495151960064");

    case WebFeature::kMediaSourceDurationTruncatingBuffered:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "MediaSourceDurationTruncatingBuffered", kUnknown,
          "Setting MediaSource.duration below the highest presentation "
          "timestamp of any buffered coded frames is deprecated due to "
          "specification change. Support for implicit removal of truncated "
          "buffered media will be removed in the future. You should instead "
          "perform explicit remove(newDuration, oldDuration) on all "
          "sourceBuffers, where newDuration < oldDuration.",
          "6107495151960064");

    case WebFeature::kApplicationCacheAPIInsecureOrigin:
    case WebFeature::kApplicationCacheManifestSelectInsecureOrigin:
      return DeprecationInfo::WithDetails(
          "ApplicationCacheAPIInsecureOrigin", kM70,
          "Application Cache was previously restricted to secure origins only "
          "from M70 on but now secure origin use is deprecated and will be "
          "removed in M82.  Please shift your use case over to Service "
          "Workers.");

    case WebFeature::kApplicationCacheAPISecureOrigin:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          "ApplicationCacheAPISecureOrigin", kM85, "Application Cache API use",
          "6192449487634432");

    case WebFeature::kApplicationCacheManifestSelectSecureOrigin:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          "ApplicationCacheAPISecureOrigin", kM85,
          "Application Cache API manifest selection", "6192449487634432");

    case WebFeature::kNotificationInsecureOrigin:
    case WebFeature::kNotificationAPIInsecureOriginIframe:
    case WebFeature::kNotificationPermissionRequestedInsecureOrigin:
      return DeprecationInfo::WithDetails(
          "NotificationInsecureOrigin", kUnknown,
          "The Notification API may no longer be used from insecure origins. "
          "You should consider switching your application to a secure origin, "
          "such as HTTPS. See https://goo.gl/rStTGz for more details.");

    case WebFeature::kNotificationPermissionRequestedIframe:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "NotificationPermissionRequestedIframe", kUnknown,
          "Permission for the Notification API may no longer be requested from "
          "a cross-origin iframe. You should consider requesting permission "
          "from a top-level frame or opening a new window instead.",
          "6451284559265792");

    case WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "CSSSelectorInternalMediaControlsOverlayCastButton", kUnknown,
          "The disableRemotePlayback attribute should be used in order to "
          "disable the default Cast integration instead of using "
          "-internal-media-controls-overlay-cast-button selector.",
          "5714245488476160");

    case WebFeature::kSelectionAddRangeIntersect:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "SelectionAddRangeIntersect", kUnknown,
          "The behavior that Selection.addRange() merges existing Range and "
          "the specified Range was removed.",
          "6680566019653632");

    case WebFeature::kRtcpMuxPolicyNegotiate:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          "RtcpMuxPolicyNegotiate", kM62, "The rtcpMuxPolicy option",
          "5654810086866944");

    case WebFeature::kCanRequestURLHTTPContainingNewline:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "CanRequestURLHTTPContainingNewline", kUnknown,
          "Resource requests whose URLs contained both removed whitespace "
          "(`\\n`, `\\r`, `\\t`) characters and less-than characters (`<`) are "
          "blocked. Please remove newlines and encode less-than characters "
          "from places like element attribute values in order to load these "
          "resources.",
          "5735596811091968");

    case WebFeature::kLocalCSSFileExtensionRejected:
      return DeprecationInfo::WithDetails(
          "LocalCSSFileExtensionRejected", kM64,
          "CSS cannot be loaded from `file:` URLs unless they end in a `.css` "
          "file extension.");

    case WebFeature::kChromeLoadTimesRequestTime:
    case WebFeature::kChromeLoadTimesStartLoadTime:
    case WebFeature::kChromeLoadTimesCommitLoadTime:
    case WebFeature::kChromeLoadTimesFinishDocumentLoadTime:
    case WebFeature::kChromeLoadTimesFinishLoadTime:
    case WebFeature::kChromeLoadTimesNavigationType:
    case WebFeature::kChromeLoadTimesConnectionInfo:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "ChromeLoadTimesConnectionInfo", kUnknown,
          "chrome.loadTimes() is deprecated, instead use standardized API: "
          "Navigation Timing 2.",
          "5637885046816768");

    case WebFeature::kChromeLoadTimesFirstPaintTime:
    case WebFeature::kChromeLoadTimesFirstPaintAfterLoadTime:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "ChromeLoadTimesFirstPaintAfterLoadTime", kUnknown,
          "chrome.loadTimes() is deprecated, instead use standardized API: "
          "Paint Timing.",
          "5637885046816768");

    case WebFeature::kChromeLoadTimesWasFetchedViaSpdy:
    case WebFeature::kChromeLoadTimesWasNpnNegotiated:
    case WebFeature::kChromeLoadTimesNpnNegotiatedProtocol:
    case WebFeature::kChromeLoadTimesWasAlternateProtocolAvailable:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "ChromeLoadTimesWasAlternateProtocolAvailable", kUnknown,
          "chrome.loadTimes() is deprecated, instead use standardized API: "
          "nextHopProtocol in Navigation Timing 2.",
          "5637885046816768");

    case WebFeature::kMediaElementSourceOnOfflineContext:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          "MediaElementAudioSourceNode", kM71,
          "Creating a MediaElementAudioSourceNode on an OfflineAudioContext",
          "5258622686724096");

    case WebFeature::kMediaStreamDestinationOnOfflineContext:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          "MediaStreamAudioDestinationNode", kM71,
          "Creating a MediaStreamAudioDestinationNode on an "
          "OfflineAudioContext",
          "5258622686724096");

    case WebFeature::kMediaStreamSourceOnOfflineContext:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          "MediaStreamAudioSourceNode", kM71,
          "Creating a MediaStreamAudioSourceNode on an OfflineAudioContext",
          "5258622686724096");

    case WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          "TextToSpeech_DisallowedByAutoplay", kM71,
          "speechSynthesis.speak() without user activation",
          "5687444770914304");

    case WebFeature::kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics:
      return DeprecationInfo::WithDetails(
          "RTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics", kM72,
          String::Format(
              "\"Complex\" Plan B SDP detected! Chrome will switch the default "
              "sdpSemantics in %s from 'plan-b' to the standardized "
              "'unified-plan' format and this peer connection is relying on "
              "the default sdpSemantics. This SDP is not compatible with "
              "Unified Plan and will be rejected by clients expecting Unified "
              "Plan. For more information about how to prepare for the switch, "
              "see https://webrtc.org/web-apis/chrome/unified-plan/.",
              MilestoneString(kM72).Ascii().c_str()));

    case WebFeature::kNoSysexWebMIDIWithoutPermission:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "NoSysexWebMIDIWithoutPermission", kM82,
          String::Format(
              "Web MIDI will ask a permission to use even if the sysex is not "
              "specified in the MIDIOptions since around %s.",
              MilestoneString(kM82).Ascii().c_str()),
          "5138066234671104");

    case WebFeature::kCustomCursorIntersectsViewport:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          "CustomCursorIntersectsViewport", kM75,
          "Custom cursors with size greater than 32x32 DIP intersecting native "
          "UI",
          "5825971391299584");

    case WebFeature::kXRSupportsSession:
      return DeprecationInfo::WithFeatureAndReplacement(
          "XRSupportsSession", kM80, "supportsSession()",
          "isSessionSupported() and check the resolved boolean value");

    case WebFeature::kObsoleteWebrtcTlsVersion:
      return DeprecationInfo::WithDetails(
          "ObsoleteWebRtcCipherSuite", kM81,
          String::Format("Your partner is negotiating an obsolete (D)TLS "
                         "version. Support for this will be removed in %s. "
                         "Please check with your partner to have this fixed.",
                         MilestoneString(kM81).Ascii().c_str()));

    case WebFeature::kCssStyleSheetReplaceWithImport:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          "CssStyleSheetReplaceWithImport", kM84,
          "Support for calls to CSSStyleSheet.replace() with stylesheet text "
          "that includes @import",
          "4735925877735424");

    case WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation:
      return DeprecationInfo::WithDetails(
          "SharedArrayBufferConstructedWithoutIsolation", kM92,
          String::Format("SharedArrayBuffer will require cross-origin "
                         "isolation as of %s. See "
                         "https://developer.chrome.com/blog/"
                         "enabling-shared-array-buffer/ for more details.",
                         MilestoneString(kM92).Ascii().c_str()));

    case WebFeature::kRTCConstraintEnableRtpDataChannelsFalse:
    case WebFeature::kRTCConstraintEnableRtpDataChannelsTrue:
      return DeprecationInfo::WithDetails(
          "RTP data channel", kM88,
          "RTP data channels are no longer supported. The \"RtpDataChannels\" "
          "constraint is currently ignored, and may cause an error at a later "
          "date.");

    case WebFeature::kRTCPeerConnectionSdpSemanticsPlanB:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "RTCPeerConnectionSdpSemanticsPlanB", kM93,
          "Plan B SDP semantics, which is used when constructing an "
          "RTCPeerConnection with {sdpSemantics:\"plan-b\"}, is a legacy "
          "version of the Session Description Protocol that has severe "
          "compatibility issues on modern browsers. The standardized SDP "
          "format, \"unified-plan\", has been used by default since M72 "
          "(January, 2019). Dropping support for Plan B is targeted for M93, "
          "but it's possible to register for a Deprecation Trial in order to "
          "extend the Plan B deprecation deadline for a limited amount of "
          "time.",
          "5823036655665152");

    case WebFeature::kRTCPeerConnectionSdpSemanticsPlanBWithReverseOriginTrial:
      return DeprecationInfo::WithDetails(
          "RTCPeerConnectionSdpSemanticsPlanBWithReverseOriginTrial", kM96,
          "Plan B SDP semantics, which is used when constructing an "
          "RTCPeerConnection with {sdpSemantics:\"plan-b\"}, is a legacy "
          "version of the Session Description Protocol that has severe "
          "compatibility issues on modern browsers. The standardized SDP "
          "format, \"unified-plan\", has been used by default since M72 "
          "(January, 2019). Dropping support for Plan B is targeted for M93, "
          "but this page may extend the deadline until the End Date of the "
          "'RTCPeerConnection Plan B SDP Semantics' deprecation trial.");

    case WebFeature::kAddressSpaceUnknownNonSecureContextEmbeddedPrivate:
    case WebFeature::kAddressSpaceUnknownNonSecureContextEmbeddedLocal:
    case WebFeature::kAddressSpacePublicNonSecureContextEmbeddedPrivate:
    case WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal:
    case WebFeature::kAddressSpacePrivateNonSecureContextEmbeddedLocal:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          "InsecurePrivateNetworkSubresourceRequest", kM92,
          "The website requested a subresource from a network that it could "
          "only access because of its users' privileged network position. "
          "These requests expose non-public devices and servers to the "
          "internet, increasing the risk of a cross-site request forgery "
          "(CSRF) attack, and/or information leakage. To mitigate these risks, "
          "Chrome deprecates requests to non-public subresources when "
          "initiated from non-secure contexts, and will start blocking them in "
          "Chrome 92 (July 2021).",
          "5436853517811712");
    case WebFeature::kXHRJSONEncodingDetection:
      return DeprecationInfo::WithDetails(
          "XHRJSONEncodingDetection", kM93,
          "UTF-16 is not supported by response json in XMLHttpRequest");

    case WebFeature::kAuthorizationCoveredByWildcard:
      return DeprecationInfo::WithDetails(
          "AuthorizationCoveredByWildcard", kM97,
          "\"Authorization\" will not be covered by the wildcard symbol (*)in "
          "CORS \"Access-Control-Allow-Headers\" handling.");

    case WebFeature::kOpenWebDatabaseThirdPartyContext:
      return DeprecationInfo::WithFeatureAndReplacementAndChromeStatusID(
          "OpenWebDatabaseThirdPartyContext", kM97,
          "WebSQL in third-party contexts (i.e. cross-site iframes)",
          "Web Storage or Indexed Database", "5684870116278272");

    case WebFeature::kRTCConstraintEnableDtlsSrtpTrue:
      return DeprecationInfo::WithDetails(
          "RTCConstraintEnableDtlsSrtpTrue", kM97,
          "The constraint \"DtlsSrtpKeyAgreement\" is removed. You have "
          "specified a \"true\" value for this constraint, which had no "
          "effect, but you can remove this constraint for tidiness.");

    case WebFeature::kRTCConstraintEnableDtlsSrtpFalse:
      return DeprecationInfo::WithDetails(
          "RTCConstraintEnableDtlsSrtpFalse", kM97,
          "The constraint \"DtlsSrtpKeyAgreement\" is removed. You have "
          "specified a \"false\" value for this constraint, which is "
          "interpreted as an attempt to use the removed \"SDES\" key "
          "negotiation method. This functionality is removed; use a service "
          "that supports DTLS key negotiation instead.");
    case WebFeature::kV8SharedArrayBufferConstructedInExtensionWithoutIsolation:
      return DeprecationInfo::WithDetails(
          "V8SharedArrayBufferConstructedInExtensionWithoutIsolation", kM96,
          "Extensions should opt into cross-origin isolation to continue using "
          "SharedArrayBuffer. See "
          "https://developer.chrome.com/docs/extensions/mv3/"
          "cross-origin-isolation/.");

    case WebFeature::kCrossOriginWindowAlert:
      return DeprecationInfo::WithDetails(
          "CrossOriginWindowAlert", kUnknown,
          "Triggering window.alert from cross origin iframes has been "
          "deprecated and will be removed in the future.");
    case WebFeature::kCrossOriginWindowPrompt:
      return DeprecationInfo::WithDetails(
          "CrossOriginWindowPrompt", kUnknown,
          "Triggering window.prompt from cross origin iframes has been "
          "deprecated and will be removed in the future.");
    case WebFeature::kCrossOriginWindowConfirm:
      return DeprecationInfo::WithDetails(
          "CrossOriginWindowConfirm", kUnknown,
          "Triggering window.confirm from cross origin iframes has been "
          "deprecated and will be removed in the future.");

    case WebFeature::kPaymentRequestBasicCard:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          "PaymentRequestBasicCard", kM100, "The 'basic-card' payment method",
          "5730051011117056");

    case WebFeature::kHostCandidateAttributeGetter:
      return DeprecationInfo::WithFeatureAndReplacement(
          "HostCandidateAttributeGetter", kUnknown,
          "'RTCPeerConnectionIceErrorEvent.hostCandidate'",
          "'RTCPeerConnectionIceErrorEvent.address', "
          "'RTCPeerConnectionIceErrorEvent.port'");

    case WebFeature::kWebCodecsVideoFrameDefaultTimestamp:
      return DeprecationInfo::WithDetails(
          "WebCodecsVideoFrameDefaultTimestamp", kUnknown,
          "A VideoFrame was constructed without a timestamp. Support for this  "
          "may be removed in the future. Please provide an explicit timestamp "
          "via VideoFrameInit.");

    // Features that aren't deprecated don't have a deprecation message.
    default:
      return DeprecationInfo::WithDetails("NotDeprecated", kUnknown, String());
  }
}

Report* CreateReportInternal(const KURL& context_url,
                             const DeprecationInfo& info) {
  absl::optional<base::Time> optional_removal_date;
  if (info.milestone_ != kUnknown) {
    base::Time removal_date;
    bool result = base::Time::FromUTCExploded(MilestoneDate(info.milestone_),
                                              &removal_date);
    DCHECK(result);
    optional_removal_date = removal_date;
  }
  DeprecationReportBody* body = MakeGarbageCollected<DeprecationReportBody>(
      info.id_, optional_removal_date, info.message_);
  return MakeGarbageCollected<Report>(ReportType::kDeprecation, context_url,
                                      body);
}

}  // anonymous namespace

Deprecation::Deprecation() : mute_count_(0) {}

void Deprecation::ClearSuppression() {
  features_deprecation_bits_.reset();
}

void Deprecation::MuteForInspector() {
  mute_count_++;
}

void Deprecation::UnmuteForInspector() {
  mute_count_--;
}

void Deprecation::SetReported(WebFeature feature) {
  features_deprecation_bits_.set(static_cast<size_t>(feature));
}

bool Deprecation::GetReported(WebFeature feature) const {
  return features_deprecation_bits_[static_cast<size_t>(feature)];
}

void Deprecation::CountDeprecationCrossOriginIframe(LocalDOMWindow* window,
                                                    WebFeature feature) {
  DCHECK(window);
  if (!window->GetFrame())
    return;

  // Check to see if the frame can script into the top level context.
  Frame& top = window->GetFrame()->Tree().Top();
  if (!window->GetSecurityOrigin()->CanAccess(
          top.GetSecurityContext()->GetSecurityOrigin())) {
    CountDeprecation(window, feature);
  }
}

void Deprecation::CountDeprecation(ExecutionContext* context,
                                   WebFeature feature) {
  if (!context)
    return;

  Deprecation* deprecation = nullptr;
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    if (window->GetFrame())
      deprecation = &window->GetFrame()->GetPage()->GetDeprecation();
  } else if (auto* scope = DynamicTo<WorkerOrWorkletGlobalScope>(context)) {
    // TODO(crbug.com/1146824): Remove this once PlzDedicatedWorker and
    // PlzServiceWorker ship.
    if (!scope->IsInitialized()) {
      return;
    }
    deprecation = &scope->GetDeprecation();
  }

  if (!deprecation || deprecation->mute_count_ ||
      deprecation->GetReported(feature)) {
    return;
  }
  deprecation->SetReported(feature);
  context->CountUse(feature);
  const DeprecationInfo info = GetDeprecationInfo(feature);

  // Send the deprecation message to the console as a warning.
  DCHECK(!info.message_.IsEmpty());
  if (base::FeatureList::IsEnabled(features::kDeprecationWillLogToConsole)) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kDeprecation,
        mojom::blink::ConsoleMessageLevel::kWarning, info.message_);
    context->AddConsoleMessage(console_message);
  }
  if (base::FeatureList::IsEnabled(
          features::kDeprecationWillLogToDevToolsIssue)) {
    AuditsIssue::ReportDeprecationIssue(context, info.message_);
  }

  Report* report = CreateReportInternal(context->Url(), info);

  // Send the deprecation report to the Reporting API and any
  // ReportingObservers.
  ReportingContext::From(context)->QueueReport(report);
}

// static
String Deprecation::DeprecationMessage(WebFeature feature) {
  return GetDeprecationInfo(feature).message_;
}

}  // namespace blink
