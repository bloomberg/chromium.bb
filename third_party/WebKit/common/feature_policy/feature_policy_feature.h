// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_COMMON_FEATURE_POLICY_FEATURE_POLICY_FEATURE_H_
#define THIRD_PARTY_WEBKIT_COMMON_FEATURE_POLICY_FEATURE_POLICY_FEATURE_H_

namespace blink {

// These values map to the features which can be controlled by Feature Policy.
//
// Features are defined in
// https://github.com/WICG/feature-policy/blob/gh-pages/features.md. Many of
// these are still under development in blink behind the
// featurePolicyExperimentalFeatures flag.
enum class FeaturePolicyFeature {
  kNotFound = 0,
  // Controls access to media autoplay.
  kAutoplay,
  // Controls access to video input devices.
  kCamera,
  // Controls whether navigator.requestMediaKeySystemAccess is allowed.
  kEncryptedMedia,
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
  // Controls whether synchronous script elements will run.
  kSyncScript,
  // Controls use of synchronous XMLHTTPRequest API.
  kSyncXHR,
  // Controls access to the WebUSB API.
  kUsb,
  // Controls access to AOM event listeners.
  kAccessibilityEvents,
  // Controls use of WebVR API.
  kWebVr,
  // The following features control access to the corresponding sensor classes.
  // Fusion sensor APIs (e.g. LinearAcceleration, OrientationSensor-based
  // classes)require all of the features that are inputs into that API to be
  // enabled for the feature to be allowed.
  kAccelerometer,
  kAmbientLightSensor,
  kGyroscope,
  kMagnetometer,
  LAST_FEATURE = kMagnetometer
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_COMMON_FEATURE_POLICY_FEATURE_POLICY_FEATURE_H_
