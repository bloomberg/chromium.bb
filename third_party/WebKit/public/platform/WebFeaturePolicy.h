// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFeaturePolicy_h
#define WebFeaturePolicy_h

#include "WebCommon.h"
#include "WebSecurityOrigin.h"
#include "WebString.h"
#include "WebVector.h"

namespace blink {

// These values map to the features which can be controlled by Feature Policy.
//
// Features are defined in
// https://wicg.github.io/feature-policy/#defined-features. Many of these are
// still under development in blink behind the featurePolicyExperimentalFeatures
// flag, see getWebFeaturePolicyFeature().
enum class WebFeaturePolicyFeature {
  kNotFound = 0,
  // Controls access to video input devices.
  kCamera,
  // Controls whether navigator.requestMediaKeySystemAccess is allowed.
  kEme,
  // Controls whether Element.requestFullscreen is allowed.
  kFullscreen,
  // Controls access to Geolocation interface.
  kGeolocation,
  // Controls access to audio input devices.
  kMicrophone,
  // Controls access to requestMIDIAccess method.
  kMidiFeature,
  // Controls access to PaymentRequest interface.
  kPayment,
  // Controls access to audio output devices.
  kSpeaker,
  // Controls access to navigator.vibrate method.
  kVibrate,
  // Controls access to document.cookie attribute.
  kDocumentCookie,
  // Contols access to document.domain attribute.
  kDocumentDomain,
  // Controls access to document.write and document.writeln methods.
  kDocumentWrite,
  // Controls access to Notification interface.
  kNotifications,
  // Controls access to PushManager interface.
  kPush,
  // Controls whether synchronous script elements will run.
  kSyncScript,
  // Controls use of synchronous XMLHTTPRequest API.
  kSyncXHR,
  // Controls access to RTCPeerConnection interface.
  kWebRTC,
  LAST_FEATURE = kWebRTC
};

struct BLINK_PLATFORM_EXPORT WebParsedFeaturePolicyDeclaration {
  WebParsedFeaturePolicyDeclaration() : matches_all_origins(false) {}
  WebFeaturePolicyFeature feature;
  bool matches_all_origins;
  WebVector<WebSecurityOrigin> origins;
};

// Used in Blink code to represent parsed headers. Used for IPC between renderer
// and browser.
using WebParsedFeaturePolicy = WebVector<WebParsedFeaturePolicyDeclaration>;

// Composed full policy for a document. Stored in SecurityContext for each
// document. This is essentially an opaque handle to an object in the embedder.
class BLINK_PLATFORM_EXPORT WebFeaturePolicy {
 public:
  virtual ~WebFeaturePolicy() {}

  // Returns whether or not the given feature is enabled for the origin of the
  // document that owns the policy.
  virtual bool IsFeatureEnabled(blink::WebFeaturePolicyFeature) const = 0;
};

}  // namespace blink

#endif  // WebFeaturePolicy_h
