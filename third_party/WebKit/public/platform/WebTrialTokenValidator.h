// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TRIAL_TOKEN_VALIDATOR_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TRIAL_TOKEN_VALIDATOR_H_

#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

enum class OriginTrialTokenStatus;

// This interface abstracts the task of validating a token for an experimental
// feature. Experimental features can be turned on and off at runtime for a
// specific renderer, depending on the presence of a valid token provided by
// the origin.
//
// For more information, see https://github.com/jpchase/OriginTrials.

class WebTrialTokenValidator {
 public:
  virtual ~WebTrialTokenValidator() = default;

  // Returns whether the given token is valid for the specified origin. If the
  // token is valid, it also returns the feature the token is valid for in
  // |*feature_name|.
  virtual OriginTrialTokenStatus ValidateToken(const WebString& token,
                                               const WebSecurityOrigin&,
                                               WebString* feature_name) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TRIAL_TOKEN_VALIDATOR_H_
