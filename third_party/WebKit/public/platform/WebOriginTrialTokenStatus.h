// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebOriginTrialTokenStatus_h
#define WebOriginTrialTokenStatus_h

namespace blink {

// The enum entries below are written to histograms and thus cannot be deleted
// or reordered.
// New entries must be added immediately before the end.
enum class WebOriginTrialTokenStatus {
  kSuccess = 0,
  kNotSupported = 1,
  kInsecure = 2,
  kExpired = 3,
  kWrongOrigin = 4,
  kInvalidSignature = 5,
  kMalformed = 6,
  kWrongVersion = 7,
  kFeatureDisabled = 8,
  kTokenDisabled = 9,
  kLast = kTokenDisabled
};

}  // namespace blink

#endif  // WebOriginTrialTokenStatus_h
